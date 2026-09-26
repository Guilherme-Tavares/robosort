"""routing.report(): o orquestrador informando o estagio a API."""
from pathlib import Path
import json, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "orchestrator"))
import config, routing

recebido = []

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_PATCH(self):
        n = int(self.headers.get("Content-Length", 0))
        corpo = json.loads(self.rfile.read(n) or b"{}")
        volume = int(self.path.split("/")[-2])
        recebido.append((volume, corpo.get("status")))
        if volume == 404404:
            self.send_response(404); self.end_headers(); self.wfile.write(b'{}'); return
        self.send_response(200); self.send_header("Content-Length", "2"); self.end_headers(); self.wfile.write(b"{}")

srv = ThreadingHTTPServer(("127.0.0.1", 0), H)
threading.Thread(target=srv.serve_forever, daemon=True).start()

falhas = 0
def check(c, m):
    global falhas
    print(("ok: " if c else "FALHOU: ") + m)
    if not c: falhas += 1

config.ROUTE_SOURCE = "api"
config.API_BASE_URL = f"http://127.0.0.1:{srv.server_address[1]}/api"
config.API_TIMEOUT = 3.0

check(routing.report(10, "sorting") is None and recebido[-1] == (10, "sorting"),
      "PATCH /purchase/10/status {'status':'sorting'}")
check(routing.report(10, "done") is None and recebido[-1] == (10, "done"), "conclusao vira 'done'")
check(routing.report(10, "error") is None and recebido[-1] == (10, "error"), "falha vira 'error'")

aviso = routing.report(404404, "done")
check(isinstance(aviso, str) and "404" in aviso, f"compra inexistente devolve aviso, nao levanta: {aviso}")

config.ROUTE_SOURCE = "mock"
antes = len(recebido)
check(routing.report(10, "done") is None and len(recebido) == antes,
      "com ROUTE_SOURCE='mock' nao ha a quem reportar: nao chama a API")

config.ROUTE_SOURCE = "api"
srv.shutdown(); srv.server_close()
aviso = routing.report(10, "done")
check(isinstance(aviso, str), f"API fora do ar devolve aviso, nao levanta: {aviso[:60]}...")

print("\n" + ("todos os testes passaram" if not falhas else f"{falhas} FALHA(S)"))
sys.exit(1 if falhas else 0)
