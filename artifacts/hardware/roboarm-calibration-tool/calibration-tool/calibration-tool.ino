#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Calibracao assistida de braco robotico MDF com quatro servos, acionados
// por um PCA9685 via I2C. Firmware descartavel: existe para mapear limites e
// ensaiar o ciclo de preensao. Varias juntas ficam energizadas ao mesmo tempo;
// uma delas e a ativa, alvo de + e -. Energizacao sem salto, movimento
// interpolado e interrompivel a qualquer instante.
//
// Tambem ensaia a zona de separacao (sensor IR + servo empurrador) com o
// mesmo ciclo do robosort-firmware: 'mv norte <id>' energiza o empurrador na
// pre-posicao do sentido, arma o sensor e, na deteccao, empurra e volta.

#define PCA_ADDR    0x40
#define BASE_CH     1
#define HEIGHT_CH   5
#define REACH_CH    0
#define GRIPPER_CH  4

// Largura de pulso, em microssegundos, para 0 e 180 graus. Sao os valores
// padrao da biblioteca Servo do Arduino; mante-los e o que faz os angulos
// calibrados com a versao anterior deste firmware continuarem validos.
#define PULSE_MIN_US 544
#define PULSE_MAX_US 2400

// O oscilador interno do PCA9685 e nominalmente 25 MHz, mas varia por chip;
// a Adafruit mede algo proximo de 27 MHz. Se o primeiro 'mv' para o centro
// declarado der solavanco, e este valor que esta errado para este modulo.
#define OSC_FREQ    27000000

#define JOINT_COUNT 4
#define J_BASE      0
#define J_GRIPPER   1
#define J_HEIGHT    2
#define J_REACH     3

#define STEP_DELAY  8    // ms entre subpassos
#define SUBSTEPS    4    // subpassos por grau

#define LINE_MAX    32

// ------------------------------------------------------ zonas de separacao
// Espelho de robosort-firmware/config.h (secao Separacao). Ao ajustar la,
// ajuste aqui. Sensor FC-51: LOW = obstaculo. Cada empurrador tem
// interpolador proprio, na velocidade do module-tester (4 ms/grau),
// independente do braco. Ciclo por zona: pre-posicao -> armado -> DET ->
// latencia -> empurrao -> segura -> volta a pre-posicao -> assenta -> PUSHED.
//
// Uma regiao por zona, dois estados por regiao: estado 1 gira anti-horario
// (ccw), estado 2 horario (cw). Comando: 'mv re <1-5> es <1-2>'.
#define ZONE_COUNT         5
#define IR_DEBOUNCE_MS     20
#define PUSHER_STEP_DELAY  4      // ms entre subpassos
#define PUSHER_SUBSTEPS    1      // subpassos por grau
#define PUSHER_DET_DELAY_MS 400  // da deteccao ao inicio do empurrao (caixinha chega ao empurrador)
#define PUSHER_HOLD_MS     1000   // segura o empurrao antes de voltar
#define PUSHER_SETTLE_MS   200    // assentamento na volta, antes do PUSHED

// Regiao 1..5 -> indice 0..4. Pinos IR confirmados em bancada. Canais do
// PCA: so o da zona 1 (Norte, canal 8) esta confirmado; os demais sao
// PROVISORIOS, conferir antes de energizar.
//                             1 Norte  2 Nordeste  3 C-Oeste  4 Sudeste  5 Sul
const char* zoneName[]    = { "norte", "nordeste", "centro-oeste", "sudeste", "sul" };
const uint8_t zoneIrPin[] = {      4,          7,             8,        12,    13 };
const uint8_t zoneCh[]    = {      8,          9,            10,        11,    12 };  // 9-12 A CONFERIR

// Posicoes de cada empurrador. Medidas na zona 1; as demais herdam os mesmos
// valores como ponto de partida, a ajustar com 'mv <zona> <ang>'.
//                             1    2    3    4    5
const int zonePreCw[]     = {   0,   0,   0,   0,   0 };
const int zonePreCcw[]    = { 180, 180, 180, 180, 180 };
const int zonePushCw[]    = { 180, 180, 180, 180, 180 };
const int zonePushCcw[]   = {   0,   0,   0,   0,   0 };

const char* names[]    = {"base", "garra", "altura", "alcance"};
const char* alias1[]   = {"b",    "g",     "al",     "ac"};
const int   channels[] = {BASE_CH, GRIPPER_CH, HEIGHT_CH, REACH_CH};

// ------------------------------------------------------ limites calibrados
// Tabela "Resultados da calibracao" do README, na ordem base, garra, altura,
// alcance. Sao posicoes confortaveis, com margem; nao o ponto do batente.
// Apos cada sessao, copie o 'dump' para ca. -1 em min/max = desconhecido.
// min/max nao bloqueiam movimento (e preciso poder ultrapassar um limite
// para refina-lo), mas geram aviso '~~'.
//                          base  garra  altura  alcance
const int knownMin[]  = {   18,    82,     16,      36 };
const int knownMax[]  = {  178,   120,    136,     176 };

// ------------------------------------------------------------------- poses
// Espelho de robosort-firmware/config.h: mesmos nomes, mesma ordem de
// colunas, para copiar e colar entre os dois. Ajuste aqui com as sequencias
// ('mv home', 'mv dest', 'mv area') e o ajuste fino (+/-), e leve para a
// producao. 'home' e 'dest' declaram as juntas nestas poses.
//                                     base  garra  altura  alcance
const int ARM_HOME[JOINT_COUNT]     = {   18,    82,     91,      96 };
const int ARM_DELIVERY[JOINT_COUNT] = {   92,    82,    101,     102 };

// Garra: aberta e fechada sao angulos proprios, nao os limites.
#define GRIPPER_OPEN      120
#define GRIPPER_CLOSED     82

// DROP: de DELIVERY, altura e alcance avancam ate a soltura; depois voltam.
#define DROP_HEIGHT        92
#define DROP_REACH         98

// Canto 0: unica area de aquisicao por enquanto.
#define CORNER0_BASE       44
#define CORNER0_HEIGHT     29
#define CORNER0_REACH      56

