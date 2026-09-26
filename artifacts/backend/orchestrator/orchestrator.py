"""Ciclo de aquisicao e separacao, comandado junta a junta.

O orquestrador envia um 'mv' por junta, na ordem fixa validada em bancada,
e espera o OK de cada um. As ordens sao as mesmas das sequencias do
firmware (mv area, mv dest, mv home); a diferenca e que aqui o alvo da
aquisicao pode ser interpolado pela visao, em vez de um canto fixo.

run_sorting_cycle e o ciclo desta fase: identifica a caixinha pela camera,
decide o destino pela tabela mockada em config.route() (zona, estado,
sentido), prepara e arma o empurrador da zona correspondente, pega (canto
fixo ou alvo interpolado, conforme ENABLE_LOCALIZATION), entrega na esteira,
volta a HOME e espera o firmware confirmar o empurrao. Runner roda ciclos
numa thread, no automatico ou sob demanda.
"""

import threading
import time
from queue import Empty, Queue

import config
import kinematics
from ev3_io import ConveyorError
from serial_io import JOINTS, AckTimeout, ArduinoError, CommandError
from vision import VisionError

ORDER_HOME = ("base", "altura", "alcance", "garra")


class Arm:
    def __init__(self, link, cfg, log=print):
        self.link = link
        self.cfg = cfg
        self.log = log

    # ----------------------------------------------------------- basico

    def mv(self, joint, angle):
        """Valida contra os limites lidos do firmware antes de enviar. O
        firmware valida de novo; aqui e para o erro aparecer com contexto."""
        lo, hi = self.cfg.limits(joint)
        if not lo <= angle <= hi:
            raise ValueError(f"{joint} {angle} fora de {lo}..{hi}")
        self.log(f"    {joint:8s} -> {angle}")
        self.link.mv(joint, angle)

    def _pose(self, attr):
        return {j: getattr(self.cfg.joints[j], attr) for j in JOINTS}

    # Aberta e fechada vem do firmware como angulos proprios: qual extremo
    # abre depende da montagem do horn, nao dos limites.
    @property
    def gripper_open(self):   return self.cfg.gripper["open"]
    @property
    def gripper_closed(self): return self.cfg.gripper["closed"]

    # --------------------------------------------------------- sequencias

    def energize_home(self):
        """'home' declara as juntas em HOME; um mv de cada uma ao proprio
        home energiza sem deslocamento. O braco precisa estar fisicamente
        em HOME.

        Obrigatorio antes do primeiro movimento: entre offall e a
        energizacao o braco cede pela gravidade, e o primeiro pulso puxa a
        junta de volta ao declarado. Feito aqui, parado, e um acerto de
        poucos graus; feito dentro de uma sequencia, vira um movimento
        abrupto no meio dela."""
        self.log("  home: declarar e energizar")
        self.link.home()
        home = self._pose("home")
        for j in ORDER_HOME:
            self.mv(j, home[j])

    def pick(self, base, altura, alcance):
        """Mesma ordem de 'mv area': base, garra abre, aproximacao (altura,
        alcance), descida (altura, alcance), pausa, garra fecha, pausa."""
        ap = self.cfg.approach
        self.log(f"  pick: base {base} altura {altura} alcance {alcance}")
        self.mv("base", base)
        self.mv("garra", self.gripper_open)
        self.mv("altura", ap["altura"])
        self.mv("alcance", ap["alcance"])
        self.mv("altura", altura)
        self.mv("alcance", alcance)
        time.sleep(config.GRIP_CLOSE_DELAY)
        self.mv("garra", self.gripper_closed)
        time.sleep(config.GRIP_HOLD_DELAY)

    def deliver(self, before_release=None):
        """Mesma ordem de 'mv dest': alcance, altura, base ate DELIVERY;
        altura e alcance ate DROP; garra abre, pausa, fecha, pausa, repousa;
        alcance e altura de volta a DELIVERY, para o home partir de uma
        pose segura. before_release() roda imediatamente antes de a garra
        abrir: e o momento de armar o sensor, ja escutando quando a
        caixinha cai na esteira."""
        d = self._pose("delivery")
        drop = self.cfg.drop
        self.log("  deliver")
        self.mv("alcance", d["alcance"])
        self.mv("altura", d["altura"])
        self.mv("base", d["base"])
        self.mv("altura", drop["altura"])
        self.mv("alcance", drop["alcance"])
        if before_release:
            before_release()
        self.mv("garra", self.gripper_open)
        time.sleep(config.GRIP_CLOSE_DELAY)
        self.mv("garra", self.gripper_closed)
        time.sleep(config.GRIP_HOLD_DELAY)
        self.mv("garra", d["garra"])
        self.mv("alcance", d["alcance"])
        self.mv("altura", d["altura"])

    def go_home(self):
        """Mesma ordem de 'mv home': base, altura, alcance, garra."""
        home = self._pose("home")
        self.log("  home")
        for j in ORDER_HOME:
            self.mv(j, home[j])

    def pick_corner(self, k):
        c = self.cfg.corners[k]
        self.pick(c["base"], c["altura"], c["alcance"])

    # ------------------------------------------------------------ aborto

    def safe_stop(self):
        """Interrompe, tenta voltar a HOME e solta. Cada etapa e tentada
        mesmo se a anterior falhar; um offall com o braco estendido o deixa
        cair, mas e melhor que deixa-lo energizado sem supervisao."""
        self.log("  abortando: stop, home, offall")
        for step in (self.link.stop, self.go_home, self.link.offall):
            try:
                step()
            except (ArduinoError, ValueError) as exc:
                self.log(f"    !! {exc}")


