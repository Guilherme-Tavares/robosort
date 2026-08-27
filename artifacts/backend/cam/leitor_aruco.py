import argparse
import glob
import os

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


def _fabricante(pasta):
    dispositivo = os.path.realpath(os.path.join(pasta, "device"))
    caminho = os.path.join(os.path.dirname(dispositivo), "idVendor")
    try:
        with open(caminho) as arquivo:
            vendor = arquivo.read().strip()
    except OSError:
        return None
    return FABRICANTES.get(vendor, vendor)


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
        for indice in range(5):
            captura = cv2.VideoCapture(indice)
            if captura.isOpened():
                cameras.append((indice, f"camera {indice}"))
            captura.release()

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


def main():
    parser = argparse.ArgumentParser(description="Leitor de ArUco 4x4")
    parser.add_argument("--dict", type=int, default=50, choices=sorted(DICIONARIOS),
                        help="tamanho do dicionario 4x4 (padrao: 50)")
    parser.add_argument("--camera", default=CAMERA_PADRAO,
                        help=f"indice ou parte do nome da camera (padrao: {CAMERA_PADRAO})")
    parser.add_argument("--listar", action="store_true",
                        help="lista as cameras disponiveis e sai")
    args = parser.parse_args()

    cameras = listar_cameras()

    if args.listar:
        mostrar_cameras(cameras)
        return

    indice = resolver_camera(args.camera, cameras)
    if indice is None:
        print(f"Nenhuma camera com o nome '{args.camera}'.")
        mostrar_cameras(cameras)
        if not cameras:
            return
        indice = cameras[0][0]
        print(f"Usando a camera [{indice}] no lugar.")

    nome = dict(cameras).get(indice, f"camera {indice}")

    camera = cv2.VideoCapture(indice)
    if not camera.isOpened():
        print(f"Nao foi possivel abrir a camera [{indice}] {nome}.")
        mostrar_cameras(cameras)
        return

    dicionario = cv2.aruco.getPredefinedDictionary(DICIONARIOS[args.dict])
    parametros = cv2.aruco.DetectorParameters()
    detector = cv2.aruco.ArucoDetector(dicionario, parametros)

    ultimos_ids = None
    print(f"Camera [{indice}] {nome} | DICT_4X4_{args.dict}. Pressione 'q' para sair.")

    while True:
        ok, frame = camera.read()
        if not ok:
            print("Falha ao ler o frame da camera.")
            break

        cantos, ids, _ = detector.detectMarkers(frame)

        if ids is not None:
            cv2.aruco.drawDetectedMarkers(frame, cantos, ids)

            lista_ids = sorted(int(i) for i in ids.flatten())
            if lista_ids != ultimos_ids:
                print("Marcadores detectados:", lista_ids)
                ultimos_ids = lista_ids

            cv2.putText(frame, f"IDs: {lista_ids}", (20, 40),
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
