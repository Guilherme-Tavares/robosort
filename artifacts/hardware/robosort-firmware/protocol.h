#pragma once
#include <Arduino.h>

// Protocolo serial: parsing de linha e respostas.
//
// Contrato: cada comando recebe exatamente uma resposta terminal, OK ou
// ERR <motivo>, na ordem em que foi enviado. Linhas STATE e '#' que
// precedem o OK pertencem ao mesmo comando. Com movimento em curso, so
// stop, off, offall e ping sao aceitos; o resto recebe ERR ocupado. Um
// comando que interrompe o movimento faz o 'mv' pendente responder
// ERR interrompido antes da propria resposta. DET e a unica linha fora
// desse fluxo: assincrona, prefixada, pode chegar a qualquer momento.

namespace Protocol {
  void begin(bool pcaOk);   // emite READY, ou ERR pca se o modulo nao respondeu
  void poll();              // le a serial, avanca o movimento, emite respostas
}
