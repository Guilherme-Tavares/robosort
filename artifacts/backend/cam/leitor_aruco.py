"""A camera do projeto: abre o dispositivo fisico (por nome USB ou indice) e
detecta marcadores ArUco 4x4, devolvendo os IDs encontrados. Fonte unica de
acesso a camera; o orchestrator (vision.py) importa estas mesmas funcoes em
vez de abrir a camera por conta propria.

Uso direto, para diagnostico:
    python leitor_aruco.py [--listar] [--camera NOME_OU_INDICE] [--dict N]
"""

import argparse
import glob
import os
import sys

import cv2

DICIONARIOS = {
    50: cv2.aruco.DICT_4X4_50,
    100: cv2.aruco.DICT_4X4_100,
    250: cv2.aruco.DICT_4X4_250,
    1000: cv2.aruco.DICT_4X4_1000,
}

FABRICANTES = {
    "046d": "Logitech",
}

CAMERA_PADRAO = "logitech"


class CameraError(RuntimeError):
    pass


def _fabricante(pasta):
    dispositivo = os.path.realpath(os.path.join(pasta, "device"))
    caminho = os.path.join(os.path.dirname(dispositivo), "idVendor")
    try:
        with open(caminho) as arquivo:
            vendor = arquivo.read().strip()
    except OSError:
        return None
    return FABRICANTES.get(vendor, vendor)


def _abrir_captura(indice):
    """VideoCapture com o backend certo da plataforma. No Windows o DirectShow
    abre rapido e sem os avisos do MSMF; no resto, o padrao do OpenCV."""
    if sys.platform == "win32":
        return cv2.VideoCapture(indice, cv2.CAP_DSHOW)
    return cv2.VideoCapture(indice)


def listar_cameras():
    cameras = []
    for pasta in sorted(glob.glob("/sys/class/video4linux/video*")):
        try:
            with open(os.path.join(pasta, "index")) as arquivo:
                if arquivo.read().strip() != "0":
                    continue
            with open(os.path.join(pasta, "name")) as arquivo:
                nome = arquivo.read().strip()
        except OSError:
            continue

        marca = _fabricante(pasta)
        if marca and marca.lower() not in nome.lower():
            nome = f"{nome} [{marca}]"

        indice = int(os.path.basename(pasta).replace("video", ""))
        cameras.append((indice, nome))

    if not cameras:
        # Fora do Linux nao ha /sys: sonda os primeiros indices. Usa o mesmo
        # backend de abrir_camera; com o padrao (MSMF no Windows) a sondagem
        # e lenta. Cada indice vazio faz o OpenCV logar; como aqui a falha e
        # esperada, o log fica silenciado so durante a sondagem.
        nivel = cv2.utils.logging.getLogLevel()
        cv2.utils.logging.setLogLevel(cv2.utils.logging.LOG_LEVEL_SILENT)
        try:
            for indice in range(5):
                captura = _abrir_captura(indice)
                if captura.isOpened():
                    cameras.append((indice, f"camera {indice}"))
                captura.release()
        finally:
            cv2.utils.logging.setLogLevel(nivel)

    return cameras


def resolver_camera(escolha, cameras):
    if escolha.isdigit():
        return int(escolha)

    for indice, nome in cameras:
        if escolha.lower() in nome.lower():
            return indice
    return None


def mostrar_cameras(cameras):
    if not cameras:
        print("Nenhuma camera encontrada.")
        return
    print("Cameras disponiveis:")
    for indice, nome in cameras:
        print(f"  [{indice}] {nome}")


# --------------------------------------------------------------- reutilizavel

