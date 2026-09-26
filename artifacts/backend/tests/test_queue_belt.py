"""Fila de ciclos e esteira automatica, com camera e esteira falsas.

Cobre: uma operacao por ID mesmo lido em varios frames; menor ID quando ha
mais de uma caixinha; reuso de um ID ja operado; esteira liga ao ver
caixinha, fica entre ciclos seguidos e desliga apos CONVEYOR_IDLE_STOP.
"""
from pathlib import Path
import sys, threading, time, queue
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "orchestrator"))
import serial_io, config, orchestrator

# Este teste cobre fila e esteira, nao roteamento: fixa a fonte no mock para
# nao depender do ROUTE_SOURCE do projeto nem do conteudo do banco.
config.ROUTE_SOURCE = "mock"
from vision import VisionError

STATE = ["STATE base 98 declarada 18 178 18 96", "STATE garra 82 declarada 82 120 82 82",
         "STATE altura 91 declarada 16 136 91 110", "STATE alcance 96 declarada 36 176 96 84",
         "STATE norte 0 energizada 0 180 0 0", "STATE nordeste 0 energizada 0 180 0 0",
         "STATE centro-oeste 0 energizada 0 180 0 0", "STATE sudeste 0 energizada 0 180 0 0",
         "STATE sul 0 energizada 0 180 0 0"]
CORNERS = ["CORNER 0 18 22 62", "CORNER 1 40 37 57", "CORNER 2 40 22 60", "CORNER 3 37 31 58",
           "APPROACH 39 78", "DROP 96 74"]


class Fake:
    """Firmware: mv responde rapido; arm agenda DET+PUSHED."""
    def __init__(self, port, baud, timeout):
        self.port, self.is_open, self.out, self.sent = port, True, queue.Queue(), []
    def readline(self):
        try: return self.out.get(timeout=0.01)
        except queue.Empty: return b""
    def _say(self, s): self.out.put((s + "\n").encode())
    def write(self, data):
        cmd = data.decode().strip(); self.sent.append(cmd); w = cmd.split()[0]
        if w == "dump":
            for s in STATE: self._say(s)
            self._say("GRIPPER 120 82"); self._say("OK")
        elif w == "corners":
            for s in CORNERS: self._say(s)
            self._say("OK")
        elif w == "home":
            for s in STATE: self._say(s)
            self._say("OK")
        elif w == "arm":
            zone = cmd.split()[1]; self._say("OK")
            threading.Timer(0.05, lambda: (self._say(f"DET {zone}"),
                                           self._say(f"PUSHED {zone}"))).start()
        elif w == "mv":
            threading.Timer(0.01, lambda: self._say("OK")).start()
        else: self._say("OK")
    def flush(self): pass
    def close(self): self.is_open = False


class FakeVision:
    """Cena controlada: self.cena e o conjunto de IDs visiveis."""
    def __init__(self): self.cena, self.log = set(), lambda *a: None
    def peek(self): return min(self.cena, default=None)
    def identify(self):
        if not self.cena: raise VisionError("nenhum marcador de produto visivel")
        return min(self.cena)          # mesma regra do vision.py real


class FakeBelt:
    def __init__(self): self.running, self.speed, self.starts, self.stops = False, 10, 0, 0
    def start(self): self.running = True; self.starts += 1
    def stop(self): self.running = False; self.stops += 1
    def set_speed(self, s): self.speed = s


serial_io.serial.Serial = Fake
serial_io.find_port = lambda h=None: "FAKE"
config.DELAY_BEFORE_PICK = 0; config.GRIP_CLOSE_DELAY = 0; config.GRIP_HOLD_DELAY = 0
config.PREP_SETTLE_DELAY = 0; config.DET_TIMEOUT = 2; config.CONVEYOR_IDLE_STOP = 1.0

falhas = 0
def check(cond, msg):
    global falhas
    print(("ok: " if cond else "FALHOU: ") + msg)
    if not cond: falhas += 1

with serial_io.Arduino() as link:
    cfg = link.read_config()
    arm = orchestrator.Arm(link, cfg, log=lambda *a: None)
    vis, belt = FakeVision(), FakeBelt()
    r = orchestrator.Runner(arm, link, vis, belt, log=lambda *a: None)
    r.auto = True
    r.start()

    # 1. cena vazia: nada roda, esteira nao liga
    time.sleep(0.5)
    check(r.cycles == 0 and not belt.running, "cena vazia: nenhum ciclo, esteira desligada")

    # 2. uma caixinha, vista continuamente: UM ciclo so
    vis.cena = {11}
    time.sleep(0.4)
    check(belt.running and belt.starts == 1, "caixinha a vista: esteira liga (1 vez)")
    check(r.busy, "ciclo em curso enquanto o ID segue na cena")

    # Enquanto o ciclo roda, a camera le o mesmo ID em dezenas de frames:
    # nenhum ciclo extra pode ser enfileirado por isso.
    base, leituras, estavel = r.cycles, 0, True
    fim = time.monotonic() + 0.4
    while time.monotonic() < fim and r.busy:
        leituras += 1
        if r.cycles != base: estavel = False
        time.sleep(0.01)
    check(estavel, f"{leituras} leituras do mesmo ID durante o ciclo nao enfileiraram outro")
    vis.cena = set()                    # tira antes de o ciclo acabar
    while r.busy: time.sleep(0.05)
    check(r.cycles == base + 1, f"o ciclo em curso conta uma vez so (cycles={r.cycles})")
    vis.cena = {11}                     # repoe para o proximo caso

    # 3. o ID continua na cena: o proximo ciclo comeca, esteira NAO desliga
    time.sleep(0.4)
    check(belt.stops == 0 and belt.running, "entre ciclos seguidos a esteira permanece ligada")
    vis.cena = set()                    # tira a caixinha
    while r.busy: time.sleep(0.05)

    # 4. cena vazia: desliga apos CONVEYOR_IDLE_STOP, nao antes
    time.sleep(0.5)
    check(belt.running, "antes de 1 s ocioso a esteira segue ligada")
    time.sleep(1.0)
    check(not belt.running and belt.stops == 1, "apos 1 s sem caixinha a esteira desliga")

    # 5. duas caixinhas: opera a de MENOR ID
    link.serial.sent.clear()
    vis.cena = {17, 12}
    time.sleep(0.3)
    while r.busy: time.sleep(0.05)
    vis.cena = set()
    zonas = [c for c in link.serial.sent if c.startswith("prep ")]
    check(zonas and zonas[0] == "prep nordeste ccw", f"menor ID (12 -> nordeste) foi o operado: {zonas[:1]}")

    # 6. reuso: repor um ID ja operado volta a operar
    antes = r.cycles
    vis.cena = {11}
    time.sleep(0.3)
    while r.busy: time.sleep(0.05)
    vis.cena = set()
    check(r.cycles == antes + 1, "ID ja operado antes pode ser reposto e operado de novo")

    r.quit(); r.join(1)

print("\n" + ("todos os testes passaram" if not falhas else f"{falhas} FALHA(S)"))
sys.exit(1 if falhas else 0)
