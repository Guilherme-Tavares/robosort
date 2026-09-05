"""Operacao do braco de calibracao por DualSense.

Traduz o controle em comandos do firmware de calibracao. Nao substitui o
Monitor Serial: 'min', 'max' e 'dump' continuam sendo feitos por la.

Uso:
    python main.py                 conecta ao Arduino e opera
    python main.py --port COM5     forca a porta serial
    python main.py --dry-run       sem Arduino; so imprime os comandos
    python main.py --quiet         nao ecoa o trafego serial

Controles:
    Options            liga os servos e leva as juntas as posicoes iniciais
    R2 + Options       solta todos os servos e encerra
    Analogico esq (X)  base: esquerda aumenta, direita diminui
    Analogico dir (Y)  altura: cima aumenta, baixo diminui
    R1 + analog dir    alcance: baixo aumenta, cima diminui
    L1 (segurando)     modo de precisao: passo de 1 grau, 200 ms entre comandos
    Quadrado           alterna a garra entre 88 e 96
    Triangulo          garra volta a 92, somente se estiver em 96
"""

import argparse
import os
import sys
import time

os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

import pygame

from arm import ArmLink, ArmError, BASE, HEIGHT, REACH, GRIPPER

# --------------------------------------------------------------- mapeamento
# Confirmado em bancada com DualSense por Bluetooth (SDL 2.32, pygame-ce).

AXIS_LEFT_X = 0
AXIS_RIGHT_Y = 3
AXIS_R2 = 5

BTN_SQUARE = 2
BTN_TRIANGLE = 3
BTN_OPTIONS = 6
BTN_L1 = 9
BTN_R1 = 10

# Deflexao minima para valer como acionado. A intensidade nao importa: o passo
# e sempre o mesmo, o analogico so diz a direcao.
DEADZONE = 0.5

# R2 repousa em -1.0 e vai a +1.0 quando pressionado.
R2_HELD = 0.0

# ------------------------------------------------------------------ limites

LIMITS = {
    BASE: (18, 178),
    HEIGHT: (16, 136),
    REACH: (56, 176),
}

START_POSE = {BASE: 98, HEIGHT: 91, REACH: 116, GRIPPER: 92}

# Ordem de energizacao e pausa apos cada junta.
HOMING = [(BASE, 1.0), (HEIGHT, 1.0), (REACH, 1.0), (GRIPPER, 3.0)]

GRIPPER_OPEN = 88
GRIPPER_SHUT = 96
GRIPPER_REST = 92

STEP_NORMAL = 2
STEP_FINE = 1

# Espera entre comandos no modo de precisao. Sem ela o L1 so mudaria a
# granularidade: a cadencia por confirmacao ja e proporcional ao percurso,
# entao 1 grau sai na mesma velocidade angular que 2.
FINE_INTERVAL = 0.200

# Duracao do movimento no firmware: SUBSTEPS(4) x STEP_DELAY(8 ms) por grau.
# Serve de piso de cadencia contado a partir do envio, nao da confirmacao.
# No modo real a espera pela confirmacao ja consome esse tempo e o piso nao
# atrasa nada; no modo seco e ele que impede o laco de varrer o curso inteiro
# num piscar. Tambem protege caso a serial responda mais rapido que o previsto.
SECONDS_PER_DEGREE = 0.032

POLL = 0.02


class DryLink:
    """Substitui o ArmLink quando se roda sem o Arduino."""

    def __enter__(self):
        print("  MODO SECO: nenhum comando chega ao Arduino.")
        return self

    def __exit__(self, *exc):
        pass

    def home(self):
        print("  -> home")

    def release_all(self):
        print("  -> offall")

    def move(self, joint, angle, expect_motion=True):
        print(f"  -> mv {joint} {angle}")
        return angle


