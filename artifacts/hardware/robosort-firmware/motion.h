#pragma once
#include <Arduino.h>

// Interpolacao smoothstep nao bloqueante. Uma junta por vez: e o unico
// movimento em curso no firmware, seja do braco ou de um empurrador.
// O chamador valida limites e estado antes de start().

namespace Motion {
  void start(int j, int target);
  bool update();      // avanca um subpasso se for hora; true no ciclo em que termina
  bool active();
  int  joint();       // junta em movimento, -1 se nenhuma
  void abort();       // interrompe e registra na junta o angulo onde parou
}
