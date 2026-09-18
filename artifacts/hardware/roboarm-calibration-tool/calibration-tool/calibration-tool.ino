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

// ------------------------------------------------------- zona de separacao
// Espelho de robosort-firmware/config.h (secao Separacao). Ao ajustar la,
// ajuste aqui. Sensor FC-51: LOW = obstaculo. O empurrador tem interpolador
// proprio, na velocidade do module-tester (4 ms/grau), independente do braco.
// Ciclo: pre-posicao -> armado -> DET -> latencia -> empurrao -> segura ->
// volta a pre-posicao -> assenta -> PUSHED. ID par = cw, impar = ccw.
#define ZONE_NAME          "norte"
#define IR_PIN             2
#define PUSHER_CH          8
#define IR_DEBOUNCE_MS     20
#define PUSHER_STEP_DELAY  4      // ms entre subpassos
#define PUSHER_SUBSTEPS    1      // subpassos por grau
#define PUSHER_DET_DELAY_MS 400  // da deteccao ao inicio do empurrao (caixinha chega ao empurrador)
#define PUSHER_HOLD_MS     1000   // segura o empurrao antes de voltar
#define PUSHER_SETTLE_MS   200    // assentamento na volta, antes do PUSHED
#define PUSHER_PRE_CW      0
#define PUSHER_PRE_CCW     180
#define PUSHER_PUSH_CW     180
#define PUSHER_PUSH_CCW    0

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
#define SEQ_MAX  14
struct SeqStep { int8_t joint; uint16_t value; };
SeqStep seqSteps[SEQ_MAX];
int  seqCount  = 0;
int  seqIndex  = 0;
bool seqActive = false;
unsigned long seqWaitUntil = 0;

// Estado do empurrador e do ciclo da zona. Independente da junta ativa e de
// 'moving': o braco pode se mover com o sensor armado, como em producao.
enum PusherPhase { P_IDLE, P_PREP, P_ARMED, P_WAITING, P_PUSHING, P_HOLDING, P_RETURNING, P_SETTLING };
PusherPhase pPhase   = P_IDLE;
bool pLive    = false;      // ja recebeu pulso; pAngle e a posicao real
int  pAngle   = PUSHER_PRE_CW;
bool pCw      = true;
int  pPre     = PUSHER_PRE_CW;
int  pPush    = PUSHER_PUSH_CW;
bool pMoving  = false;
int  pFrom, pTo, pStep, pTotal;
unsigned long pLastAt   = 0;
unsigned long pPhaseAt  = 0;   // inicio da espera (latencia, segura, assenta)
unsigned long pLowSince = 0;   // debounce do sensor

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
  if (pLive || pPhase != P_IDLE) pusherRelease();
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

void pusherWrite(int angle) {
  pwm.writeMicroseconds(PUSHER_CH, map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US));
}

int pusherEased(int stepIndex) {
  float t = (float)stepIndex / pTotal;
  float eased = t * t * (3.0f - 2.0f * t);
  return pFrom + (int)((pTo - pFrom) * eased);
}

// Como em producao: sem pulso ainda, declara no alvo e energiza ali, num
// pulso so (sem carga, um salto e inofensivo). Com pulso, interpola.
void pusherStartMove(int target) {
  if (!pLive) {
    pusherWrite(target);
    pAngle  = target;
    pLive   = true;
    pMoving = false;
    return;
  }
  if (target == pAngle) { pMoving = false; return; }
  pFrom   = pAngle;
  pTo     = target;
  pTotal  = abs(pTo - pFrom) * PUSHER_SUBSTEPS;
  pStep   = 0;
  pLastAt = millis();
  pMoving = true;
}

void pusherPrint(const __FlashStringHelper* what) {
  Serial.print(F(">> ")); Serial.print(F(ZONE_NAME)); Serial.print(' '); Serial.println(what);
}

// 'mv norte <id|cw|ccw>': energiza na pre-posicao do sentido, arma o sensor
// e deixa o ciclo correr sozinho em pusherUpdate().
void pusherCycle(bool cw) {
  if (!pcaOk) { Serial.println(F("!! PCA9685 nao respondeu no boot; confira I2C e reinicie")); return; }
  if (pPhase != P_IDLE) {
    Serial.print(F("!! ")); Serial.print(F(ZONE_NAME));
    Serial.println(F(" em ciclo; use 'stop'"));
    return;
  }
  // Sensor ja em obstaculo: o empurrao sairia agora, antes da caixinha.
  // Sensibilidade alta demais (ve a esteira) ou pino solto.
  if (digitalRead(IR_PIN) == LOW) {
    Serial.println(F("!! sensor ja em obstaculo; ajuste o trimpot ou confira o pino"));
    return;
  }
  pCw   = cw;
  pPre  = cw ? PUSHER_PRE_CW  : PUSHER_PRE_CCW;
  pPush = cw ? PUSHER_PUSH_CW : PUSHER_PUSH_CCW;
  pusherStartMove(pPre);
  pPhase = P_PREP;
  Serial.print(F(">> ")); Serial.print(F(ZONE_NAME));
  Serial.print(cw ? F(" cw: ") : F(" ccw: "));
  Serial.print(pPre); Serial.print(F(" -> ")); Serial.print(pPush);
  Serial.print(F(" -> ")); Serial.println(pPre);
}