class Session:
    def __init__(self, link, joystick):
        self.link = link
        self.joy = joystick
        self.angles = dict(START_POSE)
        self.live = False               # servos energizados
        self.next_gripper = GRIPPER_OPEN
        self.pending_gripper = None
        self.next_command_at = 0.0
        self.turn = 0                   # rodizio entre analogicos
        self.prev_buttons = {}

    # --------------------------------------------------------- utilitarios

    def pressed(self, index):
        """True apenas na transicao de solto para pressionado."""
        now = bool(self.joy.get_button(index))
        before = self.prev_buttons.get(index, False)
        self.prev_buttons[index] = now
        return now and not before

    def held(self, index):
        return bool(self.joy.get_button(index))

    def r2_held(self):
        return self.joy.get_axis(AXIS_R2) > R2_HELD

    def send(self, joint, angle, expect_motion=True):
        try:
            self.angles[joint] = self.link.move(joint, angle, expect_motion)
            return True
        except ArmError as exc:
            print(f"  !! {exc}")
            return False

    # ------------------------------------------------------------- inicio

    def start(self):
        print()
        print("  Confirme que o braco esta nas posicoes iniciais:")
        print("    base 98   altura 91   alcance 116   garra 92")
        print("  Energizar uma junta a puxa a forca ate o angulo declarado.")
        print()

        # 'home' declara as quatro juntas nos centros; sem isso o firmware
        # recusa 'mv' por nao saber onde elas estao.
        self.link.home()

        for joint, pause in HOMING:
            # O destino e igual ao declarado por 'home', entao o firmware
            # energiza sem deslocamento e nao sinaliza fim de movimento.
            if not self.send(joint, START_POSE[joint], expect_motion=False):
                print("  !! falha na energizacao; abortando")
                return False
            time.sleep(pause)

        self.live = True
        print()
        print("  Pronto. Controle liberado.")
        print()
        return True

    def shutdown(self):
        print()
        print("  Soltando todos os servos.")
        print("  Apos soltar, as juntas ficam presas so pelo atrito da")
        print("  reducao, que cede sob carga. Ampare o braco se estiver")
        print("  estendido.")
        self.link.release_all()
        self.live = False

    # ------------------------------------------------------------ operacao

    def gripper_commands(self):
        """Botoes da garra. Devolve o angulo pedido, ou None."""
        if self.pressed(BTN_SQUARE):
            target = self.next_gripper
            self.next_gripper = (
                GRIPPER_SHUT if target == GRIPPER_OPEN else GRIPPER_OPEN
            )
            return target

        if self.pressed(BTN_TRIANGLE):
            # So tem efeito com a garra fechada; alivia a pressao sem abrir.
            if self.angles[GRIPPER] == GRIPPER_SHUT:
                self.next_gripper = GRIPPER_OPEN
                return GRIPPER_REST
        return None

    def jog_request(self):
        """Le os analogicos. Devolve (junta, passo) ou None."""
        step = STEP_FINE if self.held(BTN_L1) else STEP_NORMAL
        wanted = []

        left_x = self.joy.get_axis(AXIS_LEFT_X)
        if abs(left_x) > DEADZONE:
            # esquerda aumenta, direita diminui
            wanted.append((BASE, step if left_x < 0 else -step))

        right_y = self.joy.get_axis(AXIS_RIGHT_Y)
        if abs(right_y) > DEADZONE:
            down = right_y > 0
            if self.held(BTN_R1):
                # alcance: baixo aumenta, cima diminui
                wanted.append((REACH, step if down else -step))
            else:
                # altura: cima aumenta, baixo diminui
                wanted.append((HEIGHT, -step if down else step))

        if not wanted:
            return None

        # Com os dois analogicos ativos, alterna para nenhum ficar parado.
        self.turn += 1
        return wanted[self.turn % len(wanted)]

    def clamp(self, joint, delta):
        """Aplica o passo parando exatamente no limite."""
        low, high = LIMITS[joint]
        return max(low, min(high, self.angles[joint] + delta))

    def pace(self, sent_at, degrees):
        """Define quando o proximo comando pode sair.

        Contado a partir do envio: no modo real a espera pela confirmacao ja
        consumiu esse tempo e o piso nao acrescenta atraso.
        """
        gap = degrees * SECONDS_PER_DEGREE
        if self.held(BTN_L1):
            gap = max(gap, FINE_INTERVAL)
        self.next_command_at = sent_at + gap

    # ---------------------------------------------------------------- loop

    def run(self):
        print("  Options liga os servos.  R2 + Options encerra.")
        print("  Ctrl+C encerra a qualquer momento.")
        print()

        while True:
            pygame.event.pump()
            now = time.monotonic()

            start_pressed = self.pressed(BTN_OPTIONS)

            if start_pressed and self.r2_held():
                if self.live:
                    self.shutdown()
                return

            if start_pressed and not self.live:
                if not self.start():
                    return
                continue

            # Os botoes da garra sao lidos em toda passagem: se so fossem
            # amostrados quando ha permissao de envio, um toque curto durante
            # a espera do modo de precisao se perderia.
            target = self.gripper_commands()
            if target is not None:
                self.pending_gripper = target

            if not self.live or now < self.next_command_at:
                time.sleep(POLL)
                continue

            if self.pending_gripper is not None:
                target = self.pending_gripper
                self.pending_gripper = None
                if target != self.angles[GRIPPER]:
                    travel = abs(target - self.angles[GRIPPER])
                    sent_at = time.monotonic()
                    self.send(GRIPPER, target)
                    self.pace(sent_at, travel)
                continue

            request = self.jog_request()
            if request is None:
                time.sleep(POLL)
                continue

            joint, delta = request
            angle = self.clamp(joint, delta)
            if angle == self.angles[joint]:
                # No limite: nao ha comando a enviar.
                time.sleep(POLL)
                continue

            travel = abs(angle - self.angles[joint])
            sent_at = time.monotonic()
            self.send(joint, angle)
            self.pace(sent_at, travel)


def open_joystick():
    pygame.init()
    pygame.joystick.init()
    if pygame.joystick.get_count() == 0:
        print("!! Nenhum controle encontrado.")
        print("   Pareie o DualSense (Create + PS) ou conecte o cabo USB-C.")
        return None
    joystick = pygame.joystick.Joystick(0)
    print(f"  controle: {joystick.get_name()}")
    return joystick


def main():
    parser = argparse.ArgumentParser(description="Braco de calibracao por DualSense")
    parser.add_argument("--port", help="porta serial do Arduino (ex: COM5)")
    parser.add_argument("--dry-run", action="store_true",
                        help="nao abre a serial; so imprime os comandos")
    parser.add_argument("--quiet", action="store_true",
                        help="nao ecoa o trafego serial")
    args = parser.parse_args()

    print()
    joystick = open_joystick()
    if joystick is None:
        return 1

    link = DryLink() if args.dry_run else ArmLink(args.port, echo=not args.quiet)

    try:
        with link:
            Session(link, joystick).run()
    except ArmError as exc:
        print(f"!! {exc}")
        return 1
    except KeyboardInterrupt:
        print()
        print("  Interrompido. Os servos seguem energizados.")
        print("  Use R2 + Options, ou 'offall' no Monitor Serial, para soltar.")
    finally:
        pygame.quit()

    return 0


if __name__ == "__main__":
    sys.exit(main())