class Aborted(RuntimeError):
    """Ciclo abandonado por condicao esperada (sem caixinha, sem deteccao)."""


def run_sorting_cycle(arm, link, vision, forced_id=None):
    """Um ciclo: identifica, decide o sentido, prepara e arma o empurrador,
    pega, entrega, volta a HOME e espera o firmware confirmar o empurrao.

    A esteira e continua e ja esta ligada. Na deteccao o proprio firmware
    empurra, entao a espera aqui e so pela confirmacao (PUSHED); o proximo
    ciclo depende dela e do braco ter chegado a HOME.
    """
    log = arm.log
    cfg = arm.cfg

    # 1. Identificacao (camera) ou ID forcado pelo console.
    if forced_id is not None:
        pid = forced_id
        log(f"  id forcado: {pid}")
    else:
        pid = vision.identify()
        log(f"  identificado: marcador {pid}")

    # 2. Roteamento mockado: marcador -> zona (regiao), estado, sentido.
    zone, state, direction = config.route(pid)
    log(f"  destino: {state} ({zone}) -> empurrador {direction}")

    # 3. Empurrador declarado e energizado na pre-posicao do sentido, com um
    #    tempo para assentar, antes de o braco se mover. O sensor so e armado
    #    na entrega, imediatamente antes de a garra abrir (ver deliver):
    #    antes disso nada deve passar por ele.
    before_release = None
    if config.ENABLE_SORTING:
        link.drain_events()
        link.prep(zone, direction)
        time.sleep(config.PREP_SETTLE_DELAY)
        log(f"  {zone}: empurrador na pre-posicao ({direction})")

        def before_release():
            link.arm(zone, direction)
            log(f"  {zone}: sensor armado ({direction})")

    # 4. Aquisicao e entrega.
    log(f"  {config.DELAY_BEFORE_PICK:.0f} s para a caixinha estar no lugar")
    time.sleep(config.DELAY_BEFORE_PICK)
    if config.ENABLE_LOCALIZATION:
        _, x, y = vision.locate(pid)
        target = kinematics.target(cfg.corners, x, y)
        log(f"  localizada em ({x:.2f}, {y:.2f}) cm -> {target}")
        arm.pick(**target)
    else:
        arm.pick_corner(config.FIXED_CORNER)
    arm.deliver(before_release)
    arm.go_home()

    # 5. Confirmacao do empurrao. O DET pode ter chegado durante o home.
    if config.ENABLE_SORTING:
        if link.wait_event("PUSHED", zone, config.DET_TIMEOUT) is None:
            link.disarm(zone)
            raise Aborted(f"sem deteccao em {config.DET_TIMEOUT:.0f} s; sensor desarmado")
        log(f"  {zone}: empurrou ({direction})")
    log(f"ciclo concluido: caixinha {pid} -> {state}")
    return pid, state


class Runner(threading.Thread):
    """Executa ciclos numa thread propria, para o console continuar
    respondendo. Modo automatico: liga a esteira sozinho antes de checar a
    camera, roda um ciclo atras do outro enquanto houver caixinha, e desliga
    a esteira ao final de cada ciclo. 'cycle N' enfileira um ciclo com ID
    forcado, com ou sem automatico, sem mexer na esteira."""

    def __init__(self, arm, link, vision, conveyor, log=print):
        super().__init__(name="runner", daemon=True)
        self.arm, self.link, self.vision, self.conveyor, self.log = arm, link, vision, conveyor, log
        self.auto = False
        self.requests = Queue()          # IDs forcados de 'cycle N'
        self.busy = False
        self.cycles = 0
        self.failed = None               # ultima falha do braco, se houver
        self._quit = threading.Event()
        self._warned = set()

    def quit(self):
        self._quit.set()

    def _warn_once(self, key, msg):
        if key not in self._warned:
            self._warned.add(key)
            self.log(msg)

    def _next(self):
        """(id_forcado | None) se ha ciclo a rodar agora; senao levanta Empty."""
        try:
            return self.requests.get(timeout=0.2)
        except Empty:
            pass
        if not self.auto:
            raise Empty
        if self.vision is None:
            self._warn_once("cam", "  automatico: sem camera; use 'cycle N'")
            raise Empty
        if not self.conveyor.running:
            self.conveyor.start()
            self.log(f"  automatico: esteira ligada a {self.conveyor.speed}%, procurando caixinha")
        if self.vision.peek() is None:
            raise Empty
        self._warned.clear()
        return None

    def run(self):
        while not self._quit.is_set():
            try:
                forced = self._next()
            except Empty:
                continue
            except ConveyorError as exc:
                self.log(f"!! {exc}")
                self.failed = exc
                self.auto = False
                continue
            if forced is not None and not self.conveyor.running:
                self.log("  aviso: esteira desligada; a caixinha nao vai chegar ao sensor")
            self.busy = True
            try:
                run_sorting_cycle(self.arm, self.link, self.vision, forced)
                self.cycles += 1
            except (VisionError, Aborted) as exc:
                self.log(f"  -- {exc}")
            except CommandError as exc:
                if exc.reason == "interrompido":
                    self.log("  ciclo interrompido")
                else:
                    self.log(f"!! {exc}")
                    self.failed = exc
                self.auto = False
            except (AckTimeout, ValueError, ConveyorError) as exc:
                self.log(f"!! {exc}")
                self.failed = exc
                self.auto = False
            finally:
                self.busy = False
                if forced is None and self.conveyor.running:
                    try:
                        self.conveyor.stop()
                    except ConveyorError as exc:
                        self.log(f"!! {exc}")
                        self.failed = exc
                        self.auto = False
                    self.log("  automatico: esteira desligada")
            self.log("")