def abrir_camera(escolha=CAMERA_PADRAO, log=None, largura=None, altura=None):
    """Abre a camera pedida (indice, ou nome/trecho do nome, ex. 'logitech').
    Confirma que ela entrega frame antes de aceitar. Devolve (VideoCapture,
    indice, nome); levanta CameraError se nao achar ou nao conseguir abrir.
    'log', se dado, recebe mensagens informativas (fallback de nome, etc.).
    'largura'/'altura', se dados, sao aplicados ANTES do frame de
    confirmacao: trocar a resolucao depois de ja ter lido pode travar
    algumas webcams UVC na proxima leitura."""
    aviso = log or (lambda *a: None)
    cameras = listar_cameras()
    indice = resolver_camera(str(escolha), cameras)
    if indice is None:
        if not cameras:
            raise CameraError(f"nenhuma camera encontrada para abrir '{escolha}'")
        # Os nomes vem do /sys do Linux; fora dele a lista sai como
        # 'camera N' e nome nenhum casa. Nao e erro: e a plataforma.
        if all(nome.startswith("camera ") for _, nome in cameras):
            aviso(f"Nesta plataforma a camera nao tem nome; '{escolha}' so casa por indice.")
        else:
            aviso(f"Nenhuma camera com o nome '{escolha}'.")
            mostrar_cameras(cameras)
        indice = cameras[0][0]
        aviso(f"Usando a camera [{indice}].")

    nome = dict(cameras).get(indice, f"camera {indice}")
    camera = _abrir_captura(indice)
    if not camera.isOpened():
        camera.release()
        raise CameraError(f"nao foi possivel abrir a camera [{indice}] {nome}")
    if largura is not None:
        camera.set(cv2.CAP_PROP_FRAME_WIDTH, largura)
    if altura is not None:
        camera.set(cv2.CAP_PROP_FRAME_HEIGHT, altura)
    ok, _ = camera.read()
    if not ok:
        camera.release()
        raise CameraError(f"camera [{indice}] {nome} nao entrega frames")
    return camera, indice, nome


def criar_detector(tamanho_dict=50):
    """ArucoDetector para o dicionario DICT_4X4_<tamanho_dict>."""
    dicionario = cv2.aruco.getPredefinedDictionary(DICIONARIOS[tamanho_dict])
    return cv2.aruco.ArucoDetector(dicionario, cv2.aruco.DetectorParameters())


def detectar_marcadores(frame, detector):
    """{id: cantos (4x2 float32)} de tudo que o detector achar no frame."""
    cantos, ids, _ = detector.detectMarkers(frame)
    if ids is None:
        return {}
    return {int(i): c[0] for c, i in zip(cantos, ids.ravel())}


def ler_ids(frame, detector):
    """Codigo(s) ArUco visiveis no frame: lista ordenada dos IDs de marcador."""
    return sorted(detectar_marcadores(frame, detector).keys())


def _desenhar_marcadores(frame, marcadores):
    import numpy as np

    cantos = [c.reshape(1, 4, 2) for c in marcadores.values()]
    ids = np.array([[i] for i in marcadores])
    cv2.aruco.drawDetectedMarkers(frame, cantos, ids)


def main():
    parser = argparse.ArgumentParser(description="Leitor de ArUco 4x4")
    parser.add_argument("--dict", type=int, default=50, choices=sorted(DICIONARIOS),
                        help="tamanho do dicionario 4x4 (padrao: 50)")
    parser.add_argument("--camera", default=CAMERA_PADRAO,
                        help=f"indice ou parte do nome da camera (padrao: {CAMERA_PADRAO})")
    parser.add_argument("--listar", action="store_true",
                        help="lista as cameras disponiveis e sai")
    args = parser.parse_args()

    if args.listar:
        mostrar_cameras(listar_cameras())
        return

    try:
        camera, indice, nome = abrir_camera(args.camera, log=print)
    except CameraError as exc:
        print(exc)
        mostrar_cameras(listar_cameras())
        return

    detector = criar_detector(args.dict)

    ultimos_ids = None
    print(f"Camera [{indice}] {nome} | DICT_4X4_{args.dict}. Pressione 'q' para sair.")

    while True:
        ok, frame = camera.read()
        if not ok:
            print("Falha ao ler o frame da camera.")
            break

        marcadores = detectar_marcadores(frame, detector)
        if marcadores:
            _desenhar_marcadores(frame, marcadores)
            ids = sorted(marcadores)
            if ids != ultimos_ids:
                print("Marcadores detectados:", ids)
                ultimos_ids = ids
            cv2.putText(frame, f"IDs: {ids}", (20, 40),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        else:
            ultimos_ids = None

        cv2.imshow(f"ArUco 4x4_{args.dict} - {nome}", frame)

        tecla = cv2.waitKey(1) & 0xFF
        if tecla in (ord("q"), 27):  # 'q' ou ESC
            break

    camera.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
