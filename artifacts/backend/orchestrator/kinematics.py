"""Interpolacao bilinear dos valores-guia.

Os quatro valores-guia (lidos do firmware com 'corners') sao poses do braco
para o centro de cada celula da grade 2x2. Os centros formam um quadrado de
CELL_SIZE_CM de lado, entre CENTER_MIN e CENTER_MAX em cada eixo; qualquer
posicao valida do centro da caixinha cai dentro dele, entao e sempre
interpolacao, nunca extrapolacao.

Cantos: 0 superior-esquerdo, 1 superior-direito, 2 inferior-esquerdo,
3 inferior-direito. u cresce para a direita, v para baixo.
"""

import config

AXES = ("base", "altura", "alcance")


def normalize(x, y):
    """(x, y) em cm -> (u, v) em 0..1 dentro do quadrado dos centros."""
    lo = config.BOX_SIZE_CM / 2
    span = config.CELL_SIZE_CM
    u = (x - lo) / span
    v = (y - lo) / span
    if not (0.0 <= u <= 1.0 and 0.0 <= v <= 1.0):
        raise ValueError(f"({x:.2f}, {y:.2f}) cm fora do quadrado dos centros")
    return u, v


def interpolate(u, v, c0, c1, c2, c3):
    top = c0 * (1 - u) + c1 * u
    bottom = c2 * (1 - u) + c3 * u
    return round(top * (1 - v) + bottom * v)


def target(corners, x, y):
    """{"base", "altura", "alcance"} para a caixinha em (x, y) cm."""
    u, v = normalize(x, y)
    return {axis: interpolate(u, v, *(corners[k][axis] for k in range(4))) for axis in AXES}
