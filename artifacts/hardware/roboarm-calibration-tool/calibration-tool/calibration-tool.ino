#include <Servo.h>

// Calibracao assistida de braco robotico MDF com quatro servos.
// Firmware descartavel: existe para mapear limites e ensaiar o ciclo de
// preensao. Varias juntas ficam energizadas ao mesmo tempo; uma delas e a
// ativa, alvo de + e -. Energizacao sem salto, movimento interpolado e
// interrompivel a qualquer instante.

#define BASE_PIN    3
#define HEIGHT_PIN  5
#define REACH_PIN   9
#define GRIPPER_PIN 11

#define JOINT_COUNT 4
#define J_BASE      0
#define J_GRIPPER   1
#define J_HEIGHT    2
#define J_REACH     3

#define STEP_DELAY  8    // ms entre subpassos
#define SUBSTEPS    4    // subpassos por grau

#define LINE_MAX    32

const char* names[]   = {"base", "garra", "altura", "alcance"};
const char* alias1[]  = {"b",    "g",     "al",     "ac"};
const int   pins[]    = {BASE_PIN, GRIPPER_PIN, HEIGHT_PIN, REACH_PIN};

// Centros medidos em bancada. Usados por 'home' para declarar as juntas de
// uma vez, quando o braco esta na posicao de repouso.
const int centers[]   = {98, 92, 91, 116};

// Passo do ajuste fino. A garra tem curso util de poucos graus e folga
// mecanica no meio, entao 2 graus la e grosseiro demais.
const int stepSize[]  = {2, 1, 2, 2};

Servo servos[JOINT_COUNT];
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

// ---------------------------------------------------------------- movimento

// Interpolacao smoothstep: 3t^2 - 2t^3. Arranque e chegada suaves.
int easedAngle(int stepIndex) {
  float t = (float)stepIndex / moveTotal;
  float eased = t * t * (3.0f - 2.0f * t);
  return moveStart + (int)((moveTarget - moveStart) * eased);
}

void startMove(int target) {
  target = constrain(target, 0, 180);
  if (target == angles[current]) { servos[current].write(target); return; }

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
  servos[current].write(easedAngle(moveStep));

  if (moveStep >= moveTotal) {
    servos[current].write(moveTarget);
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
  if (liveAt[j]) servos[j].detach();
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
  if (limitMin[j] < 0) Serial.print(F("?")); else Serial.print(limitMin[j]);
  Serial.print(F(".."));
  if (limitMax[j] < 0) Serial.print(F("?")); else Serial.print(limitMax[j]);
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

    if (!liveAt[j] && !declaredAt[j]) {
      Serial.print(F("!! ")); Serial.print(names[j]);
      Serial.println(F(" tem posicao desconhecida"));
      Serial.println(F("   use 'home' (braco em repouso) ou 'set <junta> <ang>'"));
      return;
    }

    current = j;
    if (!liveAt[j]) {
      servos[j].write(angles[j]);        // antes do attach, evita o salto para 90
      servos[j].attach(pins[j]);
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
    limitMin[i]   = -1;
    limitMax[i]   = -1;
  }
  help();
  Serial.println(F("Nenhuma junta energizada. Sistema em repouso."));
}

void loop() {
  pollSerial();
  updateMove();
}