// Aproximacao: altura e alcance antes de descer ao canto.
#define APPROACH_HEIGHT    39
#define APPROACH_REACH     78

// Pausas em torno do fechamento da garra.
#define GRIP_CLOSE_DELAY_MS 1000
#define GRIP_HOLD_DELAY_MS  1000

// Ciclo com '--arm', espelho do orquestrador (config.py).
#define PREP_SETTLE_MS       1000   // empurrador assenta na pre-posicao
#define DELAY_BEFORE_PICK_MS 3000   // operador posiciona a caixinha

// Passo do ajuste fino de + e -.
const int stepSize[]  = {2, 1, 2, 2};

// Garra direta (1) ou interpolada (0). Direta: um pulso so, na velocidade
// do servo, para fechar de uma vez e morder. Interpolada: smoothstep como as
// outras juntas. As demais juntas sao sempre interpoladas: carregam o braco.
#ifndef GRIPPER_DIRECT
#define GRIPPER_DIRECT 0
#endif

const bool direct[]   = {false, GRIPPER_DIRECT, false, false};

Adafruit_PWMServoDriver pwm(PCA_ADDR);
bool  pcaOk = false;

int   angles[JOINT_COUNT];
bool  declaredAt[JOINT_COUNT];
bool  liveAt[JOINT_COUNT];

// Limites registrados pelo operador durante a calibracao.
// -1 significa "ainda nao registrado".
int limitMin[JOINT_COUNT];
int limitMax[JOINT_COUNT];

int current = -1;   // junta que recebe + e -

// Estado do movimento em curso (sempre da junta ativa)
bool moving = false;
int  moveStart  = 0;
int  moveTarget = 0;
int  moveStep   = 0;
int  moveTotal  = 0;
unsigned long lastStepAt = 0;

// Buffer da leitura serial nao bloqueante
char lineBuf[LINE_MAX];
byte lineLen = 0;

// Sequencia de passos (mv home / mv dest / mv area): um por vez, pelo mesmo
// interpolador do 'mv'. joint = SEQ_WAIT e uma pausa de 'value' ms.
#define SEQ_WAIT (-1)
#define SEQ_ARM  (-2)   // arma o sensor da zona em 'value'; instantaneo
#define SEQ_MAX  32
struct SeqStep { int8_t joint; uint16_t value; };
SeqStep seqSteps[SEQ_MAX];
int  seqCount  = 0;
int  seqIndex  = 0;
bool seqActive = false;
unsigned long seqWaitUntil = 0;

// Estado do empurrador e do ciclo, por zona. Independente da junta ativa e
// de 'moving': o braco pode se mover com o sensor armado, como em producao.
// P_READY e a pre-posicao alcancada sem armar: com '--arm', o sensor so
// passa a valer quando a garra abre, no meio da sequencia do braco.
enum PusherPhase { P_IDLE, P_PREP, P_READY, P_ARMED, P_WAITING, P_PUSHING,
                   P_HOLDING, P_RETURNING, P_SETTLING };
PusherPhase pPhase[ZONE_COUNT];
bool pLive[ZONE_COUNT];     // ja recebeu pulso; pAngle e a posicao real
int  pAngle[ZONE_COUNT];
bool pCw[ZONE_COUNT];
int  pPre[ZONE_COUNT];
int  pPush[ZONE_COUNT];
bool pArmOnReady[ZONE_COUNT];  // arma sozinho ao chegar (sem --arm)
bool pMoving[ZONE_COUNT];
int  pFrom[ZONE_COUNT], pTo[ZONE_COUNT], pStep[ZONE_COUNT], pTotal[ZONE_COUNT];
unsigned long pLastAt[ZONE_COUNT];
unsigned long pPhaseAt[ZONE_COUNT];   // inicio da espera (latencia, segura, assenta)
unsigned long pLowSince[ZONE_COUNT];  // debounce do sensor

// ------------------------------------------------------------------- servos

// Envia o pulso correspondente ao angulo. No PCA9685 nao existe attach(): o
// primeiro pulso e a propria energizacao, ja na largura pedida. E isso que
// garante a ausencia de salto.
void writeAngle(int j, int angle) {
  pwm.writeMicroseconds(channels[j], map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US));
}

// Corta o pulso do canal. O servo deixa de resistir, como no detach().
void cutPulse(int j) {
  pwm.setPin(channels[j], 0);
}

// ---------------------------------------------------------------- movimento

// Interpolacao smoothstep: 3t^2 - 2t^3. Arranque e chegada suaves.
int easedAngle(int stepIndex) {
  float t = (float)stepIndex / moveTotal;
  float eased = t * t * (3.0f - 2.0f * t);
  return moveStart + (int)((moveTarget - moveStart) * eased);
}

// "min..max" com '?' no que for desconhecido.
void printLimits(int j) {
  if (limitMin[j] < 0) Serial.print(F("?")); else Serial.print(limitMin[j]);
  Serial.print(F(".."));
  if (limitMax[j] < 0) Serial.print(F("?")); else Serial.print(limitMax[j]);
}

void startMove(int target) {
  target = constrain(target, 0, 180);

  // Aviso, nao recusa. '~~' e um prefixo que o add-on nao trata como erro.
  bool belowMin = limitMin[current] >= 0 && target < limitMin[current];
  bool aboveMax = limitMax[current] >= 0 && target > limitMax[current];
  if (belowMin || aboveMax) {
    Serial.print(F("~~ ")); Serial.print(names[current]);
    Serial.print(F(" fora do limite registrado ")); printLimits(current);
    Serial.println();
  }

  if (target == angles[current]) { writeAngle(current, target); return; }

  if (direct[current]) {                 // um pulso so, na velocidade do servo
    writeAngle(current, target);
    angles[current] = target;
    Serial.print(F(">> ")); Serial.print(names[current]);
    Serial.print(F(" em ")); Serial.println(target);
    return;
  }

  moveStart  = angles[current];
  moveTarget = target;
  moveTotal  = abs(target - angles[current]) * SUBSTEPS;
  moveStep   = 0;
  lastStepAt = millis();
  moving     = true;
}

