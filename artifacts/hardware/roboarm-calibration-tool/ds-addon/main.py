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
    L2 (segurando)     desativa os limites calibrados; vale so a faixa 0-180
    Quadrado           alterna a garra entre o minimo e o maximo calibrados
    Triangulo          garra volta ao centro, somente se estiver no maximo

Centros e limites vem do firmware na conexao ('home' + 'dump'); a tabela de
calibracao vive so no sketch.
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
AXIS_L2 = 4
AXIS_R2 = 5

BTN_SQUARE = 2
BTN_TRIANGLE = 3
BTN_OPTIONS = 6
BTN_L1 = 9
BTN_R1 = 10

# Deflexao minima para valer como acionado. A intensidade nao importa: o passo
# e sempre o mesmo, o analogico so diz a direcao.
DEADZONE = 0.5

# Os gatilhos repousam em -1.0 e vao a +1.0 quando pressionados.
TRIGGER_HELD = 0.0

# Faixa que o firmware aceita. Vale quando L2 desativa os limites calibrados.
HARD_RANGE = (0, 180)

# ---------------------------------------------------------------- operacao

# Ordem de energizacao e pausa apos cada junta.
HOMING = [(BASE, 1.0), (HEIGHT, 1.0), (REACH, 1.0), (GRIPPER, 3.0)]

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

    # Sem firmware para consultar, o modo seco precisa de uma tabela propria
    # para exercitar o clamp. Nao precisa acompanhar o sketch: e um cenario.
    CONFIG = {
        BASE: (98, 18, 178),
        HEIGHT: (91, 16, 136),
        REACH: (96, 36, 176),
        GRIPPER: (86, 81, 91),
    }

    def __enter__(self):
        print("  MODO SECO: nenhum comando chega ao Arduino.")
        return self

    def __exit__(self, *exc):
        pass

    def read_config(self):
        print("  -> home / dump  (tabela ficticia do modo seco)")
        return dict(self.CONFIG)

    def home(self):
        print("  -> home")

    def release_all(self):
        print("  -> offall")

    def move(self, joint, angle, expect_motion=True):
        print(f"  -> mv {joint} {angle}")
        return angle


class Session:
    def __init__(self, link, joystick, config):
        self.link = link
        self.joy = joystick

        # config: {junta: (centro, min, max)}, lido do firmware.
        self.start_pose = {j: c for j, (c, _, _) in config.items()}
        self.limits = {j: (lo, hi) for j, (_, lo, hi) in config.items()}
        # Garra: minimo abre, maximo fecha, centro alivia sem abrir.
        self.gripper_rest, self.gripper_open, self.gripper_shut = config[GRIPPER]

        self.angles = dict(self.start_pose)
        self.live = False               # servos energizados
        self.next_gripper = self.gripper_open
        self.pending_gripper = None
        self.limits_off = False         # L2 segurado
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
        return self.joy.get_axis(AXIS_R2) > TRIGGER_HELD

    def l2_held(self):
        return self.joy.get_axis(AXIS_L2) > TRIGGER_HELD

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
        print("   " + "".join(f"  {j} {a}" for j, a in self.start_pose.items()))
        print("  Energizar uma junta a puxa a forca ate o angulo declarado.")
        print()

        # 'home' declara as quatro juntas nos centros; sem isso o firmware
        # recusa 'mv' por nao saber onde elas estao. Ja foi feito ao ler a
        # configuracao, mas repetir custa nada e cobre um reset no intervalo.
        self.link.home()

        for joint, pause in HOMING:
            # O destino e igual ao declarado por 'home', entao o firmware
            # energiza sem deslocamento e nao sinaliza fim de movimento.
            if not self.send(joint, self.start_pose[joint], expect_motion=False):
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
                self.gripper_shut if target == self.gripper_open
                else self.gripper_open
            )
            return target

        if self.pressed(BTN_TRIANGLE):
            # So tem efeito com a garra fechada; alivia a pressao sem abrir.
            if self.angles[GRIPPER] == self.gripper_shut:
                self.next_gripper = self.gripper_open
                return self.gripper_rest
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
        """Aplica o passo parando exatamente no limite.

        Com L2 segurado o limite calibrado e ignorado e vale so a faixa do
        firmware, que por sua vez avisa ('~~') mas obedece. E o caminho para
        refinar um limite pelo controle, e tambem para passar de um.
        """
        low, high = HARD_RANGE if self.l2_held() else self.limits[joint]
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
        print("  L2 segurado desativa os limites calibrados.")
        print("  Ctrl+C encerra a qualquer momento.")
        print()

        while True:
            pygame.event.pump()
            now = time.monotonic()

            # Anuncia a transicao: com --quiet o aviso '~~' do firmware nao
            # aparece, e o operador precisa saber que esta sem rede.
            limits_off = self.l2_held()
            if limits_off != self.limits_off:
                self.limits_off = limits_off
                print("  LIMITES DESATIVADOS (L2)" if limits_off
                      else "  limites ativos")

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
            config = link.read_config()
            Session(link, joystick, config).run()
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