// Avanca a interpolacao e a maquina de fases. Nao bloqueia.
void pusherUpdate() {
  unsigned long now = millis();

  if (pMoving && now - pLastAt >= PUSHER_STEP_DELAY) {
    pLastAt = now;
    pStep++;
    pusherWrite(pusherEased(pStep));
    if (pStep >= pTotal) {
      pusherWrite(pTo);
      pAngle  = pTo;
      pMoving = false;
    }
  }

  switch (pPhase) {
    case P_IDLE:
      break;

    case P_PREP:
      if (pMoving) break;
      pLowSince = 0;
      pPhase    = P_ARMED;
      Serial.print(F(">> ")); Serial.print(F(ZONE_NAME));
      Serial.print(F(" em ")); Serial.print(pAngle);
      Serial.println(F(", sensor armado: passe a caixinha"));
      break;

    case P_ARMED:
      if (digitalRead(IR_PIN) != LOW) { pLowSince = 0; break; }
      if (pLowSince == 0) { pLowSince = now; break; }
      if (now - pLowSince < IR_DEBOUNCE_MS) break;
      Serial.print(F("DET ")); Serial.println(F(ZONE_NAME));
      pPhaseAt = now;
      pPhase   = P_WAITING;
      break;

    case P_WAITING:
      if (now - pPhaseAt < PUSHER_DET_DELAY_MS) break;
      pusherStartMove(pPush);
      pPhase = P_PUSHING;
      break;

    case P_PUSHING:
      if (pMoving) break;
      pPhaseAt = now;
      pPhase   = P_HOLDING;
      break;

    case P_HOLDING:
      if (now - pPhaseAt < PUSHER_HOLD_MS) break;
      pusherStartMove(pPre);
      pPhase = P_RETURNING;
      break;

    case P_RETURNING:
      if (pMoving) break;
      pPhaseAt = now;
      pPhase   = P_SETTLING;
      break;

    case P_SETTLING:
      if (now - pPhaseAt < PUSHER_SETTLE_MS) break;
      pPhase = P_IDLE;
      Serial.print(F("PUSHED ")); Serial.println(F(ZONE_NAME));
      Serial.print(F(">> ")); Serial.print(F(ZONE_NAME));
      Serial.print(F(" de volta em ")); Serial.println(pAngle);
      break;
  }
}

// 'stop': desarma e para o empurrador onde estiver, energizado.
void pusherAbort() {
  if (pPhase == P_IDLE) return;
  if (pMoving) { pAngle = pusherEased(pStep); pMoving = false; }
  pPhase = P_IDLE;
  Serial.print(F("!! ")); Serial.print(F(ZONE_NAME));
  Serial.print(F(" ciclo abortado em ")); Serial.println(pAngle);
}

void pusherRelease() {
  pusherAbort();
  if (pLive) pwm.setPin(PUSHER_CH, 0);
  pLive = false;
  Serial.print(F(">> ")); Serial.print(F(ZONE_NAME)); Serial.println(F(" SOLTO"));
}

void printPusherLine() {
  Serial.print(F("  ")); Serial.print(F(ZONE_NAME)); Serial.print(F("\t"));
  if (pLive) Serial.print(pAngle); else Serial.print(F("?"));
  Serial.print(F("\t"));
  switch (pPhase) {
    case P_IDLE:      Serial.print(pLive ? F("energizado") : F("solto")); break;
    case P_PREP:      Serial.print(F("PRE-POSICAO"));  break;
    case P_ARMED:     Serial.print(F("ARMADO"));       break;
    case P_WAITING:   Serial.print(F("LATENCIA"));     break;
    case P_PUSHING:   Serial.print(F("EMPURRANDO"));   break;
    case P_HOLDING:   Serial.print(F("SEGURANDO"));    break;
    case P_RETURNING: Serial.print(F("VOLTANDO"));     break;
    case P_SETTLING:  Serial.print(F("ASSENTANDO"));   break;
  }
  if (pPhase != P_IDLE) Serial.print(pCw ? F(" cw") : F(" ccw"));
  Serial.print(F("\tcw ")); Serial.print(PUSHER_PRE_CW); Serial.print(F("->")); Serial.print(PUSHER_PUSH_CW);
  Serial.print(F("  ccw ")); Serial.print(PUSHER_PRE_CCW); Serial.print(F("->")); Serial.print(PUSHER_PUSH_CCW);
  Serial.print(F("  sensor ")); Serial.println(digitalRead(IR_PIN) == LOW ? F("OBSTACULO") : F("livre"));
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
  Serial.println(F("=== empurrador ==="));
  Serial.println(F("  zona\tang\testado\tposicoes"));
  printPusherLine();
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
  Serial.println(F("  mv norte <id>  ciclo da zona: pre-posicao, arma o sensor, empurra e volta"));
  Serial.println(F("                 (id par = cw, impar = ccw; aceita 'cw'/'ccw' direto)"));
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
    if (!strcmp(n, ZONE_NAME)) { pusherRelease(); return; }
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

  // Ciclo da zona de separacao. Antes da trava de 'moving': o empurrador e
  // independente do braco, e em producao o sensor fica armado enquanto o
  // braco entrega e volta a HOME.
  if (startsWith(cmd, "mv " ZONE_NAME)) {
    char* arg = cmd + 3 + strlen(ZONE_NAME);
    while (*arg == ' ') arg++;
    if      (!strcmp(arg, "cw"))  pusherCycle(true);
    else if (!strcmp(arg, "ccw")) pusherCycle(false);
    else if (isDigit(arg[0]))      pusherCycle(atoi(arg) % 2 == 0);
    else Serial.println(F("!! uso: mv norte <id|cw|ccw>"));
    return;
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
  pinMode(IR_PIN, INPUT);
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