// Avanca um subpasso se ja passou o intervalo. Nao bloqueia.
void updateMove() {
  if (!moving) return;
  if (millis() - lastStepAt < STEP_DELAY) return;

  lastStepAt = millis();
  moveStep++;
  writeAngle(current, easedAngle(moveStep));

  if (moveStep >= moveTotal) {
    writeAngle(current, moveTarget);
    angles[current] = moveTarget;
    moving = false;
    Serial.print(F(">> ")); Serial.print(names[current]);
    Serial.print(F(" em ")); Serial.println(moveTarget);
  }
}

// Interrompe o movimento e registra onde parou
void abortMove() {
  if (!moving) return;
  angles[current] = easedAngle(moveStep);
  moving = false;
  Serial.print(F("!! abortado em ")); Serial.println(angles[current]);
}

// Solta uma junta. Ela para de resistir: o que a segura passa a ser apenas o
// atrito da reducao, insuficiente sob carga.
void releaseJoint(int j) {
  if (j == current) abortMove();
  if (liveAt[j]) cutPulse(j);
  liveAt[j]     = false;
  declaredAt[j] = false;
  Serial.print(F(">> "));
  Serial.print(names[j]);
  Serial.println(F(" SOLTA (presa so por atrito; nao confie sob carga)"));
}

void releaseAll() {
  abortMove();
  for (int i = 0; i < JOINT_COUNT; i++) if (liveAt[i]) releaseJoint(i);
  pusherReleaseAll();
  current = -1;
  Serial.println(F(">> TUDO SOLTO."));
}

// Declara as quatro juntas numa pose fixa (HOME ou DELIVERY), sem energizar.
// Vale apenas se o braco estiver de fato nela; se foi movido com a mao, use
// 'set'. Juntas ja energizadas sao mantidas.
void declarePose(const int* pose, const __FlashStringHelper* label) {
  for (int i = 0; i < JOINT_COUNT; i++) {
    if (liveAt[i]) continue;              // junta energizada ja tem posicao real
    angles[i]     = pose[i];
    declaredAt[i] = true;
  }
  Serial.print(F(">> juntas declaradas em ")); Serial.print(label);
  Serial.println(F(" (nao energizadas):"));
  Serial.print(F("  "));
  for (int i = 0; i < JOINT_COUNT; i++) {   // da tabela, nao de texto fixo
    Serial.print(F(" ")); Serial.print(names[i]);
    Serial.print(F(" "));  Serial.print(pose[i]);
  }
  Serial.println();
  Serial.println(F("   confira se o braco esta mesmo assim antes do primeiro 'mv'"));
}

void home() { declarePose(ARM_HOME,     F("HOME")); }
void dest() { declarePose(ARM_DELIVERY, F("DELIVERY")); }

// ---------------------------------------------------------------- sequencias

// Energiza a junta se preciso e inicia o movimento, como o 'mv' faz.
// false se a posicao da junta e desconhecida.
bool moveJoint(int j, int ang) {
  if (!liveAt[j] && !declaredAt[j]) {
    Serial.print(F("!! ")); Serial.print(names[j]);
    Serial.println(F(" tem posicao desconhecida; use 'home', 'dest' ou 'set'"));
    return false;
  }
  current = j;
  if (!liveAt[j]) {
    writeAngle(j, angles[j]);            // primeiro pulso ja na posicao declarada
    liveAt[j] = true;
    Serial.print(F(">> ")); Serial.print(names[j]);
    Serial.print(F(" energizada em ")); Serial.println(angles[j]);
  }
  startMove(ang);
  return true;
}

void seqCancel() {
  if (!seqActive) return;
  seqActive = false;
  Serial.println(F("!! sequencia interrompida"));
}

// Avanca a partir de seqIndex ate um passo que exija movimento ou espera.
void seqAdvance() {
  while (seqIndex < seqCount) {
    SeqStep& s = seqSteps[seqIndex];
    if (s.joint == SEQ_WAIT) {
      seqWaitUntil = millis() + s.value;
      return;
    }
    if (s.joint == SEQ_ARM) {            // instantaneo: arma e segue
      pusherArm(s.value);
      seqIndex++;
      continue;
    }
    if (!moveJoint(s.joint, s.value)) { seqActive = false; return; }
    if (moving) return;                  // termina em updateMove; volta por updateSequence
    seqIndex++;                          // ja estava la, ou junta direta
  }
  seqActive = false;
  Serial.println(F(">> sequencia concluida"));
}

void seqStart(const SeqStep* steps, int n) {
  for (int i = 0; i < n; i++) seqSteps[i] = steps[i];
  seqCount  = n;
  seqIndex  = 0;
  seqActive = true;
  seqWaitUntil = 0;
  seqAdvance();
}

// Chamado no loop: quando o passo corrente terminou (movimento ou pausa),
// passa ao proximo.
void updateSequence() {
  if (!seqActive || moving) return;
  if (seqWaitUntil && millis() < seqWaitUntil) return;
  seqWaitUntil = 0;
  seqIndex++;
  seqAdvance();
}

// mv home: base, altura, alcance, garra.
void seqHome() {
  SeqStep s[] = {
    { J_BASE,    (uint16_t)ARM_HOME[J_BASE]    },
    { J_HEIGHT,  (uint16_t)ARM_HOME[J_HEIGHT]  },
    { J_REACH,   (uint16_t)ARM_HOME[J_REACH]   },
    { J_GRIPPER, (uint16_t)ARM_HOME[J_GRIPPER] },
  };
  seqStart(s, 4);
}

// mv dest: alcance, altura, base ate DELIVERY; altura e alcance ate DROP;
// garra abre, pausa, fecha, pausa, repousa; alcance e altura de volta.
void seqDest() {
  SeqStep s[] = {
    { J_REACH,   (uint16_t)ARM_DELIVERY[J_REACH]   },
    { J_HEIGHT,  (uint16_t)ARM_DELIVERY[J_HEIGHT]  },
    { J_BASE,    (uint16_t)ARM_DELIVERY[J_BASE]    },
    { J_HEIGHT,  DROP_HEIGHT                       },
    { J_REACH,   DROP_REACH                        },
    { J_GRIPPER, GRIPPER_OPEN                      },
    { SEQ_WAIT,  GRIP_CLOSE_DELAY_MS               },
    { J_GRIPPER, GRIPPER_CLOSED                    },
    { SEQ_WAIT,  GRIP_HOLD_DELAY_MS                },
    { J_GRIPPER, (uint16_t)ARM_DELIVERY[J_GRIPPER] },
    { J_REACH,   (uint16_t)ARM_DELIVERY[J_REACH]   },
    { J_HEIGHT,  (uint16_t)ARM_DELIVERY[J_HEIGHT]  },
  };
  seqStart(s, 12);
}

