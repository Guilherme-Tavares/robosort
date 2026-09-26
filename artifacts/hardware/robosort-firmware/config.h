#pragma once
// Constantes de hardware e flags de compilacao. Nenhuma logica aqui.
//
// Limites e centros vem de artifacts/hardware/roboarm-calibration-tool; os
// valores-guia dos cantos, de docs/calibration/GUIDE_VALUES.md. O
// orquestrador le tudo isso do firmware ('dump', 'corners') e nao guarda
// copia: ao ajustar, edite aqui e regrave.

// ---- Flags de compilacao ----
// Sobrescrevivel na linha de comando:
//   arduino-cli compile --build-property "build.extra_flags=-DENABLE_SORTING=1" ...
#ifndef ENABLE_SORTING
#define ENABLE_SORTING 1     // 1 habilita sensor IR e servo empurrador
#endif

// ---- PCA9685 ----
#define PCA_ADDR      0x40   // padrao do modulo, sem jumpers de endereco
#define PWM_FREQ_HZ   50

// Oscilador interno: nominal 25 MHz, varia por chip. 27 MHz validado em
// bancada com este modulo (a base nao se move ao energizar no centro).
#define OSC_FREQ      27000000

// Largura de pulso para 0 e 180 graus. Sao os valores padrao da biblioteca
// Servo do Arduino; mante-los e o que faz os limites calibrados valerem.
#define PULSE_MIN_US  544
#define PULSE_MAX_US  2400

// ---- Canais do braco no PCA9685 (0-7 reservados ao braco) ----
// Conferir contra a ferramenta de calibracao: e ela que reflete a fiacao.
#define CH_BASE       1
#define CH_HEIGHT     5
#define CH_REACH      0
#define CH_GRIPPER    4

// ---- Indices das juntas do braco: compartilhados por todas as tabelas ----
#define J_BASE        0
#define J_GRIPPER     1
#define J_HEIGHT      2
#define J_REACH       3
#define ARM_JOINTS    4

// ---- Limites por junta (calibracao) ----
//                                           base  garra  altura  alcance
static const int ARM_MIN[ARM_JOINTS]      = {   18,    82,     16,      36 };
static const int ARM_MAX[ARM_JOINTS]      = {  178,   120,    136,     176 };

// Centros medidos na calibracao: posicao de repouso do braco. Referencia
// para as poses abaixo; o firmware nao os usa diretamente.
static const int ARM_CENTER[ARM_JOINTS]   = {   98,    82,     91,      96 };

// Garra: servo de posicao de 180 graus (o original era de rotacao continua,
// trocado em 2026-09-17). Aberta e fechada sao angulos proprios, nao os
// limites: qual extremo abre depende da montagem do horn. Repouso = fechada,
// na coluna garra das poses abaixo.
#define GRIPPER_OPEN    120
#define GRIPPER_CLOSED   82

// ---- Poses fixas ----
// HOME: de onde o ciclo parte e para onde volta. 'home' declara as juntas
// aqui, entao o braco precisa estar fisicamente nesta pose ao energizar.
// DELIVERY: onde a caixinha e solta sobre a esteira; a garra termina em
// repouso depois de abrir e fechar.
// Provisoriamente iguais aos centros. Ajustar em bancada.
static const int ARM_HOME[ARM_JOINTS]     = {   18,    82,     91,      96 };
static const int ARM_DELIVERY[ARM_JOINTS] = {   96,    82,    110,      84 };

// DROP: de DELIVERY, altura e alcance (nesta ordem) avancam ate o ponto de
// soltura sobre a esteira; a garra abre e fecha; alcance e altura (nesta
// ordem) voltam a DELIVERY, e so entao o braco vai a HOME com seguranca.
// A base nao muda: fica a de DELIVERY.
#define DROP_HEIGHT  96
#define DROP_REACH   74

// ---- Area de aquisicao: valores-guia por canto ----
// Base, altura e alcance para pegar a caixinha no centro de cada celula.
// Cantos: 0 sup-esq, 1 sup-dir, 2 inf-esq, 3 inf-dir (docs/calibration).
// Canto 0 bench-verificado (calibration-tool); 1-3 ainda provisorios.
#define CORNERS 4
static const int CORNER_BASE[CORNERS]     = { 18, 40, 40, 37 };
static const int CORNER_HEIGHT[CORNERS]   = { 22, 37, 22, 31 };
static const int CORNER_REACH[CORNERS]    = { 62, 57, 60, 58 };

