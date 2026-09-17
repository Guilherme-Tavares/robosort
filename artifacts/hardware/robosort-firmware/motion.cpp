#include "motion.h"
#include "config.h"
#include "joints.h"

namespace {

bool moving = false;
int  slot   = -1;
int  from   = 0;
int  to     = 0;
int  step   = 0;
int  total  = 0;
unsigned long lastAt = 0;

// smoothstep: 3t^2 - 2t^3. Arranque e chegada suaves.
int eased(int s) {
  float t = (float)s / total;
  float e = t * t * (3.0f - 2.0f * t);
  return from + (int)((to - from) * e);
}

}  // namespace

void Motion::start(int j, int target) {
  slot   = j;
  from   = Joints::angle(j);
  to     = target;
  total  = abs(to - from) * SUBSTEPS;
  if (total == 0) total = 1;     // destino igual a origem: conclui no proximo update
  step   = 0;
  lastAt = millis();
  moving = true;
}

bool Motion::update() {
  if (!moving) return false;
  if (millis() - lastAt < STEP_DELAY) return false;

  lastAt = millis();
  step++;
  Joints::pulse(slot, eased(step));

  if (step >= total) {
    Joints::pulse(slot, to);
    Joints::setAngle(slot, to);
    moving = false;
    return true;
  }
  return false;
}

bool Motion::active() { return moving; }
int  Motion::joint()  { return moving ? slot : -1; }

void Motion::abort() {
  if (!moving) return;
  Joints::setAngle(slot, eased(step));
  moving = false;
}
