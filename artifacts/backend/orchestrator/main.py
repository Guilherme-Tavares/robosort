"""Orquestrador do RoboSort: console de operacao.

    python main.py [--port COMx] [--camera K] [--echo] [--no-camera] [--assume-conveyor]

Conecta ao firmware, le a configuracao do braco, energiza em HOME e abre o
console. Os ciclos rodam numa thread propria; o console continua
respondendo.

Comandos:
    on / off       liga e desliga a esteira (continua, velocidade CONVEYOR_SPEED)
    vel N          velocidade da esteira em %
    start          modo automatico: ciclo a cada caixinha vista, com a esteira ligada
    pause          para de iniciar ciclos (o atual termina)
    resume         retoma o automatico
    cycle N        um ciclo com ID forcado, ignorando a camera
    status         esteira, modo, ciclos feitos
    stop           interrompe, volta a HOME, solta o braco e sai

--assume-conveyor (ou ENABLE_CONVEYOR = False): a esteira sai da jogada e e
assumida ligada, pelo brick, por outro PC ou a mao; 'on'/'off' so mudam a
suposicao. --no-camera: sem visao; so 'cycle N'.
"""

import argparse
import sys

import config
from ev3_io import ConveyorError, open_conveyor
from orchestrator import Arm, Runner
from serial_io import AckTimeout, Arduino, ArduinoError, CommandError, JOINTS
from vision import Vision, VisionError

HELP = """  on / off    esteira        vel N     velocidade %
  start       automatico     pause     para de iniciar ciclos     resume  retoma
  cycle N     um ciclo com ID N, sem camera
  status      estado         stop      interrompe, HOME, solta e sai"""


def show_config(cfg):
    print("  juntas (min..max, home, delivery):")
    for j in JOINTS:
        i = cfg.joints[j]
        print(f"    {j:8s} {i.min:3d}..{i.max:<3d}  home {i.home:3d}  delivery {i.delivery:3d}")
    print(f"  garra: aberta {cfg.gripper['open']} fechada {cfg.gripper['closed']}")
    print("  cantos (base, altura, alcance):")
    for k in sorted(cfg.corners):
        c = cfg.corners[k]
        print(f"    [{k}] {c['base']:3d} {c['altura']:3d} {c['alcance']:3d}")
    print(f"  aproximacao: altura {cfg.approach['altura']} alcance {cfg.approach['alcance']}")
    print(f"  soltura: base {cfg.drop['base']} alcance {cfg.drop['alcance']} altura {cfg.drop['altura']}")
    flags = [f for f in ("ENABLE_VISION", "ENABLE_LOCALIZATION", "ENABLE_CONVEYOR", "ENABLE_SORTING")
             if getattr(config, f)]
    print(f"  flags ativas: {', '.join(flags) or 'nenhuma'}; canto fixo {config.FIXED_CORNER}")


def console(link, arm, runner, conveyor):
    print()
    print(HELP)
    print()
    while True:
        try:
            line = input("> ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            print()
            line = "stop"
        if not line:
            continue
        word, _, arg = line.partition(" ")

        if word == "on":
            conveyor.start()
        elif word == "off":
            conveyor.stop()
        elif word == "vel":
            if not arg.isdigit() or not 1 <= int(arg) <= 100:
                print("  uso: vel N (1-100)")
            else:
                conveyor.set_speed(int(arg))
        elif word in ("start", "resume"):
            if runner.vision is None:
                print("  sem camera: o automatico nao tem como identificar; use 'cycle N'")
            else:
                runner.auto = True
                print("  automatico ligado" + ("" if conveyor.running else " (esperando 'on')"))
        elif word == "pause":
            runner.auto = False
            print("  automatico pausado" + ("; o ciclo atual termina" if runner.busy else ""))
        elif word == "cycle":
            if not arg.isdigit():
                print("  uso: cycle N")
            else:
                runner.requests.put(int(arg))
                print(f"  ciclo com id {arg} enfileirado")
        elif word == "status":
            print(f"  esteira {'ligada' if conveyor.running else 'desligada'} a {conveyor.speed}%; "
                  f"automatico {'on' if runner.auto else 'off'}; "
                  f"{'em ciclo' if runner.busy else 'parado'}; {runner.cycles} ciclos")
            if runner.failed:
                print(f"  ultima falha: {runner.failed}")
        elif word in ("stop", "q", "quit", "exit"):
            return
        elif word in ("h", "help", "?"):
            print(HELP)
        else:
            print("  comando invalido; 'help'")


def main():
    parser = argparse.ArgumentParser(description="Orquestrador do RoboSort")
    parser.add_argument("--port", help="porta serial do Arduino (ex: COM5)")
    parser.add_argument("--camera", type=int, help="indice da camera")
    parser.add_argument("--no-camera", action="store_true", help="sem visao; so 'cycle N'")
    parser.add_argument("--assume-conveyor", action="store_true",
                        help="sem EV3: esteira assumida ligada (brick, outro PC ou a mao)")
    parser.add_argument("--echo", action="store_true", help="ecoa o trafego serial")
    args = parser.parse_args()

    use_camera = config.ENABLE_VISION and not args.no_camera
    print()
    try:
        with Arduino(args.port, echo=args.echo) as link:
            print(f"  firmware pronto em {link.port}")
            cfg = link.read_config()
            show_config(cfg)
            if config.ENABLE_SORTING and config.ZONE not in cfg.joints:
                # Com sorting, o 'dump' lista o empurrador como junta da zona.
                print(f"!! firmware sem separacao: 'dump' nao trouxe a junta '{config.ZONE}'. "
                      f"Grave-o com ENABLE_SORTING 1 (config.h) ou desligue ENABLE_SORTING aqui.")
                return 1
            arm = Arm(link, cfg)

            vision = None
            if use_camera:
                try:
                    vision = Vision(args.camera)
                    print(f"  camera {vision.camera_index}")
                except VisionError as exc:
                    print(f"  sem camera ({exc}); so 'cycle N'")

            try:
                with open_conveyor(args.assume_conveyor) as conveyor:
                    arm.energize_home()
                    runner = Runner(arm, link, vision, conveyor)
                    runner.start()
                    try:
                        console(link, arm, runner, conveyor)
                    finally:
                        # stop: interrompe o que estiver em curso, espera a
                        # thread soltar a serial, leva a HOME e solta.
                        runner.quit()
                        runner.auto = False
                        if runner.busy:
                            print("  interrompendo o ciclo")
                            link.stop()
                        runner.join(timeout=config.ACK_TIMEOUT + 2)
                        if config.ENABLE_SORTING:
                            try:
                                link.disarm(config.ZONE)
                            except (CommandError, AckTimeout):
                                pass
                        arm.safe_stop()
                        if conveyor.running:
                            conveyor.stop()
            except (CommandError, AckTimeout, ValueError) as exc:
                print(f"!! {exc}")
                if isinstance(exc, AckTimeout):
                    link.resync()
                arm.safe_stop()
                return 1
            finally:
                if vision:
                    vision.close()
    except (ArduinoError, ConveyorError) as exc:
        print(f"!! {exc}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