// Ciclo completo do braco ('mv re .. es .. --arm'), igual ao do orquestrador:
// espera o operador posicionar a caixinha, pega no canto, leva a esteira e,
// no instante em que a garra abre, arma o sensor da zona; depois volta a
// HOME. O empurrador ja esta na pre-posicao (pusherPrep) e cuida do resto
// sozinho, em paralelo.
void seqArmCycle(int z) {
  SeqStep s[] = {
    { SEQ_WAIT,  PREP_SETTLE_MS       },   // empurrador assenta na pre-posicao
    { SEQ_WAIT,  DELAY_BEFORE_PICK_MS },   // operador posiciona a caixinha

    // aquisicao (mv area)
    { J_BASE,    CORNER0_BASE         },
    { J_GRIPPER, GRIPPER_OPEN         },
    { J_HEIGHT,  APPROACH_HEIGHT      },
    { J_REACH,   APPROACH_REACH       },
    { J_HEIGHT,  CORNER0_HEIGHT       },
    { J_REACH,   CORNER0_REACH        },
    { SEQ_WAIT,  GRIP_CLOSE_DELAY_MS  },
    { J_GRIPPER, GRIPPER_CLOSED       },
    { SEQ_WAIT,  GRIP_HOLD_DELAY_MS   },

    // entrega (mv dest), com o sensor armado ao abrir a garra
    { J_REACH,   (uint16_t)ARM_DELIVERY[J_REACH]   },
    { J_HEIGHT,  (uint16_t)ARM_DELIVERY[J_HEIGHT]  },
    { J_BASE,    (uint16_t)ARM_DELIVERY[J_BASE]    },
    { J_HEIGHT,  DROP_HEIGHT                       },
    { J_REACH,   DROP_REACH                        },
    { J_GRIPPER, GRIPPER_OPEN                      },
    { SEQ_ARM,   (uint16_t)z                       },
    { SEQ_WAIT,  GRIP_CLOSE_DELAY_MS               },
    { J_GRIPPER, GRIPPER_CLOSED                    },
    { SEQ_WAIT,  GRIP_HOLD_DELAY_MS                },
    { J_GRIPPER, (uint16_t)ARM_DELIVERY[J_GRIPPER] },
    { J_REACH,   (uint16_t)ARM_DELIVERY[J_REACH]   },
    { J_HEIGHT,  (uint16_t)ARM_DELIVERY[J_HEIGHT]  },

    // volta (mv home)
    { J_BASE,    (uint16_t)ARM_HOME[J_BASE]    },
    { J_HEIGHT,  (uint16_t)ARM_HOME[J_HEIGHT]  },
    { J_REACH,   (uint16_t)ARM_HOME[J_REACH]   },
    { J_GRIPPER, (uint16_t)ARM_HOME[J_GRIPPER] },
  };
  seqStart(s, 28);
}

// mv area: base do canto, garra abre, aproximacao (altura, alcance), descida
// (altura, alcance), pausa, garra fecha, pausa.
void seqArea() {
  SeqStep s[] = {
    { J_BASE,    CORNER0_BASE         },
    { J_GRIPPER, GRIPPER_OPEN         },
    { J_HEIGHT,  APPROACH_HEIGHT      },
    { J_REACH,   APPROACH_REACH       },
    { J_HEIGHT,  CORNER0_HEIGHT       },
    { J_REACH,   CORNER0_REACH        },
    { SEQ_WAIT,  GRIP_CLOSE_DELAY_MS  },
    { J_GRIPPER, GRIPPER_CLOSED       },
    { SEQ_WAIT,  GRIP_HOLD_DELAY_MS   },
  };
  seqStart(s, 9);
}

// --------------------------------------------------------------- empurrador

void pusherWrite(int z, int angle) {
  pwm.writeMicroseconds(zoneCh[z], map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US));
}

int pusherEased(int z, int stepIndex) {
  float t = (float)stepIndex / pTotal[z];
  float eased = t * t * (3.0f - 2.0f * t);
  return pFrom[z] + (int)((pTo[z] - pFrom[z]) * eased);
}

// Como em producao: sem pulso ainda, declara no alvo e energiza ali, num
// pulso so (sem carga, um salto e inofensivo). Com pulso, interpola.
void pusherStartMove(int z, int target) {
  if (!pLive[z]) {
    pusherWrite(z, target);
    pAngle[z]  = target;
    pLive[z]   = true;
    pMoving[z] = false;
    return;
  }
  if (target == pAngle[z]) { pMoving[z] = false; return; }
  pFrom[z]   = pAngle[z];
  pTo[z]     = target;
  pTotal[z]  = abs(pTo[z] - pFrom[z]) * PUSHER_SUBSTEPS;
  pStep[z]   = 0;
  pLastAt[z] = millis();
  pMoving[z] = true;
}

void pusherTag(int z) {
  Serial.print(F(">> ")); Serial.print(zoneName[z]); Serial.print(' ');
}

