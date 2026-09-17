#include "sequence.h"
#include "joints.h"
#include "motion.h"

namespace {

Sequence::Step steps[SEQ_MAX_STEPS];
int  count   = 0;
int  index   = 0;
bool running = false;

bool          waiting   = false;
unsigned long waitStart = 0;
uint16_t      waitMs    = 0;

// Avanca a partir de 'index' ate encontrar um passo que exija movimento ou
// espera, ou ate o fim. Movimentos cujo destino ja e a posicao corrente sao
// pulados; uma junta declarada e energizada no caminho, sem salto.
bool advance() {
  while (index < count) {
    int j = steps[index].joint;
    int v = steps[index].value;
    if (j == SEQ_WAIT) {
      waiting   = true;
      waitStart = millis();
      waitMs    = v;
      return false;
    }
    if (Joints::state(j) == JS_DECLARED) Joints::energize(j);
    if (v != Joints::angle(j)) {
      Motion::start(j, v);
      return false;
    }
    index++;
  }
  running = false;
  return true;
}

}  // namespace

Sequence::Result Sequence::start(const Step* s, int n, int* badJoint) {
  if (!Joints::ready()) return ERR_PCA;
  for (int i = 0; i < n; i++) {
    int j = s[i].joint;
    if (j == SEQ_WAIT) continue;
    int a = s[i].value;
    if (Joints::state(j) == JS_FREE) { *badJoint = j; return ERR_UNKNOWN; }
    if (a < Joints::minAngle(j) || a > Joints::maxAngle(j)) { *badJoint = j; return ERR_LIMIT; }
  }
  for (int i = 0; i < n; i++) steps[i] = s[i];
  count   = n;
  index   = 0;
  running = true;
  waiting = false;
  return advance() ? DONE : RUNNING;
}

bool Sequence::active() { return running; }

bool Sequence::update() {
  if (!running) return false;
  if (waiting) {
    if (millis() - waitStart < waitMs) return false;
    waiting = false;
    index++;
    return advance();
  }
  if (Motion::update()) {
    index++;
    return advance();
  }
  return false;
}

void Sequence::cancel() {
  running = false;
  waiting = false;
  count   = 0;
  index   = 0;
}
