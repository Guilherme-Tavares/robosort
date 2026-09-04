from time import sleep

import ev3_dc as ev3
import usb.core


_original_motor_del = ev3.Motor.__del__


def _safe_motor_del(self):
    try:
        _original_motor_del(self)
    except Exception:
        pass


ev3.Motor.__del__ = _safe_motor_del

PORT = ev3.PORT_A  # Aceita ev3.PORT_A, ev3.PORT_B, ev3.PORT_C ou ev3.PORT_D.
speed = 30  # Aceita inteiros de 1 ate 100; representa porcentagem da velocidade.
direction = 1  # Aceita 1 para horario ou -1 para anti-horario.
is_running = False  # Aceita True para motor ligado ou False para motor desligado.

MIN_SPEED = 10
MAX_SPEED = 100
SPEED_STEP = 10


def print_menu():
    print()
    print("Controle de motor EV3 via USB")
    print("--------------------------------")
    print("l = ligar/desligar")
    print("+ = aumentar velocidade")
    print("- = diminuir velocidade")
    print("d = sentido horario")
    print("e = sentido anti-horario")
    print("s = status")
    print("q = sair")
    print()


def print_status():
    print(
        "Estado:",
        "Ligado" if is_running else "Desligado",
        "| Velocidade:",
        str(speed) + "%",
        "| Direcao:",
        "Horario" if direction == 1 else "Anti-horario",
    )


def apply_motor_state(motor):
    if is_running:
        motor.start_move(speed=speed, direction=direction)
    else:
        motor.stop(brake=True)


def main():
    global speed, direction, is_running

    try:
        usb.core.find()
    except usb.core.NoBackendError:
        print("Erro: o PyUSB nao encontrou um backend USB no Windows.")
        print("Instale/configure o libusb antes de tentar conectar ao EV3.")
        print("Veja o arquivo PASSO_A_PASSO_EV3_USB_PC.md, secao de solucao de problemas.")
        return

    print("Conecte o EV3 ao computador pela porta USB marcada como PC.")
    print("Mantenha o EV3 ligado no sistema original LEGO EV3.")
    print("Tentando conectar...")

    try:
        with ev3.Motor(PORT, protocol=ev3.USB) as motor:
            print("EV3 conectado.")
            print_menu()
            print_status()

            while True:
                command = input("> ").strip().lower()

                if command == "l":
                    is_running = not is_running
                    apply_motor_state(motor)
                    print_status()

                elif command == "+":
                    speed = min(MAX_SPEED, speed + SPEED_STEP)
                    apply_motor_state(motor)
                    print_status()

                    if not is_running:
                        is_running = True
                        apply_motor_state(motor)
                        continue

                elif command == "-":
                    speed = max(MIN_SPEED, speed - SPEED_STEP)
                    apply_motor_state(motor)
                    print_status()

                    if not is_running:
                        is_running = True
                        apply_motor_state(motor)
                        continue

                elif command == "d":
                    direction = 1
                    apply_motor_state(motor)
                    print_status()

                elif command == "e":
                    direction = -1
                    apply_motor_state(motor)
                    print_status()

                elif command == "s":
                    print_status()

                elif command == "q":
                    motor.stop(brake=True)
                    sleep(0.2)
                    print("Motor parado. Programa encerrado.")
                    break

                else:
                    print("Comando invalido.")
                    print_menu()
    except OSError as error:
        print("Nao foi possivel conectar/controlar o EV3.")
        print("Detalhe do erro:", error)
        if getattr(error, "errno", None) == 5:
            print("Provavel causa: driver USB do EV3 incompativel com libusb no Windows.")
            print("Veja no Markdown a secao sobre Errno 5 e instalacao do driver WinUSB/libusbK.")
    except Exception as error:
        print("Nao foi possivel conectar/controlar o EV3.")
        print("Detalhe do erro:", error)


if __name__ == "__main__":
    main()
