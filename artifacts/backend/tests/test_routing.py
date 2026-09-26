"""Roteamento pelas duas fontes, contra uma API falsa de verdade (HTTP).

Cobre: destino vem do pedido (ID 10 -> SP, nao RO); IDs acima de 21;
pedido inexistente; API fora do ar; resposta invalida; regiao desconhecida;
e que 'api' nunca cai no mock.
"""
from pathlib import Path
import json, sys, threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "orchestrator"))
import config, routing

# Pedidos como o banco os teria: o 1o pedido (volume 10) foi para SP.
PEDIDOS = {
    10: {"state": {"uf": "SP", "name": "Sao Paulo"}, "region": "Sudeste"},
    11: {"state": {"uf": "AC", "name": "Acre"}, "region": "Norte"},
    25: {"state": {"uf": "RS", "name": "Rio Grande do Sul"}, "region": "Sul"},
    30: {"state": {"uf": "GO", "name": "Goias"}, "region": "Centro-Oeste"},
    98: {"state": {"uf": "SP", "name": "Sao Paulo"}, "region": "Sudestex"},  # regiao invalida
    99: {"naoTem": True},                                                    # resposta sem estado
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        vol = int(self.path.rsplit("/", 1)[-1])
        if vol == 97:
            self.send_response(500); self.end_headers(); self.wfile.write(b'{"error":"boom"}'); return
        if vol not in PEDIDOS:
            self.send_response(404); self.send_header("Content-Type", "application/json")
            self.end_headers(); self.wfile.write(b'{"error":"Compra nao encontrada."}'); return
        corpo = json.dumps(PEDIDOS[vol]).encode()
        self.send_response(200); self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(corpo))); self.end_headers(); self.wfile.write(corpo)


srv = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
threading.Thread(target=srv.serve_forever, daemon=True).start()
porta = srv.server_address[1]

falhas = 0
def check(cond, msg):
    global falhas
    print(("ok: " if cond else "FALHOU: ") + msg)
    if not cond: falhas += 1

def erro(fn, tipo, msg):
    try:
        fn(); check(False, f"{msg} (nao levantou)")
    except tipo as exc:
        check(True, f"{msg} -> {exc}")
    except Exception as exc:
        check(False, f"{msg} (levantou {type(exc).__name__}: {exc})")

# ------------------------------------------------------------------ mock
config.ROUTE_SOURCE = "mock"
check(routing.route(10) == ("norte", "RO", "ccw"), "mock: 10 -> norte/RO/ccw")
erro(lambda: routing.route(30), routing.UnknownMarker, "mock: ID fora da tabela")

# ------------------------------------------------------------------- api
config.ROUTE_SOURCE = "api"
config.API_BASE_URL = f"http://127.0.0.1:{porta}/api"
config.API_TIMEOUT = 3.0

check(routing.route(10) == ("sudeste", "SP", "ccw"),
      "api: o pedido manda - 10 vai para SP (sudeste), nao para RO do mock")
check(routing.route(11) == ("norte", "AC", "cw"), "api: 11 -> norte/AC/cw")
check(routing.route(25) == ("sul", "RS", "cw"), "api: ID 25, acima do limite do mock")
check(routing.route(30) == ("centro-oeste", "GO", "ccw"),
      "api: 30 -> centro-oeste (nome com hifen casa a zona)")

erro(lambda: routing.route(12), routing.UnknownMarker, "api: pedido inexistente (404)")
erro(lambda: routing.route(97), routing.RoutingError, "api: erro 500")
erro(lambda: routing.route(98), routing.RoutingError, "api: regiao desconhecida")
erro(lambda: routing.route(99), routing.RoutingError, "api: resposta sem estado")

# API fora do ar: nunca cai no mock
srv.shutdown(); srv.server_close()
erro(lambda: routing.route(10), routing.RoutingError, "api fora do ar nao volta ao mock")

config.ROUTE_SOURCE = "invalido"
erro(lambda: routing.route(10), routing.RoutingError, "ROUTE_SOURCE invalido")

print("\n" + ("todos os testes passaram" if not falhas else f"{falhas} FALHA(S)"))
sys.exit(1 if falhas else 0)
