#include "sorting.h"

#if ENABLE_SORTING

namespace {

struct Zone {
  const char* name;
  uint8_t     irPin;
  int         joint;    // indice da junta do empurrador na tabela de Joints
};

Zone zones[] = {
  { "norte", PIN_IR_NORTE, ARM_JOINTS + 0 },
};
const int NZ = sizeof(zones) / sizeof(zones[0]);

bool          armed[NZ];
unsigned long lowSince[NZ];

}  // namespace

void Sorting::begin() {
  for (int z = 0; z < NZ; z++) {
    pinMode(zones[z].irPin, INPUT);
    armed[z]    = false;
    lowSince[z] = 0;
  }
}

int Sorting::zoneByName(const char* name) {
  for (int z = 0; z < NZ; z++) if (!strcmp(name, zones[z].name)) return z;
  return -1;
}

int Sorting::pusherJoint(int zone) { return zones[zone].joint; }

void Sorting::arm(int zone) {
  armed[zone]    = true;
  lowSince[zone] = 0;
}

// Uma deteccao por 'arm': o orquestrador rearma quando quiser a proxima.
// Debounce simples: LOW continuo por IR_DEBOUNCE_MS.
void Sorting::pollSensor() {
  for (int z = 0; z < NZ; z++) {
    if (!armed[z]) continue;
    if (digitalRead(zones[z].irPin) != LOW) { lowSince[z] = 0; continue; }
    if (lowSince[z] == 0) { lowSince[z] = millis(); continue; }
    if (millis() - lowSince[z] >= IR_DEBOUNCE_MS) {
      armed[z] = false;
      Serial.print(F("DET ")); Serial.println(zones[z].name);
    }
  }
}

#endif
