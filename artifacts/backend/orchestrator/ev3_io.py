"""Esteira LEGO pelo brick EV3, via USB com ev3_dc e firmware LEGO original.

A esteira e continua: o operador liga, ajusta a velocidade e desliga pelo
console; o ciclo nao a comanda, so exige que esteja ligada.

Reaproveita o que artifacts/backend/mindstorm-py-controller validou em
bancada: Motor(porta, protocol=USB), start_move(speed, direction) e
stop(brake). Rampa de aceleracao porque o arranque brusco derruba a
caixinha.

Sem ENABLE_CONVEYOR, ou com --assume-conveyor, a esteira sai da jogada:
AssumedConveyor nasce "ligada" e 'on'/'off' so mudam o que o orquestrador
assume. Serve quando a esteira e ligada pelo brick ou por outro PC, ou quando
a caixinha e levada a mao ate o sensor.
"""

import config


class ConveyorError(RuntimeError):
    pass


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
        except ImportError as exc:
            raise ConveyorError("ev3_dc nao instalado: pip install ev3_dc (ou use --assume-conveyor)") from exc
        port = getattr(ev3, f"PORT_{config.EV3_PORT}")
        try:
            self.motor = ev3.Motor(port, protocol=ev3.USB)
            self.motor.__enter__()
        except Exception as exc:
            raise ConveyorError(f"EV3 nao conectou: {exc} (ou use --assume-conveyor)") from exc
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
        self.motor.start_move(
            speed=self.speed,
            direction=config.CONVEYOR_DIRECTION,
            ramp_up=config.CONVEYOR_RAMP,
            ramp_down=config.CONVEYOR_RAMP,
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
