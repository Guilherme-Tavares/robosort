"""Servidor MJPEG minimo (so biblioteca padrao) para expor a visao do
orquestrador na web: a tela de Fila do frontend aponta VITE_CAMERA_URL para
'/stream.mjpg' e exibe o mesmo feed usado na deteccao ArUco.
"""

import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BOUNDARY = "frame"


class FrameBroadcaster:
    """Guarda o ultimo JPEG publicado; acorda quem espera por um novo."""

    def __init__(self):
        self._cond = threading.Condition()
        self._jpg = None
        self._seq = 0

    def publish(self, jpg_bytes):
        with self._cond:
            self._jpg = jpg_bytes
            self._seq += 1
            self._cond.notify_all()

    def wait_for_frame(self, last_seq, timeout=2.0):
        """Bloqueia ate um frame com seq != last_seq, ou timeout. Devolve
        (jpg, seq); jpg e None se estourou o timeout sem frame novo."""
        with self._cond:
            if not self._cond.wait_for(lambda: self._seq != last_seq, timeout):
                return None, last_seq
            return self._jpg, self._seq


def _make_handler(broadcaster):
    class MJPEGHandler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass  # nao poluir o console do operador com acesso HTTP

        def do_GET(self):
            if self.path != "/stream.mjpg":
                self.send_response(404)
                self.end_headers()
                return
            self.send_response(200)
            self.send_header("Age", "0")
            self.send_header("Cache-Control", "no-cache, private")
            self.send_header("Pragma", "no-cache")
            self.send_header("Content-Type", f"multipart/x-mixed-replace; boundary={BOUNDARY}")
            self.end_headers()
            seq = 0
            try:
                while True:
                    jpg, seq = broadcaster.wait_for_frame(seq)
                    if jpg is None:
                        continue  # timeout: so re-espera, mantem a conexao viva
                    self.wfile.write(f"--{BOUNDARY}\r\n".encode())
                    self.wfile.write(b"Content-Type: image/jpeg\r\n")
                    self.wfile.write(f"Content-Length: {len(jpg)}\r\n\r\n".encode())
                    self.wfile.write(jpg)
                    self.wfile.write(b"\r\n")
            except (BrokenPipeError, ConnectionResetError):
                pass  # cliente fechou a aba; so encerra esta conexao

    return MJPEGHandler


def start_server(broadcaster, host, port, log=print):
    """Sobe o servidor MJPEG numa thread daemon. Devolve o server (para
    shutdown()/server_close() depois)."""
    server = ThreadingHTTPServer((host, port), _make_handler(broadcaster))
    thread = threading.Thread(target=server.serve_forever, daemon=True, name="mjpeg-server")
    thread.start()
    log(f"  stream mjpeg em http://{host}:{port}/stream.mjpg")
    return server
