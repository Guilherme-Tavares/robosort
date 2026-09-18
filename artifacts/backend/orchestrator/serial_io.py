"""Ligacao serial com o robosort-firmware.

Contrato do firmware: cada comando recebe exatamente uma resposta terminal,
OK ou ERR <motivo>, na ordem de envio. Linhas STATE, CORNER, APPROACH e '#'
que precedem o terminal pertencem ao comando. DET <zona> e PUSHED <zona> sao
assincronas: o firmware empurra sozinho na deteccao e avisa.

Uma thread le a serial continuamente e roteia cada linha: DET e PUSHED vao
para a fila de eventos, como (tipo, zona); o resto vai para a resposta
pendente mais antiga. Comandos
normais sao serializados por um lock; stop, off e offall furam o lock,
porque existem para interromper um comando em curso. Como o firmware
responde ao interrompido antes do interruptor, a fila de pendentes pareia
as duas respostas na ordem certa.

Uso direto, para testar:
    python serial_io.py [--port COMx]      REPL sobre a serial
"""

import re
import sys
import threading
import time
from collections import deque
from dataclasses import dataclass, field
from queue import Empty, Queue

import serial
from serial.tools import list_ports

import config

JOINTS = ("base", "garra", "altura", "alcance")
INTERRUPTS = ("stop", "offall", "off")
EVENTS = ("DET", "PUSHED")

STATE_RE = re.compile(r"STATE (\w+) (\?|\d+) (\w+) (\d+) (\d+) (\d+) (\d+)$")
CORNER_RE = re.compile(r"CORNER (\d) (\d+) (\d+) (\d+)$")
APPROACH_RE = re.compile(r"APPROACH (\d+) (\d+)$")
DROP_RE = re.compile(r"DROP (\d+) (\d+) (\d+)$")
GRIPPER_RE = re.compile(r"GRIPPER (\d+) (\d+)$")


class ArduinoError(RuntimeError):
    pass


class CommandError(ArduinoError):
    """O firmware respondeu ERR."""

    def __init__(self, cmd, reason):
        super().__init__(f"'{cmd}': ERR {reason}")
        self.cmd = cmd
        self.reason = reason


class AckTimeout(ArduinoError):
    """Sem resposta terminal dentro do prazo. A ligacao fica dessincronizada
    ate resync()."""


class Disconnected(ArduinoError):
    pass


@dataclass
class Response:
    cmd: str
    lines: list = field(default_factory=list)
    ok: bool = None
    error: str = None
    done: threading.Event = field(default_factory=threading.Event)


@dataclass
class JointInfo:
    angle: int          # None se solta
    state: str          # solta | declarada | energizada
    min: int
    max: int
    home: int
    delivery: int


@dataclass
class ArmConfig:
    joints: dict        # nome -> JointInfo
    gripper: dict       # {"open", "closed"}: angulos proprios, nao os limites
    corners: dict       # k -> {"base", "altura", "alcance"}
    approach: dict      # {"altura", "alcance"}
    drop: dict          # {"base", "alcance", "altura"}: soltura, apos DELIVERY

    def limits(self, joint):
        j = self.joints[joint]
        return j.min, j.max


def find_port(hint=None):
    """Porta do Arduino. Ignora as portas Bluetooth do Windows."""
    if hint:
        return hint
    for port in list_ports.comports():
        text = f"{port.description} {port.manufacturer or ''}".lower()
        if "bluetooth" in text:
            continue
        if any(k in text for k in ("arduino", "ch340", "usb-serial", "usb serial")):
            return port.device
    return None


