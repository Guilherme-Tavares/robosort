"""Conexao a um firmware com USB nativo (Uno R4): sem READY ao abrir a porta."""
from pathlib import Path
import sys, time, queue
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "orchestrator"))
import serial_io, config

class FakeR4:
    """Ja rodando ha muito; o READY do boot se perdeu. Responde a comandos."""
    def __init__(self, port, baud, timeout):
        self.port=port; self.is_open=True; self.out=queue.Queue(); self.sent=[]
    def readline(self):
        try: return self.out.get(timeout=0.02)
        except queue.Empty: return b""
    def write(self, data):
        cmd=data.decode().strip(); self.sent.append(cmd)
        if cmd=="ping": self.out.put(b"OK\n")
        elif cmd=="dump":
            for s in ["STATE base 18 energizada 18 178 18 92","STATE garra 82 energizada 82 120 82 82",
                      "STATE altura 91 energizada 16 136 91 101","STATE alcance 96 energizada 36 176 96 102",
                      "GRIPPER 120 82","OK"]: self.out.put((s+"\n").encode())
        elif cmd=="corners":
            for s in ["CORNER 0 44 29 56","CORNER 1 40 37 57","CORNER 2 40 22 60","CORNER 3 37 31 58",
                      "APPROACH 39 78","DROP 92 98","OK"]: self.out.put((s+"\n").encode())
        else: self.out.put(b"OK\n")
    def flush(self): pass
    def close(self): self.is_open=False

class FakeDead(FakeR4):
    def write(self, data): self.sent.append(data.decode().strip())   # nunca responde

serial_io.find_port=lambda h=None:"FAKE"
config.BOOT_TIMEOUT=2.0; config.PROBE_INTERVAL=0.3; config.RESYNC_QUIET=0.2

serial_io.serial.Serial=FakeR4
t=time.monotonic()
with serial_io.Arduino() as link:
    dt=time.monotonic()-t
    assert link._boot=="OK" and link.serial.sent[0]=="ping"
    cfg=link.read_config()
    assert cfg.joints["base"].state=="energizada"      # estado persistiu: R4 nao resetou
    print(f"ok: R4 sem READY -> sonda 'ping' conecta em {dt:.1f} s; estado do firmware persiste")

serial_io.serial.Serial=FakeDead
try: serial_io.Arduino().open(); raise AssertionError
except serial_io.ArduinoError as e: print("ok: placa muda ->", e)
print("\ntodos os testes passaram")
