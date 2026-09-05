"""Diagnostico de mapeamento do DualSense.

Nao fala com o Arduino. Serve para conferir os indices de eixo e botao que o
SDL expoe nesta maquina, que mudam entre USB e Bluetooth.

Uso:
    python diag.py            visao ao vivo de tudo que estiver ativo
    python diag.py --check    verificacao guiada dos controles usados

Ctrl+C encerra.
"""

import os
import sys
import time

# Esconde o banner de boas-vindas do pygame antes do import.
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame

# Limiar para considerar um eixo "mexido". Os gatilhos repousam em -1.0, entao
# a deteccao compara contra a leitura de repouso, nao contra zero.
AXIS_MOVE = 0.30

# Mapeamento confirmado em bancada (DualSense por Bluetooth, SDL 2.32).
# O indice 5 nao foi verificado; e o unico vago da faixa 0-15.
BUTTON_NAMES = {
    0: "X",           1: "Circulo",     2: "Quadrado",   3: "Triangulo",
    4: "Share",       5: "PS?",         6: "Options",    7: "R3",
    8: "L3",          9: "L1",         10: "R1",        11: "Dpad cima",
    12: "Dpad baixo", 13: "Dpad esq",  14: "Dpad dir",  15: "Touchpad",
    16: "Mute",
}

AXIS_NAMES = {
    0: "LX", 1: "LY", 2: "RX", 3: "RY", 4: "L2", 5: "R2",
}


def button_name(i):
    return BUTTON_NAMES.get(i, "NAO MAPEADO")


def axis_name(i):
    return AXIS_NAMES.get(i, "NAO MAPEADO")


def open_controller():
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() == 0:
        print("!! Nenhum controle encontrado.")
        print()
        print("   Bluetooth: segure Create + PS ate a barra piscar, entao")
        print("   pareie pelas configuracoes do Windows.")
        print("   Ou conecte o cabo USB-C e rode de novo.")
        return None

    joystick = pygame.joystick.Joystick(0)

    print()
    print("=" * 62)
    print(f"  Controle: {joystick.get_name()}")
    print(f"  Eixos: {joystick.get_numaxes()}   "
          f"Botoes: {joystick.get_numbuttons()}   "
          f"Hats: {joystick.get_numhats()}")
    print("=" * 62)

    # Avisa se a contagem exceder o que esta mapeado (o Mute, por exemplo).
    extra = joystick.get_numbuttons() - len(BUTTON_NAMES)
    if extra > 0:
        idx = ", ".join(str(len(BUTTON_NAMES) + i) for i in range(extra))
        print(f"  Aviso: botoes sem mapeamento conhecido: {idx}")
    print()

    return joystick


def read_rest(joystick):
    """Leitura de repouso dos eixos. Os gatilhos nao repousam em zero."""
    pygame.event.pump()
    time.sleep(0.1)
    pygame.event.pump()
    return [joystick.get_axis(i) for i in range(joystick.get_numaxes())]


def live_view(joystick):
    rest = read_rest(joystick)

    print("  Mexa um controle de cada vez. Ctrl+C encerra.")
    print()

    last = ""
    while True:
        pygame.event.pump()
        active = []

        for i in range(joystick.get_numaxes()):
            value = joystick.get_axis(i)
            if abs(value - rest[i]) > AXIS_MOVE:
                active.append(f"eixo {i} ({axis_name(i)}) = {value:+.2f}")

        for i in range(joystick.get_numbuttons()):
            if joystick.get_button(i):
                active.append(f"BOTAO {i} ({button_name(i)})")

        for i in range(joystick.get_numhats()):
            hat = joystick.get_hat(i)
            if hat != (0, 0):
                active.append(f"hat {i} = {hat}")

        line = "   |   ".join(active) if active else "(parado)"
        if line != last:
            print(f"  {line}")
            last = line

        time.sleep(0.04)


# Controles usados pelo orquestrador, com o indice esperado.
# ("rotulo", tipo, indice esperado, dica de direcao)
CHECKS = [
    ("Analogico ESQUERDO para a ESQUERDA", "axis", 0, "negativo"),
    ("Analogico DIREITO para CIMA",        "axis", 3, "negativo"),
    ("L1  (modo de precisao)",             "button", 9,  None),
    ("R1  (alterna altura/alcance)",       "button", 10, None),
    ("R2  (usado no encerramento)",        "axis", 5, "positivo"),
    ("Quadrado  (alterna a garra)",        "button", 2,  None),
    ("Triangulo (garra para 92)",          "button", 3,  None),
    ("Options   (faz o papel de START)",   "button", 6,  None),
]


def wait_idle(joystick, rest):
    """Espera tudo voltar ao repouso, para uma acao nao satisfazer duas etapas."""
    while True:
        pygame.event.pump()
        busy = any(joystick.get_button(i)
                   for i in range(joystick.get_numbuttons()))
        busy = busy or any(
            abs(joystick.get_axis(i) - rest[i]) > AXIS_MOVE
            for i in range(joystick.get_numaxes())
        )
        if not busy:
            return
        time.sleep(0.03)


def capture(joystick, rest, kind):
    """Bloqueia ate detectar um botao ou eixo. Devolve (indice, valor)."""
    while True:
        pygame.event.pump()

        if kind == "button":
            for i in range(joystick.get_numbuttons()):
                if joystick.get_button(i):
                    return i, None
        else:
            for i in range(joystick.get_numaxes()):
                value = joystick.get_axis(i)
                if abs(value - rest[i]) > AXIS_MOVE:
                    return i, value

        time.sleep(0.03)


def guided_check(joystick):
    rest = read_rest(joystick)

    print("  Verificacao guiada. Acione o que for pedido, um de cada vez.")
    print("  Ctrl+C encerra.")
    print()

    results = []
    for label, kind, expected, hint in CHECKS:
        wait_idle(joystick, rest)

        prompt = f"  -> {label}"
        if hint:
            prompt += f"  ({hint})"
        print(prompt, end="", flush=True)

        index, value = capture(joystick, rest, kind)

        ok = index == expected
        got = f"{'BOTAO' if kind == 'button' else 'eixo'} {index}"
        if value is not None:
            got += f" = {value:+.2f}"

        if ok:
            print(f"   OK  [{got}]")
        else:
            print(f"   DIVERGE  [{got}], esperado {expected}")

        results.append((label, kind, expected, index, ok))

    print()
    print("=" * 62)
    bad = [r for r in results if not r[4]]
    if not bad:
        print("  Mapeamento confere em todos os pontos.")
    else:
        print(f"  {len(bad)} divergencia(s). Ajustar no orquestrador:")
        for label, kind, expected, index, _ in bad:
            print(f"    {label}: esperado {expected}, obtido {index}")
    print("=" * 62)
    print()


def main():
    joystick = open_controller()
    if joystick is None:
        return 1

    try:
        if "--check" in sys.argv:
            guided_check(joystick)
        else:
            live_view(joystick)
        return 0
    except KeyboardInterrupt:
        print()
        print("  Encerrado.")
        return 0
    finally:
        pygame.quit()


if __name__ == "__main__":
    sys.exit(main())
