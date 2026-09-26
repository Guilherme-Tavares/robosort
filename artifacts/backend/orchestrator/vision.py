"""Visao: captura, deteccao ArUco, identificacao e localizacao.

Identificacao (ENABLE_VISION): qual caixinha esta na area, pelo ID do
marcador. Localizacao (ENABLE_LOCALIZATION): onde ela esta, em cm, pela
homografia da folha; so entra no ciclo quando os valores-guia dos cantos
estiverem confiaveis.

A camera em si (abrir o dispositivo, detectar marcadores ArUco e devolver os
IDs) vive em artifacts/backend/cam/leitor_aruco.py; este modulo so importa
essas funcoes e constroi a logica de negocio em cima (homografia, roteamento
por posicao).

Homografia congelada: calculada quando os quatro marcadores de referencia
estao visiveis, guardada, e reusada enquanto o braco obstrui a folha. Usa os
16 cantos dos marcadores, nao os 4 centros: com quatro pontos a solucao e
exatamente determinada e qualquer erro de deteccao entra inteiro na matriz.

Uso direto, para posicionar a camera:
    python vision.py [--camera K]     janela ao vivo com deteccoes e coordenadas
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


# ---------------------------------------------------------------- geometria

def reference_world_corners():
    """Cantos de cada marcador de referencia em cm, na ordem que o OpenCV
    devolve (superior-esquerdo, superior-direito, inferior-direito,
    inferior-esquerdo), para marcadores impressos a 0 graus."""
    half = config.MARKER_SIZE_CM / 2
    off = config.MARKER_GAP_CM + half              # do vertice da area ao centro do marcador
    far = config.AREA_SIZE_CM + off
    centers = {"TL": (-off, -off), "TR": (far, -off), "BL": (-off, far), "BR": (far, far)}
    world = {}
    for mid, corner in config.REFERENCE_MARKERS.items():
        cx, cy = centers[corner]
        world[mid] = np.float32([
            [cx - half, cy - half], [cx + half, cy - half],
            [cx + half, cy + half], [cx - half, cy + half],
        ])
    return world


REF_WORLD = reference_world_corners()

# Faixa valida para o centro da caixinha: ela tem tamanho e nao ultrapassa
# as bordas, entao o centro fica a pelo menos meia caixa de cada borda.
CENTER_MIN = config.BOX_SIZE_CM / 2
CENTER_MAX = config.AREA_SIZE_CM - config.BOX_SIZE_CM / 2


# ---------------------------------------------------------------- detector

class Vision:
    def __init__(self, camera_index=None, log=print, stream=None):
        self.log = log
        self.cap, self.camera_index = open_camera(camera_index)
        self.detector = leitor_aruco.criar_detector(_dict_size(config.ARUCO_DICT))
        self.H = None                  # homografia congelada: imagem -> cm
        self.H_at = None

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
        return {i: c for i, c in found.items()
                if i >= config.PRODUCT_ID_MIN and i not in config.REFERENCE_MARKERS}

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

    # --------------------------------------------------------- localizacao

    def update_homography(self, found):
        """Recalcula a homografia se os quatro marcadores de referencia
        estiverem visiveis; senao mantem a ultima. True se recalculou."""
        if not all(mid in found for mid in config.REFERENCE_MARKERS):
            return False
        img_pts = np.concatenate([found[mid] for mid in config.REFERENCE_MARKERS])
        world_pts = np.concatenate([REF_WORLD[mid] for mid in config.REFERENCE_MARKERS])
        H, _ = cv2.findHomography(img_pts, world_pts, 0)
        if H is None:
            return False
        self.H = H
        self.H_at = time.monotonic()
        return True

    def to_world(self, pt):
        if self.H is None:
            raise VisionError("sem homografia: os quatro marcadores de referencia nunca foram vistos")
        p = np.float32([[pt]])
        x, y = cv2.perspectiveTransform(p, self.H)[0][0]
        return float(x), float(y)

    def locate(self, pid=None):
        """(id, x, y) em cm do centro do marcador da caixinha. Recalcula a
        homografia se a folha estiver desobstruida; senao usa a congelada."""
        found = self.detect(self.frame())
        self.update_homography(found)
        prods = self.products(found)
        if pid is not None:
            prods = {i: c for i, c in prods.items() if i == pid}
        if not prods:
            raise VisionError("caixinha nao visivel para localizar")
        i, corners = next(iter(prods.items()))
        x, y = self.to_world(corners.mean(axis=0))
        if not (CENTER_MIN <= x <= CENTER_MAX and CENTER_MIN <= y <= CENTER_MAX):
            raise VisionError(f"caixinha {i} fora da area valida: ({x:.2f}, {y:.2f}) cm")
        return i, x, y


# ------------------------------------------------------------------ preview

def preview(camera_index=None):
    """Janela ao vivo: marcadores, homografia e coordenadas da caixinha."""
    with Vision(camera_index) as v:
        print(f"  camera {v.camera_index}. q sai.")
        while True:
            img = v.frame()
            found = v.detect(img)
            fresh = v.update_homography(found)
            if found:
                ids = np.array([[i] for i in found])
                cv2.aruco.drawDetectedMarkers(img, [c.reshape(1, 4, 2) for c in found.values()], ids)
            status = "H: " + ("nova" if fresh else "congelada" if v.H is not None else "nenhuma")
            y = 30
            cv2.putText(img, status, (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            for i, c in v.products(found).items():
                y += 28
                text = f"id {i}"
                if v.H is not None:
                    x, yy = v.to_world(c.mean(axis=0))
                    ok = CENTER_MIN <= x <= CENTER_MAX and CENTER_MIN <= yy <= CENTER_MAX
                    text += f"  ({x:.2f}, {yy:.2f}) cm {'ok' if ok else 'FORA'}"
                cv2.putText(img, text, (10, y), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
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
