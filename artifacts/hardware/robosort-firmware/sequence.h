#pragma once
#include <Arduino.h>
#include "config.h"

// Sequencia de passos executada um por vez sobre o Motion. Um passo e um
// movimento (junta, angulo) ou uma pausa (SEQ_WAIT, ms). Toda movimentacao
// do firmware passa por aqui, inclusive um 'mv' simples (um passo) e um
// 'push' (dois). A validacao e feita inteira antes de mover: ou a sequencia
// toda e aceita, ou nada anda. Pausas nao bloqueiam: 'stop' age no meio.

#define SEQ_WAIT (-1)

namespace Sequence {
  struct Step { int8_t joint; uint16_t value; };   // joint >= 0: angulo; SEQ_WAIT: ms

  enum Result : uint8_t {
    RUNNING,        // iniciou; o fim chega por update()
    DONE,           // nenhum passo exigia movimento ou espera; ja terminou
    ERR_PCA,        // PCA9685 ausente
    ERR_UNKNOWN,    // junta de posicao desconhecida (badJoint diz qual)
    ERR_LIMIT       // angulo fora dos limites da junta (badJoint diz qual)
  };

  Result start(const Step* steps, int n, int* badJoint);
  bool   active();
  bool   update();       // avanca movimento e pausas; true no ciclo em que a sequencia acaba
  void   cancel();       // abandona; o Motion ja deve ter sido abortado
}
