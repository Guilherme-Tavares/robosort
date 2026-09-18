"""Ligacao serial com o firmware de calibracao do braco.

Encapsula o protocolo de linha do sketch (home, mv, offall) e a espera pela
confirmacao de cada comando. Nao conhece o controle: quem manda e o main.
"""

import re
import time

import serial
from serial.tools import list_ports

BAUD = 115200

# O Uno reseta quando a porta serial e aberta (pulso de DTR). Antes desse
# tempo ele ainda esta no bootloader e perde qualquer coisa enviada.
BOOT_WAIT = 2.0

ACK_TIMEOUT = 2.0        # tempo maximo esperando a resposta de um comando
IDLE_READ = 0.05         # timeout de leitura de uma linha

# Nomes das juntas como o firmware os conhece.
BASE = "base"
HEIGHT = "altura"
REACH = "alcance"
GRIPPER = "garra"
JOINTS = (BASE, HEIGHT, REACH, GRIPPER)

# ">> altura em 93"  (movimento concluido)
ACK_MOVED = re.compile(r">>\s+(\w+)\s+em\s+(-?\d+)")
# ">> altura energizada em 91"  (primeira energizacao)
ACK_LIVE = re.compile(r">>\s+(\w+)\s+energizada em\s+(-?\d+)")
# "base   98   declarada   18..178"  (linha de junta do 'dump')
DUMP_ROW = re.compile(r"(\w+)\s+(\?|-?\d+)\s+\S+\s+(\?|-?\d+)\.\.(\?|-?\d+)$")


class ArmError(RuntimeError):
    pass


def find_port(hint=None):
    """Descobre a porta do Arduino. Ignora as portas Bluetooth do Windows."""
    if hint:
        return hint
    for port in list_ports.comports():
        text = f"{port.description} {port.manufacturer or ''}".lower()
        if "bluetooth" in text:
            continue
        if port.vid in (0x2341, 0x2A03, 0x1A86):   # Arduino, Arduino.org, CH340
            return port.device
        if any(k in text for k in ("arduino", "ch340", "usb-serial", "usb serial")):
            return port.device
    return None


class ArmLink:
    """Canal de comandos para o firmware. Use como context manager."""

    def __init__(self, port=None, echo=True):
        self.port_hint = port
        self.echo = echo
        self.serial = None

    # ------------------------------------------------------------ conexao

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *exc):
        self.close()

    def open(self):
        port = find_port(self.port_hint)
        if port is None:
            raise ArmError(
                "Arduino nao encontrado. Conecte o cabo USB ou passe --port COMx."
            )

        self.serial = serial.Serial(port, BAUD, timeout=IDLE_READ)
        print(f"  porta: {port}")

        # Espera o reset e descarta o menu de ajuda que o firmware imprime.
        # Se o PCA9685 nao respondeu no boot, o aviso vem junto: melhor
        # falhar aqui do que no primeiro 'mv', ao apertar Options.
        time.sleep(BOOT_WAIT)
        banner = self.drain()
        for line in banner:
            if line.startswith("!!") and "PCA9685" in line:
                self.close()
                raise ArmError(
                    "firmware sem PCA9685: confira SDA/SCL (A4/A5), VCC do "
                    "modulo e GND comum, e reconecte."
                )

    def close(self):
        if self.serial and self.serial.is_open:
            self.serial.close()
        self.serial = None

    # -------------------------------------------------------------- baixo

    def _write(self, line):
        if self.echo:
            print(f"  -> {line}")
        self.serial.write((line + "\n").encode("ascii"))
        self.serial.flush()

    def _readline(self):
        raw = self.serial.readline()
        if not raw:
            return None
        return raw.decode("ascii", errors="replace").strip()

    def drain(self, window=0.3):
        """Consome o que estiver pendente na serial e devolve as linhas."""
        lines = []
        deadline = time.monotonic() + window
        while time.monotonic() < deadline:
            line = self._readline()
            if line:
                lines.append(line)
                deadline = time.monotonic() + window
        return lines

    # ---------------------------------------------------------- comandos

    def home(self):
        """Declara as quatro juntas nos centros. Nao energiza, nao move."""
        self._write("home")
        self.drain()

    def read_config(self):
        """Le centros e limites do firmware. Devolve {junta: (centro, min, max)}.

        A tabela de calibracao vive no sketch; aqui nao ha copia. 'home'
        declara os centros e 'dump' os mostra junto com os limites.
        """
        self.home()
        self._write("dump")
        config = {}
        for line in self.drain():
            row = DUMP_ROW.match(line)
            if not row:
                continue
            joint, center, low, high = row.groups()
            if "?" in (center, low, high):
                raise ArmError(
                    f"firmware sem centro ou limites para '{joint}'; "
                    "preencha centers/knownMin/knownMax no sketch."
                )
            config[joint] = (int(center), int(low), int(high))

        missing = [j for j in JOINTS if j not in config]
        if missing:
            raise ArmError(f"'dump' nao trouxe: {', '.join(missing)}")
        return config

    def release_all(self):
        self._write("offall")
        self.drain()

    def move(self, joint, angle, expect_motion=True):
        """Envia 'mv' e espera a confirmacao.

        expect_motion=False quando o destino e igual ao angulo ja conhecido:
        nesse caso o firmware escreve no servo e retorna sem sinalizar fim de
        movimento, entao a unica resposta e a linha de energizacao.
        """
        self._write(f"mv {joint} {angle}")

        deadline = time.monotonic() + ACK_TIMEOUT
        energized = False

        while time.monotonic() < deadline:
            line = self._readline()
            if line is None:
                # Sem movimento previsto, a energizacao ja encerra o comando.
                if energized and not expect_motion:
                    return angle
                continue

            if self.echo:
                print(f"  <- {line}")

            if line.startswith("!!"):
                raise ArmError(line)

            done = ACK_MOVED.match(line)
            if done and done.group(1) == joint:
                return int(done.group(2))

            live = ACK_LIVE.match(line)
            if live and live.group(1) == joint:
                energized = True
                if not expect_motion:
                    return int(live.group(2))

        raise ArmError(f"sem resposta para 'mv {joint} {angle}'")
