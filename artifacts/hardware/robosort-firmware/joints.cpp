#include "joints.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

namespace {

struct Joint {
  const char* name;
  const char* alias;
  uint8_t     channel;
  int         minA;
  int         maxA;
  int         home;
  int         delivery;
  int         step;      // passo de + e -
  int         angle;
  JointState  state;
};

#define ARM_ROW(NAME, ALIAS, CH, J, STEP) \
  { NAME, ALIAS, CH, ARM_MIN[J], ARM_MAX[J], ARM_HOME[J], ARM_DELIVERY[J], STEP, 0, JS_FREE }

// A ordem das quatro primeiras linhas e J_BASE, J_GRIPPER, J_HEIGHT, J_REACH.
// A garra tem curso de poucos graus e folga no meio: passo de 1.
Joint table[] = {
  ARM_ROW("base",    "b",  CH_BASE,    J_BASE,    2),
  ARM_ROW("garra",   "g",  CH_GRIPPER, J_GRIPPER, 1),
  ARM_ROW("altura",  "al", CH_HEIGHT,  J_HEIGHT,  2),
  ARM_ROW("alcance", "ac", CH_REACH,   J_REACH,   2),
#if ENABLE_SORTING
  // Empurradores: movidos pelo interpolador de Sorting, nao pelo Motion.
  // Uma linha por zona, na mesma ordem de ZONE_NAMES; o nome da zona e o
  // nome da junta, entao 'mv norte 90' e 'dump' funcionam sem caso especial.
  #define PUSHER_ROW(NAME, CH) \
    { NAME, nullptr, CH, 0, 180, PUSHER_NEUTRAL, PUSHER_NEUTRAL, 2, 0, JS_FREE }
  PUSHER_ROW(Z1_NAME, Z1_CH),
  PUSHER_ROW(Z2_NAME, Z2_CH),
  PUSHER_ROW(Z3_NAME, Z3_CH),
  PUSHER_ROW(Z4_NAME, Z4_CH),
  PUSHER_ROW(Z5_NAME, Z5_CH),
#endif
};
const int N = sizeof(table) / sizeof(table[0]);

Adafruit_PWMServoDriver pwm(PCA_ADDR);
bool pcaOk = false;

}  // namespace

bool Joints::begin() {
  pcaOk = pwm.begin();
  if (!pcaOk) return false;
  // Um reset do Arduino (inclusive o de abrir o Monitor Serial) nao reseta o
  // PCA9685: sem este corte, os canais continuariam pulsando na ultima
  // posicao com o firmware, recem-reiniciado, sem saber.
  for (int ch = 0; ch < 16; ch++) pwm.setPin(ch, 0);
  pwm.setOscillatorFrequency(OSC_FREQ);
  pwm.setPWMFreq(PWM_FREQ_HZ);
  return true;
}

bool Joints::ready() { return pcaOk; }
int  Joints::count() { return N; }

int Joints::find(const char* s) {
  for (int j = 0; j < N; j++) {
    if (!strcmp(s, table[j].name)) return j;
    if (table[j].alias && !strcmp(s, table[j].alias)) return j;
  }
  return -1;
}

const char* Joints::name(int j)     { return table[j].name; }
int         Joints::minAngle(int j) { return table[j].minA; }
int         Joints::maxAngle(int j) { return table[j].maxA; }
int         Joints::home(int j)     { return table[j].home; }
int         Joints::delivery(int j) { return table[j].delivery; }
int         Joints::stepSize(int j) { return table[j].step; }
int         Joints::angle(int j)    { return table[j].angle; }
JointState  Joints::state(int j)    { return table[j].state; }

void Joints::declare(int j, int ang) {
  table[j].angle = ang;
  table[j].state = JS_DECLARED;
}

void Joints::pulse(int j, int ang) {
  if (!pcaOk) return;
  pwm.writeMicroseconds(table[j].channel, map(ang, 0, 180, PULSE_MIN_US, PULSE_MAX_US));
}

// No PCA9685 nao existe attach(): o primeiro pulso e a energizacao, e sai
// ja na largura da posicao declarada. E isso que garante a ausencia de salto.
void Joints::energize(int j) {
  pulse(j, table[j].angle);
  table[j].state = JS_LIVE;
}

void Joints::setAngle(int j, int ang) { table[j].angle = ang; }

// Um pulso so, sem interpolacao. Se a junta estava solta, este pulso e a
// energizacao. Uso: primeira energizacao do empurrador, que nao tem de onde
// interpolar; sem carga, um eventual salto e inofensivo.
void Joints::moveDirect(int j, int ang) {
  pulse(j, ang);
  table[j].angle = ang;
  table[j].state = JS_LIVE;
}

void Joints::release(int j) {
  if (pcaOk && table[j].state == JS_LIVE) pwm.setPin(table[j].channel, 0);
  table[j].state = JS_FREE;
}

void Joints::releaseAll() {
  for (int j = 0; j < N; j++) release(j);
}