// Leva o empurrador a pre-posicao do sentido. armOnReady: arma o sensor ao
// chegar (uso sem --arm); senao para em P_READY e espera 'pusherArm'.
bool pusherPrep(int z, bool cw, bool armOnReady) {
  if (!pcaOk) { Serial.println(F("!! PCA9685 nao respondeu no boot; confira I2C e reinicie")); return false; }
  if (pPhase[z] != P_IDLE) {
    Serial.print(F("!! ")); Serial.print(zoneName[z]);
    Serial.println(F(" em ciclo; use 'stop'"));
    return false;
  }
  // Sensor ja em obstaculo: o empurrao sairia agora, antes da caixinha.
  // Sensibilidade alta demais (ve a esteira) ou pino solto.
  if (digitalRead(zoneIrPin[z]) == LOW) {
    Serial.print(F("!! ")); Serial.print(zoneName[z]);
    Serial.println(F(": sensor ja em obstaculo; ajuste o trimpot ou confira o pino"));
    return false;
  }
  pCw[z]         = cw;
  pPre[z]        = cw ? zonePreCw[z]  : zonePreCcw[z];
  pPush[z]       = cw ? zonePushCw[z] : zonePushCcw[z];
  pArmOnReady[z] = armOnReady;
  pusherStartMove(z, pPre[z]);
  pPhase[z] = P_PREP;
  pusherTag(z);
  Serial.print(cw ? F("cw (es 2): ") : F("ccw (es 1): "));
  Serial.print(pPre[z]); Serial.print(F(" -> ")); Serial.print(pPush[z]);
  Serial.print(F(" -> ")); Serial.println(pPre[z]);
  return true;
}

// Arma o sensor de uma zona que ja esta na pre-posicao. Chamado pelo passo
// SEQ_ARM, quando a garra abre e a caixinha cai na esteira.
void pusherArm(int z) {
  if (pPhase[z] != P_READY && pPhase[z] != P_PREP) {
    Serial.print(F("!! ")); Serial.print(zoneName[z]);
    Serial.println(F(" nao esta na pre-posicao; sensor nao armado"));
    return;
  }
  pArmOnReady[z] = true;       // se ainda em P_PREP, arma ao chegar
  if (pPhase[z] == P_READY) {
    pLowSince[z] = 0;
    pPhase[z] = P_ARMED;
    pusherTag(z); Serial.println(F("sensor armado: a caixinha esta na esteira"));
  }
}

// Avanca a interpolacao e a maquina de fases de todas as zonas. Nao bloqueia.
void pusherUpdate() {
  unsigned long now = millis();

  for (int z = 0; z < ZONE_COUNT; z++) {
    if (pMoving[z] && now - pLastAt[z] >= PUSHER_STEP_DELAY) {
      pLastAt[z] = now;
      pStep[z]++;
      pusherWrite(z, pusherEased(z, pStep[z]));
      if (pStep[z] >= pTotal[z]) {
        pusherWrite(z, pTo[z]);
        pAngle[z]  = pTo[z];
        pMoving[z] = false;
      }
    }

    switch (pPhase[z]) {
      case P_IDLE:
        break;

      case P_PREP:
        if (pMoving[z]) break;
        pLowSince[z] = 0;
        if (pArmOnReady[z]) {
          pPhase[z] = P_ARMED;
          pusherTag(z);
          Serial.print(F("em ")); Serial.print(pAngle[z]);
          Serial.println(F(", sensor armado: passe a caixinha"));
        } else {
          pPhase[z] = P_READY;
          pusherTag(z);
          Serial.print(F("em ")); Serial.print(pAngle[z]);
          Serial.println(F(", pronto; sensor arma quando a garra abrir"));
        }
        break;

      case P_READY:
        break;                 // espera pusherArm (passo SEQ_ARM)

      case P_ARMED:
        if (digitalRead(zoneIrPin[z]) != LOW) { pLowSince[z] = 0; break; }
        if (pLowSince[z] == 0) { pLowSince[z] = now; break; }
        if (now - pLowSince[z] < IR_DEBOUNCE_MS) break;
        Serial.print(F("DET ")); Serial.println(zoneName[z]);
        pPhaseAt[z] = now;
        pPhase[z]   = P_WAITING;
        break;

      case P_WAITING:
        if (now - pPhaseAt[z] < PUSHER_DET_DELAY_MS) break;
        pusherStartMove(z, pPush[z]);
        pPhase[z] = P_PUSHING;
        break;

      case P_PUSHING:
        if (pMoving[z]) break;
        pPhaseAt[z] = now;
        pPhase[z]   = P_HOLDING;
        break;

      case P_HOLDING:
        if (now - pPhaseAt[z] < PUSHER_HOLD_MS) break;
        pusherStartMove(z, pPre[z]);
        pPhase[z] = P_RETURNING;
        break;

      case P_RETURNING:
        if (pMoving[z]) break;
        pPhaseAt[z] = now;
        pPhase[z]   = P_SETTLING;
        break;

      case P_SETTLING:
        if (now - pPhaseAt[z] < PUSHER_SETTLE_MS) break;
        pPhase[z] = P_IDLE;
        Serial.print(F("PUSHED ")); Serial.println(zoneName[z]);
        pusherTag(z);
        Serial.print(F("de volta em ")); Serial.println(pAngle[z]);
        break;
    }
  }
}

// 'stop': desarma e para os empurradores onde estiverem, energizados.
void pusherAbort() {
  for (int z = 0; z < ZONE_COUNT; z++) {
    if (pPhase[z] == P_IDLE) continue;
    if (pMoving[z]) { pAngle[z] = pusherEased(z, pStep[z]); pMoving[z] = false; }
    pPhase[z] = P_IDLE;
    Serial.print(F("!! ")); Serial.print(zoneName[z]);
    Serial.print(F(" ciclo abortado em ")); Serial.println(pAngle[z]);
  }
}

void pusherRelease(int z) {
  if (pPhase[z] != P_IDLE) {
    if (pMoving[z]) { pAngle[z] = pusherEased(z, pStep[z]); pMoving[z] = false; }
    pPhase[z] = P_IDLE;
  }
  if (pLive[z]) pwm.setPin(zoneCh[z], 0);
  pLive[z] = false;
  Serial.print(F(">> ")); Serial.print(zoneName[z]); Serial.println(F(" SOLTO"));
}

void pusherReleaseAll() {
  for (int z = 0; z < ZONE_COUNT; z++) if (pLive[z] || pPhase[z] != P_IDLE) pusherRelease(z);
}

