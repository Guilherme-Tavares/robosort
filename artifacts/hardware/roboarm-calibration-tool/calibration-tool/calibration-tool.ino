#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// Calibracao assistida de braco robotico MDF com quatro servos, acionados
// por um PCA9685 via I2C. Firmware descartavel: existe para mapear limites e
// ensaiar o ciclo de preensao. Varias juntas ficam energizadas ao mesmo tempo;
// uma delas e a ativa, alvo de + e -. Energizacao sem salto, movimento
// interpolado e interrompivel a qualquer instante.

#define PCA_ADDR    0x40
#define BASE_CH     8
#define HEIGHT_CH   12
#define REACH_CH    0
#define GRIPPER_CH  15

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

const char* names[]    = {"base", "garra", "altura", "alcance"};
const char* alias1[]   = {"b",    "g",     "al",     "ac"};
const int   channels[] = {BASE_CH, GRIPPER_CH, HEIGHT_CH, REACH_CH};

// ------------------------------------------------------ limites calibrados
// Tabela "Resultados da calibracao" do README, na ordem base, garra, altura,
// alcance. Sao posicoes confortaveis, com margem; nao o ponto do batente.
// Apos cada sessao, copie o 'dump' para ca. -1 em min/max = desconhecido.
//
// 'home' declara as juntas nos centros. min/max nao bloqueiam movimento (e
// preciso poder ultrapassar um limite para refina-lo), mas geram aviso '~~'.
//                          base  garra  altura  alcance
const int centers[]   = {   98,    90,     91,     116 };
const int knownMin[]  = {   18,    84,     16,      56 };
const int knownMax[]  = {  178,    91,    136,     176 };

// Passo do ajuste fino. A garra tem curso util de poucos graus e folga
// mecanica no meio, entao 2 graus la e grosseiro demais.
const int stepSize[]  = {2, 1, 2, 2};

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
  current = -1;
  Serial.println(F(">> TUDO SOLTO."));
}

// Declara as quatro juntas nos centros de bancada, sem energizar. Vale apenas
// se o braco estiver de fato em repouso; se foi movido com a mao, use 'set'.
void home() {
  for (int i = 0; i < JOINT_COUNT; i++) {
    if (liveAt[i]) continue;              // junta energizada ja tem posicao real
    angles[i]     = centers[i];
    declaredAt[i] = true;
  }
  Serial.println(F(">> juntas declaradas nos centros (nao energizadas):"));
  Serial.print(F("  "));
  for (int i = 0; i < JOINT_COUNT; i++) {   // da tabela, nao de texto fixo
    Serial.print(F(" ")); Serial.print(names[i]);
    Serial.print(F(" "));  Serial.print(centers[i]);
  }
  Serial.println();
  Serial.println(F("   confira se o braco esta mesmo assim antes do primeiro 'mv'"));
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

// Tabela completa: e daqui que saem as constantes do firmware de producao.
// Mostrar todas as juntas de uma vez e o que permite registrar a tripla
// (altura, alcance_min, alcance_max) do envelope de acoplamento.
void dumpAll() {
  Serial.println();
  Serial.println(F("=== juntas ==="));
  Serial.println(F("  nome\tang\testado\tlimites"));
  for (int i = 0; i < JOINT_COUNT; i++) printJointLine(i);
  Serial.println();
}

void report() {
  if (current < 0) { Serial.println(F(">> nenhuma junta ativa")); return; }
  printJointLine(current);
}

void help() {
  Serial.println();
  Serial.println(F("=== Calibracao do braco ==="));
  Serial.println(F("  home           declara as 4 juntas nos centros (nao energiza)"));
  Serial.println(F("  set <j> <ang>  declara onde a junta esta AGORA (nao move)"));
  Serial.println(F("  sel <j>        torna ativa e informa o angulo atual (nao move)"));
  Serial.println(F("  mv <j> <ang>   torna ativa, energiza sem salto e vai ate <ang>"));
  Serial.println(F("  + / -          move a junta ativa (garra 1 grau, demais 2)"));
  Serial.println(F("  stop           interrompe o movimento, mantem energizado"));
  Serial.println(F("  off [<j>]      solta a junta indicada, ou a ativa"));
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
  if (!strcmp(cmd, "stop"))   { abortMove();  return; }
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
    int j = findJoint(n);
    if (j < 0) { Serial.println(F("!! junta invalida")); return; }
    releaseJoint(j);
    return;
  }

  // Ajuste fino redireciona o movimento em curso em vez de recusar
  if (!strcmp(cmd, "+") || !strcmp(cmd, "-")) {
    if (current < 0)      { Serial.println(F("!! nenhuma junta ativa")); return; }
    if (!liveAt[current]) { Serial.println(F("!! junta nao energizada; use 'mv'")); return; }
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

  if (!strcmp(cmd, "home")) {
    if (moving) { Serial.println(F("!! em movimento; use 'stop'")); return; }
    home();
    return;
  }

  if (moving) { Serial.println(F("!! em movimento; use 'stop' ou 'off'")); return; }

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
  for (int i = 0; i < JOINT_COUNT; i++) {
    angles[i]     = centers[i];
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
}
