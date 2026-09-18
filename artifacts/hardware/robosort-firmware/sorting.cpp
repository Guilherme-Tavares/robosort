#include "sorting.h"

#if ENABLE_SORTING

#include "joints.h"

namespace {

struct Zone {
  const char* name;
  uint8_t     irPin;
  int         joint;      // indice da junta do empurrador na tabela de Joints
  int         preCw, preCcw, pushCw, pushCcw;
};

Zone zones[] = {
  { "norte", PIN_IR_NORTE, ARM_JOINTS + 0,
    PUSHER_PRE_CW, PUSHER_PRE_CCW, PUSHER_PUSH_CW, PUSHER_PUSH_CCW },
};
const int NZ = sizeof(zones) / sizeof(zones[0]);

// Interpolador por zona, igual ao do motion mas com velocidade propria.
struct Mover {
  bool          moving;
  bool          autonomous;   // disparado pelo sensor: no fim, PUSHED em vez de OK
  int           from, to, step, total;
  unsigned long lastAt;
  unsigned long arrivedAt;    // 0 = nada pendente apos a chegada
  unsigned long detectedAt;   // DET emitido, empurrao ainda nao iniciado (0 = nenhum)
  int           returnTo;     // empurrao autonomo: pre-posicao a que volta (-1 = nao volta)
};

Mover         mover[NZ];
bool          armed[NZ];
bool          armedCw[NZ];
unsigned long lowSince[NZ];
int           commanded = -1;   // zona do movimento comandado em curso

int eased(const Mover& m, int s) {
  float t = (float)s / m.total;
  float e = t * t * (3.0f - 2.0f * t);
  return m.from + (int)((m.to - m.from) * e);
}

void start(int z, int target, bool autonomous) {
  int j = zones[z].joint;
  Mover& m = mover[z];
  if (Joints::state(j) != JS_LIVE) {
    // Ainda sem pulso (solta, ou so declarada por 'home'): nao ha de onde
    // interpolar. Declara no alvo e energiza ali, num pulso so, em vez de
    // energizar na posicao declarada e interpolar a partir dela; sem carga,
    // um eventual salto e inofensivo, e evita o vaivem neutro -> alvo.
    Joints::moveDirect(j, target);
    m.moving = false;
    return;
  }
  m.from       = Joints::angle(j);
  m.to         = target;
  m.total      = abs(m.to - m.from) * PUSHER_SUBSTEPS;
  if (m.total == 0) m.total = 1;
  m.step       = 0;
  m.lastAt     = millis();
  m.moving     = true;
  m.autonomous = autonomous;
}

}  // namespace

void Sorting::begin() {
  for (int z = 0; z < NZ; z++) {
    pinMode(zones[z].irPin, INPUT);
    armed[z]    = false;
    lowSince[z] = 0;
    mover[z].moving     = false;
    mover[z].arrivedAt  = 0;
    mover[z].detectedAt = 0;
    mover[z].returnTo   = -1;
  }
}

int Sorting::zoneByName(const char* name) {
  for (int z = 0; z < NZ; z++) if (!strcmp(name, zones[z].name)) return z;
  return -1;
}

int Sorting::zoneOfJoint(int joint) {
  for (int z = 0; z < NZ; z++) if (zones[z].joint == joint) return z;
  return -1;
}

void Sorting::moveTo(int zone, int angle) {
  start(zone, angle, false);
  commanded = mover[zone].moving ? zone : -1;
}

void Sorting::prep(int zone, bool cw) { moveTo(zone, cw ? zones[zone].preCw  : zones[zone].preCcw); }
void Sorting::push(int zone, bool cw) { moveTo(zone, cw ? zones[zone].pushCw : zones[zone].pushCcw); }
void Sorting::rest(int zone)          { moveTo(zone, PUSHER_NEUTRAL); }

bool Sorting::moving() { return commanded >= 0; }

void Sorting::abort() {
  if (commanded < 0) return;
  Mover& m = mover[commanded];
  Joints::setAngle(zones[commanded].joint, eased(m, m.step));
  m.moving  = false;
  commanded = -1;
}

// Recusa armar com o sensor ja em obstaculo: senao o empurrao sai no mesmo
// loop(), antes de a caixinha existir, e o ciclo segue como se tivesse
// dado certo. Sensor permanentemente em LOW e sensibilidade alta demais
// (ve a esteira) ou pino solto; melhor acusar na hora.
bool Sorting::arm(int zone, bool cw) {
  if (digitalRead(zones[zone].irPin) == LOW) return false;
  armed[zone]    = true;
  armedCw[zone]  = cw;
  lowSince[zone] = 0;
  return true;
}

void Sorting::disarm(int zone) { armed[zone] = false; }
void Sorting::disarmAll()      { for (int z = 0; z < NZ; z++) armed[z] = false; }

// Avanca os interpoladores, le os sensores, emite DET e PUSHED. Na deteccao
// o empurrao comeca aqui mesmo, no mesmo loop(), sem esperar o PC: a zona
// fica no comeco da esteira e a caixinha nao espera.
Sorting::Event Sorting::poll() {
  unsigned long now = millis();
  Event ev = NONE;

  for (int z = 0; z < NZ; z++) {
    Mover& m = mover[z];
    int j = zones[z].joint;

    if (m.moving && now - m.lastAt >= PUSHER_STEP_DELAY_MS) {
      m.lastAt = now;
      m.step++;
      Joints::pulse(j, eased(m, m.step));
      if (m.step >= m.total) {
        Joints::pulse(j, m.to);
        Joints::setAngle(j, m.to);
        m.moving = false;
        if (m.autonomous) m.arrivedAt = now ? now : 1;
        else if (commanded == z) { commanded = -1; ev = CMD_DONE; }
      }
    }

    // Chegou: se foi a ida do empurrao, segura PUSHER_HOLD_MS e volta a
    // pre-posicao; se foi a volta, assenta PUSHER_SETTLE_MS e PUSHED. O
    // empurrador termina onde comecou, pronto para o proximo 'prep' do mesmo
    // sentido sem se mover.
    unsigned long wait = m.returnTo >= 0 ? PUSHER_HOLD_MS : PUSHER_SETTLE_MS;
    if (m.arrivedAt && now - m.arrivedAt >= wait) {
      m.arrivedAt = 0;
      if (m.returnTo >= 0) {
        int back = m.returnTo;
        m.returnTo = -1;
        start(z, back, true);
        if (!m.moving) m.arrivedAt = now ? now : 1;
      } else {
        Serial.print(F("PUSHED ")); Serial.println(zones[z].name);
      }
    }

    // Latencia entre DET e empurrao: a caixinha ainda anda do sensor ate a
    // frente do empurrador.
    if (m.detectedAt && now - m.detectedAt >= PUSHER_DET_DELAY_MS) {
      m.detectedAt = 0;
      if (commanded == z) commanded = -1;       // o empurrao autonomo prevalece
      start(z, armedCw[z] ? zones[z].pushCw : zones[z].pushCcw, true);
      m.returnTo = armedCw[z] ? zones[z].preCw : zones[z].preCcw;
      if (!m.moving) m.arrivedAt = now ? now : 1; // ja estava la
    }

    if (!armed[z]) continue;
    if (digitalRead(zones[z].irPin) != LOW) { lowSince[z] = 0; continue; }
    if (lowSince[z] == 0) { lowSince[z] = now; continue; }
    if (now - lowSince[z] >= IR_DEBOUNCE_MS) {
      armed[z] = false;
      m.detectedAt = now ? now : 1;
      Serial.print(F("DET ")); Serial.println(zones[z].name);
    }
  }
  return ev;
}

#endif