// 'mv <zona> <ang>': posiciona um empurrador, para achar pre/push de cada um.
void pusherMoveTo(int z, int ang) {
  if (!pcaOk) { Serial.println(F("!! PCA9685 nao respondeu no boot; confira I2C e reinicie")); return; }
  if (pPhase[z] != P_IDLE) {
    Serial.print(F("!! ")); Serial.print(zoneName[z]);
    Serial.println(F(" em ciclo; use 'stop'"));
    return;
  }
  pusherStartMove(z, constrain(ang, 0, 180));
  pusherTag(z); Serial.print(F("-> ")); Serial.println(ang);
}

int findZone(const char* n) {
  for (int z = 0; z < ZONE_COUNT; z++) if (!strcmp(n, zoneName[z])) return z;
  return -1;
}

void printPusherLine(int z) {
  Serial.print(F("  ")); Serial.print(z + 1); Serial.print(' ');
  Serial.print(zoneName[z]); Serial.print(F("\t"));
  if (pLive[z]) Serial.print(pAngle[z]); else Serial.print(F("?"));
  Serial.print(F("\t"));
  switch (pPhase[z]) {
    case P_IDLE:      Serial.print(pLive[z] ? F("energizado") : F("solto")); break;
    case P_PREP:      Serial.print(F("PRE-POSICAO")); break;
    case P_READY:     Serial.print(F("PRONTO"));      break;
    case P_ARMED:     Serial.print(F("ARMADO"));      break;
    case P_WAITING:   Serial.print(F("LATENCIA"));    break;
    case P_PUSHING:   Serial.print(F("EMPURRANDO"));  break;
    case P_HOLDING:   Serial.print(F("SEGURANDO"));   break;
    case P_RETURNING: Serial.print(F("VOLTANDO"));    break;
    case P_SETTLING:  Serial.print(F("ASSENTANDO"));  break;
  }
  if (pPhase[z] != P_IDLE) Serial.print(pCw[z] ? F(" cw") : F(" ccw"));
  Serial.print(F("\tch ")); Serial.print(zoneCh[z]);
  Serial.print(F("  ir ")); Serial.print(zoneIrPin[z]);
  Serial.print(F("  cw ")); Serial.print(zonePreCw[z]); Serial.print(F("->")); Serial.print(zonePushCw[z]);
  Serial.print(F("  ccw ")); Serial.print(zonePreCcw[z]); Serial.print(F("->")); Serial.print(zonePushCcw[z]);
  Serial.print(F("  sensor ")); Serial.println(digitalRead(zoneIrPin[z]) == LOW ? F("OBSTACULO") : F("livre"));
}

// ------------------------------------------------------------------ relatos

void printJointLine(int j) {
  Serial.print(F("  "));
  Serial.print(names[j]);
  Serial.print(F("\t"));
  if (declaredAt[j] || liveAt[j]) Serial.print(angles[j]); else Serial.print(F("?"));
  Serial.print(F("\t"));
  if      (j == current && moving) Serial.print(F("MOVENDO"));
  else if (liveAt[j])              Serial.print(j == current ? F("ATIVA*") : F("energizada"));
  else if (declaredAt[j])          Serial.print(F("declarada"));
  else                             Serial.print(F("solta"));
  Serial.print(F("\t"));
  printLimits(j);
  Serial.println();
}

// Tabela completa: e daqui que saem as constantes do firmware de producao
// e e o que o add-on le na conexao para montar centros e limites.
void dumpAll() {
  Serial.println();
  Serial.println(F("=== juntas ==="));
  Serial.println(F("  nome\tang\testado\tlimites"));
  for (int i = 0; i < JOINT_COUNT; i++) printJointLine(i);
  Serial.println();
  Serial.println(F("=== empurradores ==="));
  Serial.println(F("  re zona\tang\testado\tcanal, sensor e posicoes"));
  for (int z = 0; z < ZONE_COUNT; z++) printPusherLine(z);
  Serial.println();
}

void report() {
  if (current < 0) { Serial.println(F(">> nenhuma junta ativa")); return; }
  printJointLine(current);
}

void help() {
  Serial.println();
  Serial.println(F("=== Calibracao do braco ==="));
  Serial.println(F("  home / dest    declara as 4 juntas em HOME / DELIVERY (nao energiza)"));
  Serial.println(F("  mv home        sequencia: base, altura, alcance, garra -> HOME"));
  Serial.println(F("  mv dest        sequencia: DELIVERY, DROP, garra abre/fecha, volta"));
  Serial.println(F("  mv area        sequencia: canto 0, aproximacao, descida, garra fecha"));
  Serial.println(F("  set <j> <ang>  declara onde a junta esta AGORA (nao move)"));
  Serial.println(F("  sel <j>        torna ativa e informa o angulo atual (nao move)"));
  Serial.println(F("  mv <j> <ang>   torna ativa, energiza sem salto e vai ate <ang>"));
  Serial.println(F("  mv re <r> es <e> [--arm]"));
  Serial.println(F("                 ciclo da regiao r (1 Norte .. 5 Sul), estado e"));
  Serial.println(F("                 (1 = ccw, 2 = cw): pre-posicao, arma, empurra e volta."));
  Serial.println(F("                 --arm inclui o braco: pega no canto 0, entrega e volta;"));
  Serial.println(F("                 o sensor so arma quando a garra abre."));
  Serial.println(F("  mv <zona> <ang>  posiciona um empurrador (norte, nordeste, ...)"));
  Serial.println(F("  + / -          move a junta ativa (garra 1 grau, demais 2)"));
  Serial.println(F("  stop           interrompe movimento e ciclo da zona, mantem energizado"));
  Serial.println(F("  off [<j>]      solta a junta indicada (ou 'norte'), ou a ativa"));
  Serial.println(F("  offall         solta todas as juntas (panico)"));
  Serial.println(F("  min / max      registra o angulo atual como limite da junta"));
  Serial.println(F("  dump           tabela de todas as juntas"));
  Serial.println(F("  ? / h          estado / esta ajuda"));
  Serial.println();
  Serial.println(F("  juntas: base|b  garra|g  altura|al  alcance|ac"));
  Serial.println();
}

// ----------------------------------------------------------------- comandos