// Aproximacao: altura e alcance comuns a todos os cantos, aplicados antes
// de descer ao canto. A ordem (altura antes de alcance na ida, o inverso na
// volta) e a protecao contra o acoplamento do pantografo.
#define APPROACH_HEIGHT 39
#define APPROACH_REACH  78

// ---- Movimento ----
#define STEP_DELAY    8      // ms entre subpassos
#define SUBSTEPS      4      // subpassos por grau (32 ms/grau, ~30 graus/s)
#define SEQ_MAX_STEPS 14     // passos de uma sequencia (mv dest usa 12)

// ---- Latencias de seguranca nas sequencias ----
// Pausas em torno do fechamento da garra: antes, para o braco assentar;
// depois, para a caixinha firmar antes do proximo passo ou comando.
#define GRIP_CLOSE_DELAY_MS  1000   // antes de a garra fechar
#define GRIP_HOLD_DELAY_MS   1000   // depois de fechar: mv area segura o OK, mv dest espera antes do repouso

// ---- Serial ----
#define BAUD_RATE     115200
#define LINE_MAX      32

// ---- Separacao (condicional) ----
// O empurrador tem interpolador proprio, independente do braco: na deteccao
// o firmware o move sozinho, sem esperar o PC nem o Motion. Velocidade do
// module-tester, validada em bancada: rapida, mas suave no arranque e na
// chegada, para empurrar a caixinha em vez de lanca-la.
// Fluxo por caixinha: 'prep' (declara e energiza na pre-posicao do sentido
// decidido) -> braco entrega -> 'arm' logo antes de a garra abrir -> DET -> espera
// PUSHER_DET_DELAY_MS -> empurrao -> segura PUSHER_HOLD_MS -> volta a
// pre-posicao -> assenta -> PUSHED.
// Ex.: cw parte de PRE_CW, vai a PUSH_CW e volta a PRE_CW.
#if ENABLE_SORTING
  // Cinco zonas, uma por regiao: 1 Norte, 2 Nordeste, 3 Centro-Oeste,
  // 4 Sudeste, 5 Sul. Cada uma tem sensor IR (pino digital do Uno; A4/A5 sao
  // o I2C) e empurrador (canal do PCA9685, a partir do 8). Os pinos IR estao
  // confirmados; dos canais, so o da zona 1 foi conferido em bancada.
  #define ZONE_COUNT           5
  #define IR_DEBOUNCE_MS       20    // FC-51: LOW = obstaculo
  #define PUSHER_STEP_DELAY_MS 4     // ms entre subpassos (module-tester)
  #define PUSHER_SUBSTEPS      1     // subpassos por grau: 4 ms/grau, ~250 graus/s
  #define PUSHER_DET_DELAY_MS  400   // da deteccao ao inicio do empurrao: a caixinha anda do sensor ao empurrador
  #define PUSHER_HOLD_MS       1000  // segura o empurrao antes de voltar a pre-posicao
  #define PUSHER_SETTLE_MS     200   // assentamento na volta, antes do PUSHED

  // Posicoes do empurrador, validadas em bancada na zona 1. O repouso e a
  // pre-posicao do sentido horario. Estado 1 do produto gira ccw, estado 2 cw.
  #define PUSHER_NEUTRAL     0      // repouso; 'rest' leva aqui (= PRE_CW)
  #define PUSHER_PRE_CW      0      // pre-posicao para empurrar em sentido horario
  #define PUSHER_PRE_CCW     180    // pre-posicao para anti-horario
  #define PUSHER_PUSH_CW     180    // empurrao horario: 0 -> 180
  #define PUSHER_PUSH_CCW    0      // empurrao anti-horario: 180 -> 0

  // Zonas: nome (usado no protocolo e como nome da junta do empurrador),
  // pino do sensor IR e canal do empurrador no PCA9685. Fonte unica: joints
  // e sorting montam suas tabelas a partir daqui. Pinos e canais bench-
  // verificados no calibration-tool; so a zona 1 foi validada com o braco
  // completo (--arm), as demais so com o empurrador isolado (mv <zona> <ang>).
  #define Z1_NAME "norte"          // regiao 1
  #define Z1_IR    11
  #define Z1_CH    8
  #define Z2_NAME "nordeste"       // regiao 2
  #define Z2_IR    10
  #define Z2_CH    9
  #define Z3_NAME "centro-oeste"   // regiao 3
  #define Z3_IR    9
  #define Z3_CH   10
  #define Z4_NAME "sudeste"        // regiao 4
  #define Z4_IR    6
  #define Z4_CH   12
  #define Z5_NAME "sul"            // regiao 5
  #define Z5_IR    2
  #define Z5_CH   13
#endif
