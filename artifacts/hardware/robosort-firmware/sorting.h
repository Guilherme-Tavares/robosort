#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_SORTING

// Separacao: sensor IR de presenca por zona. O empurrador de cada zona e uma
// junta comum da tabela de Joints (nome da zona); 'push' e uma Sequence de
// dois passos montada pelo protocolo. Aqui fica so o que e do sensor.

namespace Sorting {
  void begin();
  int  zoneByName(const char* name);   // -1 se nao reconhecer
  int  pusherJoint(int zone);          // indice da junta do empurrador

  void arm(int zone);                  // arma a escuta do sensor, uma deteccao
  void pollSensor();                   // emite "DET <zona>" e desarma
}

#endif
