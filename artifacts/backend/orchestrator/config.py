"""Configuracao do orquestrador.

Nenhum numero do braco vive aqui. Limites, poses (home, delivery), garra
(aberta, fechada), valores-guia dos cantos e aproximacao vem do firmware na
conexao ('dump' e 'corners'); fonte unica: robosort-firmware/config.h.
"""

# ---- Flags ----
ENABLE_VISION = False          # camera identifica o marcador da caixinha
ENABLE_LOCALIZATION = False   # visao localiza a caixinha e interpola o alvo;
                              # desligado: alvo fixo em FIXED_CORNER
ENABLE_CONVEYOR = True        # esteira LEGO via EV3 pelo console; desligado: a esteira
                              # sai da jogada e e assumida ligada (brick, outro PC, ou
                              # a caixinha movida a mao ate o sensor)
ENABLE_SORTING = True         # sensor IR + empurrador (firmware com ENABLE_SORTING=1)

FIXED_CORNER = 3              # canto usado quando a localizacao esta desligada

# ---- Serial ----
BAUD = 115200
BOOT_TIMEOUT = 6.0            # espera pelo READY apos abrir a porta (o Uno reseta)
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

# ---- Separacao ----
ZONE = "norte"                # unica zona nesta fase
DET_TIMEOUT = 60.0            # espera pela passagem da caixinha no sensor
PUSH_DELAY_AFTER_DET = 0.0    # s entre DET e push, se o sensor estiver antes do empurrador

# ---- Esteira (EV3) ----
EV3_PORT = "A"                # porta do motor no brick
CONVEYOR_SPEED = 10           # % ; arranque brusco derruba a caixinha
CONVEYOR_DIRECTION = -1       # 1 horario, -1 anti-horario
CONVEYOR_RAMP_TIME = 1.0      # s de rampa de aceleracao (a parada e imediata)


# ---- Roteamento provisorio (sem banco) ----
def route(marker_id):
    """ID par -> Rondonia (empurrador cw); impar -> Acre (ccw)."""
    return ("RO", "cw") if marker_id % 2 == 0 else ("AC", "ccw")
