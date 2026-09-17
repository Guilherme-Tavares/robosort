#pragma once
#include <Arduino.h>
#include "config.h"

// Juntas sobre o PCA9685: cada uma e um canal com nome, alias, limites,
// poses e estado. Com ENABLE_SORTING, os empurradores entram na mesma tabela
// apos o braco, com o nome da zona, e o motion os trata como qualquer junta.

enum JointState : uint8_t {
  JS_FREE,      // solta: sem pulso, posicao desconhecida
  JS_DECLARED,  // posicao declarada por set/home, ainda sem pulso
  JS_LIVE       // energizada: recebendo pulso na posicao corrente
};

namespace Joints {
  // Inicia o PCA9685 e corta os 16 canais. Devolve false se o modulo nao
  // responder no I2C; nesse caso nenhuma outra funcao envia pulso.
  bool begin();
  bool ready();

  int         count();
  int         find(const char* nameOrAlias);   // -1 se nao reconhecer
  const char* name(int j);
  int         minAngle(int j);
  int         maxAngle(int j);
  int         home(int j);
  int         delivery(int j);
  int         stepSize(int j);          // passo de + e -
  void        moveDirect(int j, int ang); // pulso unico no alvo; energiza se preciso
  int         angle(int j);             // so tem sentido se state != JS_FREE
  JointState  state(int j);

  void declare(int j, int ang);   // set: registra a posicao, nao move, nao energiza
  void energize(int j);           // primeiro pulso, ja na posicao declarada: sem salto
  void pulse(int j, int ang);     // envia pulso sem alterar o estado (uso do motion)
  void setAngle(int j, int ang);  // registra a posicao corrente
  void release(int j);            // corta o pulso; posicao passa a desconhecida
  void releaseAll();
}