bool startsWith(const char* s, const char* prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

// Resolve nome completo ou alias. -1 se nao reconhecer.
int findJoint(const char* n) {
  for (int i = 0; i < JOINT_COUNT; i++) {
    if (!strcmp(n, names[i]) || !strcmp(n, alias1[i])) return i;
  }
  return -1;
}

// Parte "<junta> <angulo>" em dois campos. Devolve a junta e escreve o angulo
// em out; -1 se o formato nao bater.
// Corta "<junta> [<angulo>]" em dois campos, terminando o nome no lugar.
// Devolve a junta; escreve o angulo em out e marca hasAngle se houver numero.
int parseJointArg(char* args, int* out, bool* hasAngle) {
  *hasAngle = false;
  while (*args == ' ') args++;
  char* sp = strchr(args, ' ');
  if (sp) *sp = '\0';                    // separa nome do resto
  int j = findJoint(args);
  if (j < 0) return -1;
  if (!sp) return j;                     // 'set <junta>': consulta

  char* num = sp + 1;
  while (*num == ' ') num++;
  if (*num == '\0') return j;            // espaco solto no fim: ainda consulta
  *out      = constrain(atoi(num), 0, 180);
  *hasAngle = true;
  return j;
}

void markLimit(bool isMax) {
  if (current < 0) { Serial.println(F("!! nenhuma junta ativa")); return; }
  if (isMax) limitMax[current] = angles[current];
  else       limitMin[current] = angles[current];
  Serial.print(F(">> "));
  Serial.print(names[current]);
  Serial.print(isMax ? F(" max = ") : F(" min = "));
  Serial.print(angles[current]);
  // O limite do alcance so vale para a altura corrente: registre as duas.
  if (current == J_REACH && liveAt[J_HEIGHT]) {
    Serial.print(F("  (altura = ")); Serial.print(angles[J_HEIGHT]); Serial.print(F(")"));
  } else if (current == J_HEIGHT && liveAt[J_REACH]) {
    Serial.print(F("  (alcance = ")); Serial.print(angles[J_REACH]); Serial.print(F(")"));
  }
  Serial.println();
}

void handleCommand(char* cmd) {
  if (cmd[0] == '\0') return;

  // Comandos sempre aceitos, inclusive durante movimento
  if (!strcmp(cmd, "offall")) { releaseAll(); return; }
  if (!strcmp(cmd, "stop"))   { abortMove(); seqCancel(); pusherAbort(); return; }
  if (!strcmp(cmd, "dump"))   { dumpAll();    return; }
  if (!strcmp(cmd, "?"))      { report();     return; }
  if (!strcmp(cmd, "h") || !strcmp(cmd, "help")) { help(); return; }

  if (!strcmp(cmd, "off")) {
    if (current < 0) { Serial.println(F("!! nenhuma junta ativa")); return; }
    releaseJoint(current);
    return;
  }

  if (startsWith(cmd, "off ")) {
    char* n = cmd + 4;
    while (*n == ' ') n++;
    int z = findZone(n);
    if (z >= 0) { pusherRelease(z); return; }
    int j = findJoint(n);
    if (j < 0) { Serial.println(F("!! junta invalida")); return; }
    releaseJoint(j);
    return;
  }

  // Ajuste fino redireciona o movimento em curso em vez de recusar
  if (!strcmp(cmd, "+") || !strcmp(cmd, "-")) {
    if (current < 0)      { Serial.println(F("!! nenhuma junta ativa")); return; }
    if (!liveAt[current]) { Serial.println(F("!! junta nao energizada; use 'mv'")); return; }
    if (seqActive) { Serial.println(F("!! sequencia em curso; use 'stop'")); return; }
    if (moving) { angles[current] = easedAngle(moveStep); moving = false; }
    int d = stepSize[current];
    startMove(angles[current] + (cmd[0] == '+' ? d : -d));
    return;
  }

  if (!strcmp(cmd, "min") || !strcmp(cmd, "max")) {
    if (moving) { Serial.println(F("!! pare antes de registrar")); return; }
    markLimit(cmd[1] == 'a');
    return;
  }

  if (!strcmp(cmd, "home") || !strcmp(cmd, "dest")) {
    if (moving || seqActive) { Serial.println(F("!! em movimento; use 'stop'")); return; }
    if (cmd[0] == 'h') home(); else dest();
    return;
  }

  // 'mv re <1-5> es <1-2> [--arm]': ciclo de separacao da regiao. Antes da
  // trava de 'moving' quando nao ha '--arm': o empurrador e independente do
  // braco, e em producao o sensor fica armado enquanto o braco entrega e
  // volta a HOME.
  if (startsWith(cmd, "mv re ")) {
    char* p = cmd + 6;
    while (*p == ' ') p++;
    int region = atoi(p);
    while (isDigit(*p)) p++;
    while (*p == ' ') p++;
    if (!startsWith(p, "es ")) { Serial.println(F("!! uso: mv re <1-5> es <1-2> [--arm]")); return; }
    p += 3;
    while (*p == ' ') p++;
    int state = atoi(p);
    while (isDigit(*p)) p++;
    while (*p == ' ') p++;
    bool withArm = !strcmp(p, "--arm");
    if (*p && !withArm) { Serial.println(F("!! uso: mv re <1-5> es <1-2> [--arm]")); return; }
    if (region < 1 || region > ZONE_COUNT) { Serial.println(F("!! regiao: 1 a 5")); return; }
    if (state < 1 || state > 2)            { Serial.println(F("!! estado: 1 (ccw) ou 2 (cw)")); return; }
    int z = region - 1;
    if (withArm && (moving || seqActive)) { Serial.println(F("!! em movimento; use 'stop'")); return; }

    // Estado 1 gira anti-horario; estado 2, horario.
    if (!pusherPrep(z, state == 2, !withArm)) return;
    if (withArm) seqArmCycle(z);
    return;
  }

  // 'mv <zona> <ang>': posiciona um empurrador, para achar suas posicoes.
  if (startsWith(cmd, "mv ")) {
    char* n = cmd + 3;
    while (*n == ' ') n++;
    char* sp = strchr(n, ' ');
    if (sp) {
      *sp = '\0';
      int z = findZone(n);
      if (z >= 0) {
        char* a = sp + 1;
        while (*a == ' ') a++;
        if (!isDigit(a[0])) { Serial.println(F("!! uso: mv <zona> <ang>")); return; }
        pusherMoveTo(z, atoi(a));
        return;
      }
      *sp = ' ';
    }
  }

  if (moving || seqActive) { Serial.println(F("!! em movimento; use 'stop' ou 'off'")); return; }

  // Sequencias de bancada, as mesmas do robosort-firmware.
  if (!strcmp(cmd, "mv home")) { if (pcaOk) seqHome(); else Serial.println(F("!! PCA9685 ausente")); return; }
  if (!strcmp(cmd, "mv dest")) { if (pcaOk) seqDest(); else Serial.println(F("!! PCA9685 ausente")); return; }
  if (!strcmp(cmd, "mv area") || !strcmp(cmd, "mv area 0")) {
    if (pcaOk) seqArea(); else Serial.println(F("!! PCA9685 ausente"));
    return;
  }

  // Declara onde a junta esta AGORA. Nao move, nao energiza.
  if (startsWith(cmd, "set ")) {
    int  ang;
    bool hasAngle;
    int  j = parseJointArg(cmd + 4, &ang, &hasAngle);
    if (j < 0)      { Serial.println(F("!! uso: set <junta> <ang>")); return; }
    if (!hasAngle)  { Serial.println(F("!! falta o angulo; para consultar use 'sel'")); return; }
    if (liveAt[j])  { Serial.println(F("!! solte a junta antes de redeclarar")); return; }
    angles[j]     = ang;
    declaredAt[j] = true;
    Serial.print(F(">> ")); Serial.print(names[j]);
    Serial.print(F(" declarada em ")); Serial.println(ang);
    return;
  }

  // Torna a junta ativa e informa a posicao, sem mover nada.
  if (startsWith(cmd, "sel ")) {
    int  ang;
    bool hasAngle;
    int  j = parseJointArg(cmd + 4, &ang, &hasAngle);
    if (j < 0)     { Serial.println(F("!! uso: sel <junta>")); return; }
    if (hasAngle)  { Serial.println(F("!! 'sel' nao move; use 'mv <junta> <ang>'")); return; }
    current = j;
    printJointLine(j);
    return;
  }

  // Torna a junta ativa, energiza se preciso e move. Nao solta as demais:
  // elas seguem energizadas, sustentando o braco.
  if (startsWith(cmd, "mv ")) {
    int  ang;
    bool hasAngle;
    int  j = parseJointArg(cmd + 3, &ang, &hasAngle);
    if (j < 0)     { Serial.println(F("!! uso: mv <junta> <ang>")); return; }
    if (!hasAngle) { Serial.println(F("!! falta o angulo; para consultar use 'sel'")); return; }
    if (!pcaOk)    { Serial.println(F("!! PCA9685 nao respondeu no boot; confira I2C e reinicie")); return; }

    if (!liveAt[j] && !declaredAt[j]) {
      Serial.print(F("!! ")); Serial.print(names[j]);
      Serial.println(F(" tem posicao desconhecida"));
      Serial.println(F("   use 'home' (braco em repouso) ou 'set <junta> <ang>'"));
      return;
    }

    current = j;
    if (!liveAt[j]) {
      writeAngle(j, angles[j]);          // primeiro pulso ja na posicao declarada
      liveAt[j] = true;
      Serial.print(F(">> ")); Serial.print(names[j]);
      Serial.print(F(" energizada em ")); Serial.println(angles[j]);
    }
    startMove(ang);
    return;
  }

  Serial.println(F("!! comando invalido; digite 'h'"));
}

// Le a serial caractere a caractere. Nunca bloqueia: sem isso o
// readStringUntil() congelava o loop por ate 1 s e o 'stop' chegava atrasado.
void pollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      handleCommand(lineBuf);
      lineLen = 0;
      return;                      // uma linha por passagem, movimento segue
    }
    if (lineLen < LINE_MAX - 1) lineBuf[lineLen++] = c;
  }
}

