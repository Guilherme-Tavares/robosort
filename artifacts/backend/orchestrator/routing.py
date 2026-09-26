"""De onde vem o destino de uma caixinha: da API (banco) ou do mock.

O ID do marcador ArUco e o 'volume' da compra: a API o gera a partir de 10,
incrementando. Dado o ID, ela responde o estado e a regiao do pedido; o
destino, portanto, e do PEDIDO, nao do ID. Se o primeiro pedido escolher SP,
a caixinha 10 vai para Sao Paulo, e nao para Rondonia como no mock.

Divisao de responsabilidade:

    ID -> estado (UF) e regiao        API/banco (ou o mock)
    regiao -> zona                    aqui, pelo nome
    UF -> sentido do empurrador       aqui, por SENTIDO_POR_UF

O sentido nao vem do banco de proposito: de que lado da esteira fica o
compartimento de cada estado e geometria da bancada, nao dado de negocio.
Remontou os compartimentos, muda a tabela; o banco nao tem nada com isso.

Com ROUTE_SOURCE = "api" nao ha volta ao mock: uma falha da API aborta o
ciclo com mensagem. Cair no mock em silencio mandaria a caixinha para o
compartimento errado sem ninguem perceber.
"""

import json
import urllib.error
import urllib.request

import config


class RoutingError(RuntimeError):
    """Falha de infraestrutura: API fora do ar, resposta invalida, regiao
    desconhecida. Derruba o modo automatico: e problema de configuracao ou
    de servico, nao da caixinha."""


class UnknownMarker(RuntimeError):
    """A caixinha lida nao tem pedido (ou esta fora do mock). Aborta so o
    ciclo; a proxima caixinha pode estar correta."""


def _zone_of_region(region):
    """'Centro-Oeste' -> 'centro-oeste'. Os nomes das regioes no seed da API
    sao os mesmos das zonas do firmware (config.h, Z1..Z5)."""
    zone = str(region).strip().lower()
    if zone not in config.ZONES:
        raise RoutingError(f"regiao '{region}' nao corresponde a nenhuma zona {config.ZONES}")
    return zone


def _direction_of_uf(uf):
    try:
        return config.SENTIDO_POR_UF[str(uf).strip().upper()]
    except KeyError:
        raise RoutingError(
            f"estado '{uf}' sem compartimento na esteira; ver SENTIDO_POR_UF em config.py"
        ) from None


# ----------------------------------------------------------------- fontes

def route_mock(marker_id):
    try:
        return config.ROTEAMENTO[marker_id]
    except KeyError:
        raise UnknownMarker(
            f"marcador {marker_id} sem roteamento (o mock cobre "
            f"{min(config.ROTEAMENTO)}-{max(config.ROTEAMENTO)}); "
            f"com ROUTE_SOURCE = 'api' qualquer pedido vale"
        ) from None


def route_api(marker_id):
    """GET /purchase/<id> -> (zona, uf, sentido)."""
    url = f"{config.API_BASE_URL.rstrip('/')}/purchase/{marker_id}"
    try:
        with urllib.request.urlopen(url, timeout=config.API_TIMEOUT) as resposta:
            corpo = json.loads(resposta.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            raise UnknownMarker(f"marcador {marker_id} sem pedido no banco") from None
        raise RoutingError(f"API respondeu {exc.code} para o marcador {marker_id}") from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise RoutingError(f"API inacessivel em {config.API_BASE_URL} ({exc})") from exc
    except json.JSONDecodeError as exc:
        raise RoutingError(f"API devolveu resposta ilegivel para o marcador {marker_id}") from exc

    try:
        uf = corpo["state"]["uf"]
        region = corpo["region"]
    except (KeyError, TypeError) as exc:
        raise RoutingError(f"resposta da API sem estado/regiao: {corpo}") from exc
    return _zone_of_region(region), str(uf).upper(), _direction_of_uf(uf)


FONTES = {"mock": route_mock, "api": route_api}


def route(marker_id):
    """(zona, estado, sentido) do marcador, pela fonte de ROUTE_SOURCE."""
    try:
        fonte = FONTES[config.ROUTE_SOURCE]
    except KeyError:
        raise RoutingError(
            f"ROUTE_SOURCE = '{config.ROUTE_SOURCE}' invalido; use um de {sorted(FONTES)}"
        ) from None
    return fonte(marker_id)
