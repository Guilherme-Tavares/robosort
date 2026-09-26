"""Configuracao do orquestrador.

Nenhum numero do braco vive aqui. Limites, poses (home, delivery), garra
(aberta, fechada), valores-guia dos cantos e aproximacao vem do firmware na
conexao ('dump' e 'corners'); fonte unica: robosort-firmware/config.h.
"""

# ---- Flags ----
ENABLE_VISION = True           # camera identifica o marcador da caixinha
ENABLE_LOCALIZATION = False   # visao localiza a caixinha e interpola o alvo;
                              # desligado: alvo fixo em FIXED_CORNER
ENABLE_CONVEYOR = True        # esteira LEGO via EV3 pelo console; desligado: a esteira
                              # sai da jogada e e assumida ligada (brick, outro PC, ou
                              # a caixinha movida a mao ate o sensor)
ENABLE_SORTING = True         # sensor IR + empurrador (firmware com ENABLE_SORTING=1)

FIXED_CORNER = 0              # unica area de aquisicao por enquanto; os demais cantos nao valem

# ---- Serial ----
BAUD = 115200
BOOT_TIMEOUT = 6.0            # espera pelo firmware responder apos abrir a porta
PROBE_INTERVAL = 1.0          # entre 'ping' de sondagem enquanto nao ha READY
ACK_TIMEOUT = 8.0             # comando simples: maior que o movimento mais longo (160 graus, ~5 s)
SEQUENCE_TIMEOUT = 40.0       # mv home/dest/area e push: varios movimentos e pausas
RESYNC_QUIET = 0.5            # silencio na serial que caracteriza fim de respostas atrasadas

# ---- Camera ----
CAMERA_INDEX = None           # None: primeira que abrir e entregar frame
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
IDENTIFY_ATTEMPTS = 15        # frames tentados na identificacao
IDENTIFY_MIN_HITS = 3         # frames em que o mesmo ID precisa aparecer
IDENTIFY_INTERVAL = 0.1       # s entre frames

# ---- Stream MJPEG (camera na web) ----
CAMERA_STREAM_HOST = "0.0.0.0"     # alcancavel na LAN, para a tela de fila em outro dispositivo
CAMERA_STREAM_PORT = 8090          # evita 5000 (proxy do front) e 5173 (vite dev)
CAMERA_STREAM_FPS = 15             # taxa de publicacao; a captura para o ArUco roda no fps nativo
CAMERA_STREAM_JPEG_QUALITY = 80

# ---- Area de aquisicao (cm) ----
# Referencial do mundo: origem no vertice superior-esquerdo da area, x para a
# direita, y para baixo. A area e um quadrado de AREA_SIZE_CM de lado.
AREA_SIZE_CM = 3.0
BOX_SIZE_CM = 1.5
CELL_SIZE_CM = AREA_SIZE_CM / 2       # grade 2x2 de celulas

# Marcadores de referencia: quadrados de MARKER_SIZE_CM, bordas paralelas as
# da area, fora dela, com MARKER_GAP_CM de folga por eixo entre o vertice
# interno do marcador e o vertice da area (medido no gabarito a 300 DPI).
MARKER_SIZE_CM = 2.0
MARKER_GAP_CM = 1.0

# ID do marcador de referencia -> canto da area que ele acompanha. Serve so
# a homografia; nada a ver com os valores-guia dos cantos, que sao poses do
# braco por celula e vem do firmware.
REFERENCE_MARKERS = {0: "TL", 1: "TR", 2: "BL", 3: "BR"}

# Produtos: qualquer ID fora dos de referencia. 0-9 reservados.
PRODUCT_ID_MIN = 10

ARUCO_DICT = "DICT_4X4_50"

# ---- Ciclo ----
# Pausas em torno da garra, iguais as do firmware (GRIP_*_DELAY_MS). O
# orquestrador comanda junta a junta, entao aplica as esperas ele mesmo.
DELAY_BEFORE_PICK = 3.0       # tempo para o operador posicionar a caixinha
GRIP_CLOSE_DELAY = 1.0        # antes de a garra fechar
GRIP_HOLD_DELAY = 1.0         # depois de fechar
PREP_SETTLE_DELAY = 1.0       # depois de o empurrador chegar a pre-posicao

# ---- Separacao ----
# Cinco zonas, uma por regiao; mesmos nomes do firmware (config.h Z1..Z5).
ZONES = ["norte", "nordeste", "centro-oeste", "sudeste", "sul"]
DET_TIMEOUT = 60.0            # espera pela passagem da caixinha no sensor
# A latencia entre DET e empurrao e do firmware (PUSHER_DET_DELAY_MS): quem
# empurra na deteccao e ele, sem passar pelo PC.

# ---- Esteira (EV3) ----
EV3_PORT = "A"                # porta do motor no brick
CONVEYOR_SPEED = 10           # % ; arranque brusco derruba a caixinha
CONVEYOR_DIRECTION = -1       # 1 horario, -1 anti-horario
CONVEYOR_RAMP_TIME = 1.0      # s de rampa de aceleracao (a parada e imediata)
CONVEYOR_IDLE_STOP = 3.0      # s sem caixinha a vista, apos um ciclo, ate desligar


# ---- Roteamento: de onde vem o destino da caixinha ----
# "api": consulta a API (GET /purchase/<id>), que devolve o estado e a
#        regiao do pedido. O destino e do PEDIDO: se o primeiro pedido for
#        para SP, a caixinha 10 vai para Sao Paulo. Sem limite de ID.
# "mock": a tabela ROTEAMENTO abaixo, para bancada sem MySQL.
# Nao ha volta ao mock em caso de falha da API: ver routing.py.
ROUTE_SOURCE = "api"
API_BASE_URL = "http://localhost:3000/api"
API_TIMEOUT = 3.0

# Lado do compartimento de cada estado na esteira: geometria da bancada, nao
# dado de negocio, por isso fica aqui e nao no banco. O 1o estado de cada
# regiao gira anti-horario; o 2o, horario (convencao do calibration-tool).
SENTIDO_POR_UF = {
    "RO": "ccw", "AC": "cw",     # Norte
    "BA": "ccw", "CE": "cw",     # Nordeste
    "GO": "ccw", "MT": "cw",     # Centro-Oeste
    "SP": "ccw", "RJ": "cw",     # Sudeste
    "PR": "ccw", "RS": "cw",     # Sul
}

# Mock: ArUco 10-21 -> (zona, estado, sentido). Espelha o seed da API
# (Region 1..5 e dois estados cada) com o primeiro pedido em 10.
ROTEAMENTO = {
    10: ("norte", "RO", "ccw"),
    11: ("norte", "AC", "cw"),
    12: ("nordeste", "BA", "ccw"),
    13: ("nordeste", "CE", "cw"),
    14: ("centro-oeste", "GO", "ccw"),
    15: ("centro-oeste", "MT", "cw"),
    16: ("sudeste", "SP", "ccw"),
    17: ("sudeste", "RJ", "cw"),
    18: ("sul", "PR", "ccw"),
    19: ("sul", "RS", "cw"),
    20: ("norte", "RO", "ccw"),
    21: ("norte", "AC", "cw"),
}


# route() vive em routing.py, que escolhe a fonte por ROUTE_SOURCE.
