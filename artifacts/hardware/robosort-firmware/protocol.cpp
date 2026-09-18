#include "protocol.h"
#include "config.h"
#include "joints.h"
#include "motion.h"
#include "sequence.h"
#if ENABLE_SORTING
#include "sorting.h"
#endif

namespace {

char line[LINE_MAX];
byte lineLen = 0;

int active = -1;   // junta que recebe + e -; definida por mv/sel
#if ENABLE_SORTING
bool awaitingPusher = false;   // ha um comando do empurrador cujo OK sai quando ele chegar
#endif

// ------------------------------------------------------------- respostas

void ok() { Serial.println(F("OK")); }

void err(const __FlashStringHelper* why) {
  Serial.print(F("ERR ")); Serial.println(why);
}

void errJoint(const __FlashStringHelper* why, int j) {
  Serial.print(F("ERR ")); Serial.print(why);
  Serial.print(' '); Serial.println(Joints::name(j));
}

void errLimit(int j) {
  Serial.print(F("ERR limite ")); Serial.print(Joints::name(j));
  Serial.print(' '); Serial.print(Joints::minAngle(j));
  Serial.print(F("..")); Serial.println(Joints::maxAngle(j));
}

// STATE <junta> <ang|?> <estado> <min> <max> <home> <delivery>
void stateLine(int j) {
  Serial.print(F("STATE ")); Serial.print(Joints::name(j)); Serial.print(' ');
  if (Joints::state(j) == JS_FREE) Serial.print('?'); else Serial.print(Joints::angle(j));
  Serial.print(' ');
  switch (Joints::state(j)) {
    case JS_FREE:     Serial.print(F("solta"));      break;
    case JS_DECLARED: Serial.print(F("declarada"));  break;
    case JS_LIVE:     Serial.print(F("energizada")); break;
  }
  Serial.print(' '); Serial.print(Joints::minAngle(j));
  Serial.print(' '); Serial.print(Joints::maxAngle(j));
  Serial.print(' '); Serial.print(Joints::home(j));
  Serial.print(' '); Serial.println(Joints::delivery(j));
}

// CORNER <k> <base> <altura> <alcance>  x4, depois APPROACH <altura> <alcance>
void cornerLines() {
  for (int k = 0; k < CORNERS; k++) {
    Serial.print(F("CORNER ")); Serial.print(k);
    Serial.print(' '); Serial.print(CORNER_BASE[k]);
    Serial.print(' '); Serial.print(CORNER_HEIGHT[k]);
    Serial.print(' '); Serial.println(CORNER_REACH[k]);
  }
  Serial.print(F("APPROACH ")); Serial.print(APPROACH_HEIGHT);
  Serial.print(' '); Serial.println(APPROACH_REACH);
  Serial.print(F("DROP ")); Serial.print(DROP_BASE);
  Serial.print(' '); Serial.print(DROP_REACH);
  Serial.print(' '); Serial.println(DROP_HEIGHT);
}

// Linhas '#' sao para o operador; o orquestrador as ignora.
void help() {
  Serial.println(F("# comandos"));
  Serial.println(F("#   mv <j> <ang>     move ate <ang>"));
  Serial.println(F("#   mv <j>           mostra a junta e a torna ativa"));
  Serial.println(F("#   mv home          vai a HOME: base, altura, alcance, garra"));
  Serial.println(F("#   mv dest          vai a DELIVERY (alcance, altura, base), avanca a DROP (base, alcance, altura); garra abre, fecha, repousa"));
  Serial.println(F("#   mv area <0-3>    pega a caixinha no canto: base, garra abre, aproxima, desce, fecha"));
  Serial.println(F("#   sel <j>          torna a junta ativa e a mostra"));
  Serial.println(F("#   + / -            move a junta ativa (garra 1 grau, demais 2)"));
  Serial.println(F("#   set <j> <ang>    declara a posicao atual (0-180); nao move, nao energiza"));
  Serial.println(F("#   home / dest      declara as juntas em HOME / DELIVERY e as mostra"));
  Serial.println(F("#   off [<j>]        solta a junta, ou a ativa"));
  Serial.println(F("#   offall           solta todas (panico)"));
  Serial.println(F("#   stop             interrompe o movimento, mantem energizado"));
  Serial.println(F("#   dump / ?         STATE de todas as juntas (+ GRIPPER aberta fechada) / da ativa"));
  Serial.println(F("#   corners          valores-guia dos cantos, aproximacao e soltura (DROP)"));
  Serial.println(F("#   ping             OK"));
  Serial.println(F("#   help / h         esta ajuda"));
#if ENABLE_SORTING
  Serial.println(F("#   prep <zona> <cw|ccw>   empurrador na pre-posicao do sentido"));
  Serial.println(F("#   arm <zona> <cw|ccw>    arma o sensor; na deteccao empurra sozinho (DET, PUSHED); ERR sensor se ja ha obstaculo"));
  Serial.println(F("#   push <zona> <cw|ccw>   empurrao manual"));
  Serial.println(F("#   rest / disarm <zona>   empurrador ao neutro / desarma o sensor"));
#endif
  Serial.println(F("# juntas: base|b  garra|g  altura|al  alcance|ac"));
  Serial.println(F("# STATE <junta> <ang|?> <estado> <min> <max> <home> <delivery>"));
}

// --------------------------------------------------------------- parsing

bool startsWith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

char* skipSpaces(char* s) { while (*s == ' ') s++; return s; }

// Le um inteiro em 0..hi. false se nao for numero ou estiver fora.
bool parseInt(const char* s, int hi, int* out) {
  char* end;
  long v = strtol(s, &end, 10);
  if (end == s || *end != '\0') return false;
  if (v < 0 || v > hi) return false;
  *out = (int)v;
  return true;
}

// Separa "<a> [<b>]" no espaco: termina <a> no lugar e devolve <b> ou "".
char* splitArg(char* s) {
  char* sp = strchr(s, ' ');
  if (!sp) return s + strlen(s);
  *sp = '\0';
  return skipSpaces(sp + 1);
}

// ----------------------------------------------------------- sequencias

bool busy() {
#if ENABLE_SORTING
  if (awaitingPusher) return true;
#endif
  return Sequence::active();
}

// Interrompe o que estiver em curso e responde ao comando que o iniciou.
void interrupt() {
  if (!busy()) return;
  Motion::abort();
  Sequence::cancel();
#if ENABLE_SORTING
  Sorting::abort();
  awaitingPusher = false;
#endif
  err(F("interrompido"));
}

// Inicia uma sequencia e responde de imediato se ela falhar na validacao
// ou nao exigir movimento. Caso contrario o OK sai em poll().
void run(const Sequence::Step* s, int n) {
  int bad;
  switch (Sequence::start(s, n, &bad)) {
    case Sequence::ERR_PCA:     err(F("pca"));                    break;
    case Sequence::ERR_UNKNOWN: errJoint(F("desconhecida"), bad); break;
    case Sequence::ERR_LIMIT:   errLimit(bad);                    break;
    case Sequence::DONE:        ok();                             break;
    case Sequence::RUNNING:                                       break;
  }
}

#if ENABLE_SORTING
// Depois de um moveTo/prep/push/rest: OK quando o empurrador chegar; se nao
// houve movimento (ja estava la, ou primeira energizacao direta), OK ja.
void awaitPusher() {
  awaitingPusher = Sorting::moving();
  if (!awaitingPusher) ok();
}
#endif

void moveOne(int j, int ang) {
#if ENABLE_SORTING
  int z = Sorting::zoneOfJoint(j);
  if (z >= 0) {
    if (ang < Joints::minAngle(j) || ang > Joints::maxAngle(j)) { errLimit(j); return; }
    if (!Joints::ready()) { err(F("pca")); return; }
    Sorting::moveTo(z, ang);
    awaitPusher();
    return;
  }
#endif
  Sequence::Step s[] = { { (int8_t)j, (uint16_t)ang } };
  run(s, 1);
}

// mv home: base, altura, alcance, garra.
void moveHome() {
  Sequence::Step s[] = {
    { J_BASE,    (uint16_t)Joints::home(J_BASE)    },
    { J_HEIGHT,  (uint16_t)Joints::home(J_HEIGHT)  },
    { J_REACH,   (uint16_t)Joints::home(J_REACH)   },
    { J_GRIPPER, (uint16_t)Joints::home(J_GRIPPER) },
  };
  run(s, 4);
}

// mv dest: alcance, altura, base ate DELIVERY; base, alcance, altura ate
// DROP; garra abre, 1 s, fecha, 1 s, repousa. Leva a caixinha a esteira e a
// solta, deixando a garra fechada em repouso.
void moveDelivery() {
  Sequence::Step s[] = {
    { J_REACH,   (uint16_t)Joints::delivery(J_REACH)   },
    { J_HEIGHT,  (uint16_t)Joints::delivery(J_HEIGHT)  },
    { J_BASE,    (uint16_t)Joints::delivery(J_BASE)    },
    { J_BASE,    DROP_BASE                             },
    { J_REACH,   DROP_REACH                            },
    { J_HEIGHT,  DROP_HEIGHT                           },
    { J_GRIPPER, GRIPPER_OPEN                          },
    { SEQ_WAIT,  GRIP_CLOSE_DELAY_MS                    },
    { J_GRIPPER, GRIPPER_CLOSED                        },
    { SEQ_WAIT,  GRIP_HOLD_DELAY_MS                     },
    { J_GRIPPER, (uint16_t)Joints::delivery(J_GRIPPER) },
  };
  run(s, 11);
}

// mv area <k>: base do canto, garra aberta, aproximacao (altura, alcance),
// descida ao canto (altura, alcance), pausa, garra fechada, pausa. A ordem
// altura-antes-de-alcance e a protecao contra o acoplamento do pantografo.
// A primeira pausa deixa o braco assentar antes de pegar; a segunda segura
// o OK, e com ele o proximo comando, ate a garra ter firmado a caixinha.
void moveArea(int k) {
  Sequence::Step s[] = {
    { J_BASE,    (uint16_t)CORNER_BASE[k]              },
    { J_GRIPPER, GRIPPER_OPEN                          },
    { J_HEIGHT,  APPROACH_HEIGHT                      },
    { J_REACH,   APPROACH_REACH                       },
    { J_HEIGHT,  (uint16_t)CORNER_HEIGHT[k]            },
    { J_REACH,   (uint16_t)CORNER_REACH[k]             },
    { SEQ_WAIT,  GRIP_CLOSE_DELAY_MS                   },
    { J_GRIPPER, GRIPPER_CLOSED                        },
    { SEQ_WAIT,  GRIP_HOLD_DELAY_MS                    },
  };
  run(s, 9);
}

// Declara as juntas numa pose fixa (home/dest) e as mostra. Vale se o braco
// estiver de fato nela; juntas ja energizadas sao mantidas.
void declarePose(bool delivery) {
  for (int j = 0; j < Joints::count(); j++) {
    if (Joints::state(j) != JS_LIVE) {
      Joints::declare(j, delivery ? Joints::delivery(j) : Joints::home(j));
    }
    stateLine(j);
  }
  ok();
}

// -------------------------------------------------------------- comandos

void handle(char* cmd) {
  if (cmd[0] == '\0') return;

  // Aceitos a qualquer momento, inclusive em movimento.
  if (!strcmp(cmd, "ping"))   { ok(); return; }
  if (!strcmp(cmd, "stop"))   { interrupt(); ok(); return; }
  if (!strcmp(cmd, "offall")) {
    interrupt();
#if ENABLE_SORTING
    Sorting::disarmAll();
#endif
    Joints::releaseAll();
    active = -1;
    ok();
    return;
  }

  if (!strcmp(cmd, "off") || startsWith(cmd, "off ")) {
    bool named = cmd[3] == ' ';
    int j = named ? Joints::find(skipSpaces(cmd + 4)) : active;
    if (j < 0) { err(named ? F("junta") : F("ativa")); return; }
    if (Motion::joint() == j) interrupt();
    Joints::release(j);
    ok();
    return;
  }

  // Tudo abaixo exige o braco parado: uma resposta por comando, na ordem.
  if (busy()) { err(F("ocupado")); return; }

  if (!strcmp(cmd, "help") || !strcmp(cmd, "h")) { help(); ok(); return; }
  if (!strcmp(cmd, "corners")) { cornerLines(); ok(); return; }
  if (!strcmp(cmd, "home"))    { declarePose(false); return; }
  if (!strcmp(cmd, "dest"))    { declarePose(true);  return; }

  if (!strcmp(cmd, "dump")) {
    for (int j = 0; j < Joints::count(); j++) stateLine(j);
    Serial.print(F("GRIPPER ")); Serial.print(GRIPPER_OPEN);
    Serial.print(' '); Serial.println(GRIPPER_CLOSED);
    ok();
    return;
  }

  if (!strcmp(cmd, "?")) {
    if (active < 0) { err(F("ativa")); return; }
    stateLine(active);
    ok();
    return;
  }

  // set aceita 0-180 mesmo fora dos limites: e a unica forma de declarar
  // uma junta que caiu e traze-la de volta com um mv para dentro da faixa.
  if (startsWith(cmd, "set ")) {
    char* name = skipSpaces(cmd + 4);
    char* arg  = splitArg(name);
    int j = Joints::find(name);
    int ang;
    if (j < 0) { err(F("junta")); return; }
    if (!parseInt(arg, 180, &ang)) { err(F("angulo")); return; }
    if (Joints::state(j) == JS_LIVE) { err(F("energizada")); return; }
    Joints::declare(j, ang);
    ok();
    return;
  }

  if (startsWith(cmd, "sel ")) {
    int j = Joints::find(skipSpaces(cmd + 4));
    if (j < 0) { err(F("junta")); return; }
    active = j;
    stateLine(j);
    ok();
    return;
  }

  // mv <j> <ang> move; mv <j> mostra e torna ativa; mv home/dest/area <k>
  // executam as sequencias de bancada.
  if (startsWith(cmd, "mv ")) {
    char* name = skipSpaces(cmd + 3);
    char* arg  = splitArg(name);

    if (!strcmp(name, "home")) { moveHome();     return; }
    if (!strcmp(name, "dest")) { moveDelivery(); return; }
    if (!strcmp(name, "area")) {
      int k;
      if (!parseInt(arg, CORNERS - 1, &k)) { err(F("canto")); return; }
      moveArea(k);
      return;
    }

    int j = Joints::find(name);
    if (j < 0) { err(F("junta")); return; }
    active = j;
    if (*arg == '\0') { stateLine(j); ok(); return; }
    int ang;
    if (!parseInt(arg, 180, &ang)) { err(F("angulo")); return; }
    moveOne(j, ang);
    return;
  }

  if (!strcmp(cmd, "+") || !strcmp(cmd, "-")) {
    if (active < 0) { err(F("ativa")); return; }
    if (Joints::state(active) == JS_FREE) { errJoint(F("desconhecida"), active); return; }
    int d = Joints::stepSize(active);
    moveOne(active, Joints::angle(active) + (cmd[0] == '+' ? d : -d));
    return;
  }

#if ENABLE_SORTING
  // prep/push/rest movem o empurrador pelo interpolador de Sorting e
  // respondem OK na chegada; arm/disarm respondem na hora. Na deteccao, o
  // empurrao sai do proprio firmware, mesmo com o braco em movimento; so os
  // comandos obedecem ao contrato (ERR ocupado durante movimento).
  if (startsWith(cmd, "prep ") || startsWith(cmd, "push ") || startsWith(cmd, "arm ")) {
    char* zone = skipSpaces(strchr(cmd, ' ') + 1);
    char* dir  = splitArg(zone);
    int z = Sorting::zoneByName(zone);
    if (z < 0) { err(F("zona")); return; }
    bool cw;
    if      (!strcmp(dir, "cw"))  cw = true;
    else if (!strcmp(dir, "ccw")) cw = false;
    else { err(F("sintaxe")); return; }
    if (cmd[0] == 'a') {
      if (Sorting::arm(z, cw)) ok(); else err(F("sensor"));
      return;
    }
    if (!Joints::ready()) { err(F("pca")); return; }
    if (cmd[1] == 'r') Sorting::prep(z, cw); else Sorting::push(z, cw);
    awaitPusher();
    return;
  }

  if (startsWith(cmd, "rest ") || startsWith(cmd, "disarm ")) {
    int z = Sorting::zoneByName(skipSpaces(strchr(cmd, ' ') + 1));
    if (z < 0) { err(F("zona")); return; }
    if (cmd[0] == 'd') { Sorting::disarm(z); ok(); return; }
    if (!Joints::ready()) { err(F("pca")); return; }
    Sorting::rest(z);
    awaitPusher();
    return;
  }
#endif

  err(F("comando"));
}

// Le a serial caractere a caractere. Nunca bloqueia: readStringUntil()
// congelaria o loop por ate 1 s e o 'stop' chegaria atrasado.
void readSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      while (lineLen > 0 && line[lineLen - 1] == ' ') lineLen--;
      line[lineLen] = '\0';
      handle(line);
      lineLen = 0;
      return;                     // uma linha por passagem; o movimento segue
    }
    if (lineLen < LINE_MAX - 1) line[lineLen++] = c;
  }
}

}  // namespace

void Protocol::begin(bool pcaOk) {
  if (pcaOk) Serial.println(F("READY"));
  else       err(F("pca"));
}

void Protocol::poll() {
  readSerial();

  if (Sequence::update()) ok();

#if ENABLE_SORTING
  if (Sorting::poll() == Sorting::CMD_DONE && awaitingPusher) {
    awaitingPusher = false;
    ok();
  }
#endif
}