class Arduino:
    """Canal de comandos para o firmware. Use como context manager."""

    def __init__(self, port=None, echo=False):
        self.port_hint = port
        self.echo = echo
        self.serial = None
        self.events = Queue()            # (tipo, zona): ("DET", z) e ("PUSHED", z)

        self._pending = deque()
        self._pending_lock = threading.Lock()
        self._cmd_lock = threading.Lock()
        self._write_lock = threading.Lock()
        self._reader = None
        self._stop = threading.Event()
        self._boot = None                # primeira linha do firmware apos o reset
        self._booted = threading.Event()
        self._last_line_at = 0.0
        self._desynced = False
        self._disconnected = False

    # ------------------------------------------------------------ conexao

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *exc):
        self.close()

    def open(self):
        port = find_port(self.port_hint)
        if port is None:
            raise ArduinoError("Arduino nao encontrado. Conecte o USB ou passe --port COMx.")
        try:
            self.serial = serial.Serial(port, config.BAUD, timeout=0.05)
        except serial.SerialException as exc:
            if isinstance(exc.__context__, PermissionError) or "Access is denied" in str(exc):
                raise ArduinoError(f"{port} em uso por outro programa (Monitor Serial do "
                                   f"Arduino IDE, outro console?). Feche-o e tente de novo.") from exc
            raise ArduinoError(f"nao abriu {port}: {exc}") from exc
        self._stop.clear()
        self._reader = threading.Thread(target=self._read_loop, name="serial-reader", daemon=True)
        self._reader.start()

        # Abrir a porta reseta o Uno; o firmware anuncia READY ao fim do setup.
        if not self._booted.wait(config.BOOT_TIMEOUT):
            self.close()
            raise ArduinoError(f"firmware nao respondeu em {config.BOOT_TIMEOUT:.0f} s na porta {port}")
        if self._boot != "READY":
            self.close()
            raise ArduinoError(f"firmware sem PCA9685 ({self._boot}); confira I2C, VCC e GND")

    def close(self):
        self._stop.set()
        if self._reader and self._reader.is_alive() and threading.current_thread() is not self._reader:
            self._reader.join(timeout=1.0)
        if self.serial and self.serial.is_open:
            self.serial.close()
        self.serial = None

    @property
    def port(self):
        return self.serial.port if self.serial else None

    # -------------------------------------------------------------- leitor

    def _read_loop(self):
        while not self._stop.is_set():
            try:
                raw = self.serial.readline()
            except (serial.SerialException, OSError):
                self._disconnected = True
                self._fail_all("serial desconectada")
                return
            if not raw:
                continue
            line = raw.decode("ascii", errors="replace").strip()
            if not line:
                continue
            self._last_line_at = time.monotonic()
            if self.echo:
                print(f"  <- {line}")
            self._route(line)

    def _route(self, line):
        for kind in EVENTS:
            if line.startswith(kind + " "):
                self.events.put((kind, line[len(kind) + 1:].strip()))
                return

        if not self._booted.is_set():
            # Antes de qualquer comando so pode vir o banner: READY ou ERR pca.
            if line == "READY" or line.startswith("ERR"):
                self._boot = line
                self._booted.set()
            return

        with self._pending_lock:
            resp = self._pending[0] if self._pending else None
            terminal = line == "OK" or line.startswith("ERR")
            if resp is not None and terminal:
                self._pending.popleft()

        if resp is None:
            return                          # resposta atrasada ou banner repetido: ignora
        if line == "OK":
            resp.ok = True
            resp.done.set()
        elif line.startswith("ERR"):
            resp.ok = False
            resp.error = line[3:].strip()
            resp.done.set()
        else:
            resp.lines.append(line)         # STATE, CORNER, APPROACH, '#'

    def _fail_all(self, why):
        with self._pending_lock:
            pending = list(self._pending)
            self._pending.clear()
        for resp in pending:
            resp.ok = False
            resp.error = why
            resp.done.set()

    # ------------------------------------------------------------ comandos

    def _write(self, line):
        if self.echo:
            print(f"  -> {line}")
        with self._write_lock:
            try:
                self.serial.write((line + "\n").encode("ascii"))
                self.serial.flush()
            except (serial.SerialException, OSError) as exc:
                self._disconnected = True
                raise Disconnected(str(exc)) from exc

    def command(self, line, timeout=None):
        """Envia uma linha e espera a resposta terminal. Devolve as linhas
        intermediarias (STATE, CORNER, ...). Levanta CommandError em ERR."""
        if self._disconnected:
            raise Disconnected("serial desconectada")
        if self._desynced:
            raise AckTimeout("ligacao dessincronizada; chame resync()")

        word = line.split(" ", 1)[0]
        if timeout is None:
            timeout = config.ACK_TIMEOUT
        interrupt = word in INTERRUPTS
        lock = self._cmd_lock if not interrupt else None

        if lock:
            lock.acquire()
        try:
            resp = Response(line)
            with self._pending_lock:
                self._pending.append(resp)
            self._write(line)
            if not resp.done.wait(timeout):
                self._desynced = True
                raise AckTimeout(f"'{line}': sem resposta em {timeout:.0f} s")
        finally:
            if lock:
                lock.release()

        if not resp.ok:
            raise CommandError(line, resp.error)
        return resp.lines

    def resync(self):
        """Depois de um AckTimeout: descarta pendentes, espera a serial
        silenciar e confirma com ping."""
        self._fail_all("descartado no resync")
        while time.monotonic() - self._last_line_at < config.RESYNC_QUIET:
            time.sleep(0.05)
        self._desynced = False
        self.ping()

    # Atalhos. Sequencias (mv home/dest/area, push) usam o timeout longo.

    def ping(self):                   return self.command("ping")
    def home(self):                   return self.command("home")
    def dest(self):                   return self.command("dest")
    def dump(self):                   return self.command("dump")
    def corners(self):                return self.command("corners")
    def set(self, joint, angle):      return self.command(f"set {joint} {angle}")
    def mv(self, joint, angle):       return self.command(f"mv {joint} {angle}")
    def mv_home(self):                return self.command("mv home", config.SEQUENCE_TIMEOUT)
    def mv_dest(self):                return self.command("mv dest", config.SEQUENCE_TIMEOUT)
    def mv_area(self, k):             return self.command(f"mv area {k}", config.SEQUENCE_TIMEOUT)
    def off(self, joint):             return self.command(f"off {joint}")
    def offall(self):                 return self.command("offall")
    def stop(self):                   return self.command("stop")
    def prep(self, zone, direction):  return self.command(f"prep {zone} {direction}")
    def arm(self, zone, direction):   return self.command(f"arm {zone} {direction}")
    def push(self, zone, direction):  return self.command(f"push {zone} {direction}")
    def rest(self, zone):             return self.command(f"rest {zone}")
    def disarm(self, zone):           return self.command(f"disarm {zone}")

    def drain_events(self):
        while not self.events.empty():
            self.events.get_nowait()

    def wait_event(self, kind, zone, timeout):
        """Espera (kind, zone) na fila, descartando outros eventos. None se
        o prazo estourar."""
        deadline = time.monotonic() + timeout
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                return None
            try:
                ev = self.events.get(timeout=left)
            except Empty:
                return None
            if ev == (kind, zone):
                return ev

    # ------------------------------------------------------- configuracao

    def read_config(self):
        """Le do firmware tudo que o orquestrador precisa saber do braco."""
        joints, gripper = {}, None
        for line in self.dump():
            m = STATE_RE.match(line)
            if m:
                name, angle, state, lo, hi, home, delivery = m.groups()
                joints[name] = JointInfo(
                    angle=None if angle == "?" else int(angle),
                    state=state, min=int(lo), max=int(hi),
                    home=int(home), delivery=int(delivery),
                )
                continue
            m = GRIPPER_RE.match(line)
            if m:
                gripper = {"open": int(m.group(1)), "closed": int(m.group(2))}
        missing = [j for j in JOINTS if j not in joints]
        if missing:
            raise ArduinoError(f"'dump' nao trouxe: {', '.join(missing)}")
        if gripper is None:
            raise ArduinoError("'dump' nao trouxe GRIPPER (aberta fechada)")

        corners, approach, drop = {}, None, None
        for line in self.corners():
            m = CORNER_RE.match(line)
            if m:
                k, base, altura, alcance = map(int, m.groups())
                corners[k] = {"base": base, "altura": altura, "alcance": alcance}
                continue
            m = APPROACH_RE.match(line)
            if m:
                approach = {"altura": int(m.group(1)), "alcance": int(m.group(2))}
                continue
            m = DROP_RE.match(line)
            if m:
                base, alcance, altura = map(int, m.groups())
                drop = {"base": base, "alcance": alcance, "altura": altura}
        if sorted(corners) != [0, 1, 2, 3] or approach is None or drop is None:
            raise ArduinoError("'corners' incompleto: esperados CORNER 0-3, APPROACH e DROP")

        return ArmConfig(joints=joints, gripper=gripper, corners=corners,
                         approach=approach, drop=drop)


# ------------------------------------------------------------------ REPL

def repl(port=None):
    """Terminal sobre a serial, com DET assincrono impresso quando chegar."""
    with Arduino(port, echo=True) as link:
        print(f"  conectado em {link.port}. Ctrl+C sai; 'help' lista os comandos.")

        def watch_events():
            while True:
                kind, zone = link.events.get()
                print(f"  ** {kind} {zone}")

        threading.Thread(target=watch_events, daemon=True).start()
        while True:
            try:
                line = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                return
            if not line:
                continue
            try:
                timeout = config.SEQUENCE_TIMEOUT if line.startswith(("mv home", "mv dest", "mv area")) else None
                link.command(line, timeout)
            except CommandError:
                pass                            # ja ecoado como '<- ERR ...'
            except AckTimeout as exc:
                print(f"  !! {exc}; tentando resync")
                link.resync()


if __name__ == "__main__":
    port = sys.argv[sys.argv.index("--port") + 1] if "--port" in sys.argv else None
    try:
        repl(port)
    except ArduinoError as exc:
        print(f"!! {exc}")
        sys.exit(1)