void setup() {
  Serial.begin(115200);
  for (int z = 0; z < ZONE_COUNT; z++) {
    pinMode(zoneIrPin[z], INPUT);
    pPhase[z]  = P_IDLE;
    pLive[z]   = false;
    pMoving[z] = false;
    pAngle[z]  = zonePreCw[z];
    pCw[z]     = true;
    pPre[z]    = zonePreCw[z];
    pPush[z]   = zonePushCw[z];
    pArmOnReady[z] = true;
    pLowSince[z]   = 0;
  }
  for (int i = 0; i < JOINT_COUNT; i++) {
    angles[i]     = ARM_HOME[i];       // sem sentido ate 'home'/'dest'/'set'; so um valor inicial
    declaredAt[i] = false;
    liveAt[i]     = false;
    limitMin[i]   = knownMin[i];       // 'min'/'max' sobrescrevem em RAM
    limitMax[i]   = knownMax[i];
  }

  // Um reset do Arduino (inclusive o que abrir o Monitor Serial provoca) nao
  // reseta o PCA9685: os canais continuariam pulsando na ultima posicao,
  // com este firmware sem saber disso. Cortar todos os 16 canais aqui e o
  // que sustenta a garantia de "nenhum sinal ate comando explicito", e
  // reproduz o comportamento do Servo.h, que soltava tudo no reset.
  pcaOk = pwm.begin();
  if (pcaOk) {
    for (int ch = 0; ch < 16; ch++) pwm.setPin(ch, 0);
    pwm.setOscillatorFrequency(OSC_FREQ);
    pwm.setPWMFreq(50);
  }

  help();
  if (pcaOk) {
    Serial.println(F("Nenhuma junta energizada. Sistema em repouso."));
  } else {
    Serial.println(F("!! PCA9685 NAO RESPONDEU no I2C. Confira SDA/SCL (A4/A5),"));
    Serial.println(F("   alimentacao VCC do modulo e GND comum. 'mv' esta bloqueado."));
  }
}

void loop() {
  pollSerial();
  updateMove();
  updateSequence();
  pusherUpdate();
}
