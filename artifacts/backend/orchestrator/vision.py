"""Visao: captura, deteccao ArUco e identificacao da caixinha.

Identificacao (ENABLE_VISION): qual caixinha esta na area, pelo ID do
marcador. Onde ela esta nao se pergunta: a area de aquisicao e unica e a pose
para pega-la vem do firmware.

A camera em si (abrir o dispositivo, detectar marcadores ArUco e devolver os
IDs) vive em artifacts/backend/cam/leitor_aruco.py; este modulo so importa
essas funcoes e constroi a identificacao em cima.

Uso direto, para posicionar a camera:
    python vision.py [--camera K]     janela ao vivo com as deteccoes
"""

import os
import sys
import threading
import time
from collections import Counter

import cv2
import numpy as np

import config

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "cam"))
import leitor_aruco  # noqa: E402  (precisa do sys.path acima)


class VisionError(RuntimeError):
    pass


# ------------------------------------------------------------------ camera

def open_camera(index=None):
    """Abre a camera do projeto (leitor_aruco.abrir_camera): indice
    explicito se dado, senao a padrao por nome USB (CAMERA_PADRAO)."""
    escolha = index if index is not None else leitor_aruco.CAMERA_PADRAO
    try:
        cap, indice, _ = leitor_aruco.abrir_camera(
            escolha, largura=config.CAMERA_WIDTH, altura=config.CAMERA_HEIGHT
        )
    except leitor_aruco.CameraError as exc:
        raise VisionError(str(exc)) from exc
    return cap, indice


def _dict_size(aruco_dict_name):
    """'DICT_4X4_50' -> 50, para leitor_aruco.criar_detector."""
    return int(aruco_dict_name.rsplit("_", 1)[-1])


# ---------------------------------------------------------------- detector

class Vision:
    def __init__(self, camera_index=None, log=print, stream=None):
        self.log = log
        self.cap, self.camera_index = open_camera(camera_index)
        self.detector = leitor_aruco.criar_detector(_dict_size(config.ARUCO_DICT))

        # Uma so thread le a camera (cap.read() nao e seguro entre threads):
        # alimenta tanto frame() quanto, se houver, o stream mjpeg da web.
        self._stream = stream
        self._frame_lock = threading.Lock()
        self._latest = None
        self._latest_ok = threading.Event()
        self._last_publish = 0.0
        self._grab_stop = threading.Event()
        self._grab_thread = threading.Thread(target=self._grab_loop, daemon=True, name="vision-grab")
        self._grab_thread.start()

    def attach_stream(self, broadcaster):
        """Liga o publicador mjpeg depois de a camera ja estar aberta."""
        self._stream = broadcaster

    def close(self):
        self._grab_stop.set()
        self._grab_thread.join(timeout=2.0)
        if self.cap:
            self.cap.release()
            self.cap = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def _grab_loop(self):
        min_interval = 1.0 / config.CAMERA_STREAM_FPS
        while not self._grab_stop.is_set():
            ok, img = self.cap.read()
            if not ok or img is None:
                time.sleep(0.05)  # nao gira a CPU se a camera falhar por um tempo
                continue
            with self._frame_lock:
                self._latest = img
            self._latest_ok.set()
            if self._stream is not None:
                now = time.monotonic()
                if now - self._last_publish >= min_interval:
                    self._last_publish = now
                    ok2, buf = cv2.imencode(
                        ".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, config.CAMERA_STREAM_JPEG_QUALITY]
                    )
                    if ok2:
                        self._stream.publish(buf.tobytes())

    def frame(self):
        if not self._latest_ok.wait(timeout=5.0):
            raise VisionError("camera parou de entregar frames")
        with self._frame_lock:
            return self._latest.copy()

    def detect(self, img):
        """{id: cantos (4x2 float32)} de tudo que estiver visivel."""
        return leitor_aruco.detectar_marcadores(img, self.detector)

    @staticmethod
    def products(found):
        """Marcadores de produto. Os de referencia impressos na folha ficam
        fora por serem abaixo de PRODUCT_ID_MIN."""
        return {i: c for i, c in found.items() if i >= config.PRODUCT_ID_MIN}

    # ------------------------------------------------------- identificacao

    def peek(self):
        """Menor ID de produto visivel neste frame, ou None. Barato; para o
        modo automatico saber se ha o que identificar."""
        found = self.products(self.detect(self.frame()))
        return min(found, default=None)

    def identify(self):
        """ID da caixinha a operar. Exige o mesmo ID em IDENTIFY_MIN_HITS
        frames, para nao agir sobre um falso positivo de um frame so; com
        mais de uma caixinha na cena, opera a de **menor ID**, para a ordem
        nao depender de qual marcador o detector viu primeiro."""
        hits = Counter()
        for _ in range(config.IDENTIFY_ATTEMPTS):
            found = self.products(self.detect(self.frame()))
            for i in found:
                hits[i] += 1
            if any(n >= config.IDENTIFY_MIN_HITS for n in hits.values()):
                break
            time.sleep(config.IDENTIFY_INTERVAL)
        if not hits:
            raise VisionError("nenhum marcador de produto visivel")
        estaveis = [i for i, n in hits.items() if n >= config.IDENTIFY_MIN_HITS]
        if not estaveis:
            visto = ", ".join(f"{i} em {n}" for i, n in sorted(hits.items()))
            raise VisionError(
                f"nenhum marcador estavel em {config.IDENTIFY_ATTEMPTS} frames "
                f"(minimo {config.IDENTIFY_MIN_HITS}; visto: {visto})"
            )
        pid = min(estaveis)
        if len(estaveis) > 1:
            self.log(f"    {len(estaveis)} produtos na cena {sorted(estaveis)}; opera o menor: {pid}")
        return pid


# ------------------------------------------------------------------ preview

def preview(camera_index=None):
    """Janela ao vivo com os marcadores detectados, para posicionar a camera."""
    with Vision(camera_index) as v:
        print(f"  camera {v.camera_index}. q sai.")
        while True:
            img = v.frame()
            found = v.detect(img)
            if found:
                ids = np.array([[i] for i in found])
                cv2.aruco.drawDetectedMarkers(img, [c.reshape(1, 4, 2) for c in found.values()], ids)
            y = 30
            produtos = v.products(found)
            resumo = ", ".join(str(i) for i in sorted(produtos)) or "nenhum"
            cv2.putText(img, f"produtos: {resumo}", (10, y),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            for i in sorted(produtos):
                y += 28
                cv2.putText(img, f"id {i}", (10, y),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.imshow("robosort vision", img)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    cv2.destroyAllWindows()


if __name__ == "__main__":
    idx = int(sys.argv[sys.argv.index("--camera") + 1]) if "--camera" in sys.argv else None
    try:
        preview(idx)
    except VisionError as exc:
        print(f"!! {exc}")
        sys.exit(1)
