#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_SORTING

// Separacao por zona: sensor IR e empurrador. O empurrador e uma junta da
// tabela de Joints (nome da zona) com interpolador proprio, mais rapido que
// o do braco e independente dele: se move mesmo com o braco no meio de uma
// sequencia. Velocidade validada no module-tester (PUSHER_STEP_DELAY_MS).
//
// Fluxo por caixinha:
//   prep  -> pre-posicao do sentido decidido (PUSHER_PRE_*)
//   arm   -> sensor armado com o sentido
//   DET   -> o firmware move o empurrador sozinho para PUSHER_PUSH_*,
//            emite "DET <zona>"; empurra, assenta, volta a pre-posicao do
//            sentido e, assentado de novo, emite "PUSHED <zona>"
//
// O PC decide o sentido e le os eventos; nunca esta no caminho critico.

namespace Sorting {
  void begin();
  int  zoneByName(const char* name);   // -1 se nao reconhecer
  int  zoneOfJoint(int joint);         // -1 se a junta nao for empurrador

  // Movimentos comandados: interpolados; o protocolo responde OK quando
  // poll() devolver CMD_DONE. So um comandado por vez.
  void moveTo(int zone, int angle);
  void prep(int zone, bool cw);
  void push(int zone, bool cw);
  void rest(int zone);
  bool moving();                       // ha movimento comandado em curso
  void abort();                        // interrompe o comandado, registra onde parou

  bool arm(int zone, bool cw);         // arma o sensor com o sentido, uma deteccao;
                                       // false se o sensor ja esta em obstaculo
  void disarm(int zone);
  void disarmAll();                    // offall: nada pode empurrar sozinho depois

  enum Event : uint8_t { NONE, CMD_DONE };
  Event poll();                        // sensor, interpoladores, DET e PUSHED
}

#endif
