// RoboSort: firmware de producao do Arduino Uno.
//
// O PC decide, o Arduino executa. Em producao o orquestrador
// (artifacts/backend/orchestrator) comanda junta a junta e este firmware
// move, le o sensor e confirma. As sequencias de bancada (mv home, mv dest,
// mv area) existem para testar sem o orquestrador, pelo Monitor Serial.
//
// Modulos:
//   config.h    constantes de hardware, limites, poses, valores-guia e flags
//   joints      as juntas sobre o PCA9685
//   motion      interpolacao smoothstep nao bloqueante, uma junta por vez
//   sequence    lista de passos sobre o motion; toda movimentacao passa aqui
//   protocol    parsing serial e respostas
//   sorting     sensor IR por zona                [ENABLE_SORTING]
//
// Garantias herdadas da ferramenta de calibracao (ver README.md).

#include "config.h"
#include "joints.h"
#include "protocol.h"
#if ENABLE_SORTING
#include "sorting.h"
#endif

void setup() {
  Serial.begin(BAUD_RATE);
  bool pcaOk = Joints::begin();     // corta os 16 canais antes de qualquer outra coisa
#if ENABLE_SORTING
  Sorting::begin();
#endif
  Protocol::begin(pcaOk);
}

void loop() {
  Protocol::poll();
}
