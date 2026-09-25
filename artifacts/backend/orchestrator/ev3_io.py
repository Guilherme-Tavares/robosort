"""Esteira LEGO pelo brick EV3, via USB com ev3_dc e firmware LEGO original.

A esteira e continua: o operador liga, ajusta a velocidade e desliga pelo
console; o ciclo nao a comanda, so exige que esteja ligada.

Reaproveita o que artifacts/backend/mindstorm-py-controller validou em
bancada: Motor(porta, protocol=USB), start_move(speed, direction,
ramp_up_time) e stop(brake). Rampa de aceleracao porque o arranque brusco
derruba a caixinha; a parada (opOutput_Stop) e imediata.

No Windows o EV3 e um dispositivo HID (driver HidUsb da Microsoft) e o
pyusb/libusb, que o ev3_dc usa fora do macOS, nao consegue escrever nele
(Errno 5). O ev3_dc ja tem um caminho por hidapi, so que preso ao macOS;
_use_hidapi() liga esse caminho no Windows sem trocar driver (sem Zadig) e
sem quebrar o software LEGO.

No Linux o caminho nativo do ev3_dc e pyusb/libusb. O acesso sem root vem da
regra config/udev/99-robosort.rules; falhas de backend ou permissao recebem
uma orientacao especifica na abertura da esteira.

Sem ENABLE_CONVEYOR, ou com --assume-conveyor, a esteira sai da jogada:
AssumedConveyor nasce "ligada" e 'on'/'off' so mudam o que o orquestrador
assume. Serve quando a esteira e ligada pelo brick ou por outro PC, ou quando
a caixinha e levada a mao ate o sensor.
"""

import platform

import config


class ConveyorError(RuntimeError):
    pass


def _ajuda_linux_usb():
    if platform.system() != "Linux":
        return ""
    return (
        " No Linux, instale o libusb e as regras udev de config/udev/99-robosort.rules; "
        "depois reconecte o EV3."
    )


def _use_hidapi(ev3_module):
    """Faz o ev3_dc falar com o EV3 por hidapi no Windows. O modulo decide
    pyusb x hidapi consultando platform.system() == 'Darwin' em tempo de
    execucao; trocamos o 'platform' e o 'hid' que ele enxerga. O hidapi no
    Windows exige o report ID (0x00) na frente de cada escrita e ja o
    descarta na leitura; no macOS o ev3_dc escreve sem ele."""
    if platform.system() != "Windows":
        return
    try:
        import hid
    except ImportError as exc:
        raise ConveyorError("hidapi nao instalado: pip install hidapi") from exc

    class HidDevice(hid.device):
        def write(self, data):
            return super().write(b"\x00" + bytes(data))

    class HidShim:
        enumerate = staticmethod(hid.enumerate)
        device = HidDevice

    class DarwinShim:
        @staticmethod
        def system():
            return "Darwin"

    ev3_module.hid = HidShim
    ev3_module.platform = DarwinShim


class AssumedConveyor:
    """Sem controle real. Nasce ligada; 'on' e 'off' apenas registram o que
    o operador diz que a esteira esta fazendo."""

    def __init__(self, log=print):
        self.log = log
        self.running = True
        self.speed = config.CONVEYOR_SPEED

    def __enter__(self):
        self.log("  esteira: fora do orquestrador; assumida ligada (brick, outro PC ou a mao)")
        return self

    def __exit__(self, *exc):
        pass

    def start(self):
        self.running = True
        self.log("  esteira: assumida ligada (sem controle pelo EV3)")

    def stop(self):
        self.running = False
        self.log("  esteira: assumida desligada")

    def set_speed(self, speed):
        self.speed = speed
        self.log(f"  esteira: velocidade assumida {speed}%")


class Conveyor:
    def __init__(self, log=print):
        self.log = log
        self.motor = None
        self.running = False
        self.speed = config.CONVEYOR_SPEED

    def __enter__(self):
        try:
            import ev3_dc as ev3
            import ev3_dc.ev3 as ev3_module
        except ImportError as exc:
            raise ConveyorError("ev3_dc nao instalado: pip install ev3_dc (ou use --assume-conveyor)") from exc
        _use_hidapi(ev3_module)
        port = getattr(ev3, f"PORT_{config.EV3_PORT}")
        try:
            self.motor = ev3.Motor(port, protocol=ev3.USB)
            self.motor.__enter__()
        except Exception as exc:
            raise ConveyorError(
                f"EV3 nao conectou: {exc}.{_ajuda_linux_usb()} "
                "Use --assume-conveyor para operar sem o controle USB."
            ) from exc
        self.log("  EV3 conectado")
        return self

    def __exit__(self, *exc):
        if self.motor:
            try:
                self.motor.stop(brake=True)
            finally:
                self.motor.__exit__(*exc)
            self.motor = None
        self.running = False

    def _apply(self):
        """start_move nao aceita movimento em curso; para trocar a
        velocidade com a esteira ligada, solta e rearranca com rampa."""
        if self.running:
            self.motor.stop(brake=False)
        self.motor.start_move(
            speed=self.speed,
            direction=config.CONVEYOR_DIRECTION,
            ramp_up_time=config.CONVEYOR_RAMP_TIME,
        )

    def start(self):
        self._apply()
        self.running = True
        self.log(f"  esteira: ligada a {self.speed}%")

    def stop(self):
        self.motor.stop(brake=True)
        self.running = False
        self.log("  esteira: desligada")

    def set_speed(self, speed):
        self.speed = speed
        if self.running:
            self._apply()
        self.log(f"  esteira: velocidade {speed}%")


def open_conveyor(assume=False, log=print):
    if assume or not config.ENABLE_CONVEYOR:
        return AssumedConveyor(log)
    return Conveyor(log)
