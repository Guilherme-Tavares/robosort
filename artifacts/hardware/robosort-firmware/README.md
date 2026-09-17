# robosort-firmware

Firmware de produção do Arduino Uno R3. Move as juntas do braço através do
PCA9685, lê o sensor de presença e confirma cada comando ao concluí-lo.

**O PC decide, o Arduino executa.** Em produção o orquestrador
(`artifacts/backend/orchestrator`) comanda junta a junta. As sequências de
bancada (`mv home`, `mv dest`, `mv area`) existem para testar o braço sem o
orquestrador, pelo Monitor Serial.

## Estrutura

```
robosort-firmware.ino   setup() e loop()
config.h                constantes de hardware, limites, poses, valores-guia e flags
joints.h / .cpp         as juntas sobre o PCA9685: nome, alias, canal, limites, estado
motion.h / .cpp         interpolação smoothstep não bloqueante, uma junta por vez
sequence.h / .cpp       lista de passos sobre o motion; toda movimentação passa aqui
protocol.h / .cpp       parsing serial e respostas
sorting.h / .cpp        sensor IR por zona                [ENABLE_SORTING]
```

## Compilar e gravar

Bibliotecas: `Wire` (core) e **Adafruit PWM Servo Driver Library** 3.0.x.

```
arduino-cli lib install "Adafruit PWM Servo Driver Library"
arduino-cli compile --fqbn arduino:avr:uno robosort-firmware
```

Com o módulo de separação:

```
arduino-cli compile --fqbn arduino:avr:uno \
  --build-property "build.extra_flags=-DENABLE_SORTING=1" robosort-firmware
```

Ocupa ~13,8 KB de flash e 728 bytes de RAM sem separação; ~16,1 KB e 823
bytes com.

## Ligações

I²C do Uno para o PCA9685: **A4 → SDA**, **A5 → SCL**, endereço `0x40`. `VCC`
do módulo nos 5 V do Arduino; `V+` dos servos na fonte separada. **GND comum**
entre Arduino, módulo e fonte.

| Junta | Alias | Canal |
|---|---|---|
| base | b | 1 |
| altura | al | 5 |
| alcance | ac | 0 |
| garra | g | 4 |
| norte (empurrador) | | 8 |

Canais 0-7 reservados ao braço; empurradores a partir do 8. Sensor IR da zona
Norte no pino digital 2.

## Constantes em `config.h`

Tudo que se ajusta sem tocar na lógica. O orquestrador lê estes valores do
firmware (`dump`, `corners`) e **não guarda cópia**: ao ajustar, edite aqui e
regrave.

| Tabela | O que é |
|---|---|
| `ARM_MIN` / `ARM_MAX` | limites por junta, da calibração; movimento fora deles é rejeitado |
| `ARM_CENTER` | centros da calibração; referência, não usado diretamente |
| `GRIPPER_OPEN` / `GRIPPER_CLOSED` | ângulos de garra aberta e fechada; **não** são os limites, o sentido depende do horn |
| `ARM_HOME` | posição inicial: de onde o ciclo parte e para onde volta |
| `ARM_DELIVERY` | posição de entrega sobre a esteira; a coluna garra é o repouso após soltar |
| `PUSHER_*` | neutro, pré-posições e posições de empurrão por sentido; velocidade e `PUSHER_SETTLE_MS` |
| `CORNER_BASE/HEIGHT/REACH` | valores-guia dos quatro cantos da área (`docs/calibration/GUIDE_VALUES.md`) |
| `APPROACH_HEIGHT/REACH` | altura e alcance de aproximação, comuns aos cantos |
| `GRIP_CLOSE_DELAY_MS` / `GRIP_HOLD_DELAY_MS` | pausas de 1 s antes e depois de a garra fechar |

A garra é um servo de posição de 180° (o original era de rotação contínua,
trocado em 2026-09-17). Hoje `GRIPPER_CLOSED` 82 é o mínimo e `GRIPPER_OPEN`
120 o máximo; repouso = fechada, na coluna garra das poses.

`ARM_HOME` e `ARM_DELIVERY` começam iguais aos centros. Ajuste em bancada.

## Protocolo serial

**115200 baud**, terminação `\n`. Mensagens sem acento. Juntas por nome ou
alias.

```
PC → Arduino
  mv <junta> <ang>     move até o ângulo, com interpolação; OK ao concluir
  mv <junta>           mostra a junta (STATE) e a torna ativa
  mv home              vai a HOME: base, altura, alcance, garra
  mv dest              vai a DELIVERY: alcance, altura, base; garra abre, fecha, repousa
  mv area <0-3>        pega a caixinha no canto (ver Sequências)
  sel <junta>          torna a junta ativa e a mostra
  + / -                move a junta ativa um passo (garra 1°, demais 2°)
  set <junta> <ang>    declara a posição atual (0-180); não move, não energiza
  home                 declara as juntas em ARM_HOME e mostra todas
  dest                 declara as juntas em ARM_DELIVERY e mostra todas
  off [<junta>]        solta a junta, ou a ativa; posição passa a desconhecida
  offall               solta todas (pânico)
  stop                 interrompe o movimento em curso, mantém energizado
  dump                 STATE de cada junta, depois GRIPPER
  ?                    STATE da junta ativa
  corners              CORNER de cada canto e APPROACH
  ping                 OK
  help / h             ajuda, em linhas '#'

  [ENABLE_SORTING]
  prep <zona> <cw|ccw> empurrador na pré-posição do sentido; OK ao chegar
  arm <zona> <cw|ccw>  arma o sensor com o sentido; na detecção o firmware empurra sozinho
  push <zona> <cw|ccw> empurrão manual; OK ao chegar
  rest <zona>          empurrador ao neutro; OK ao chegar
  disarm <zona>        desarma o sensor

Arduino → PC
  READY                fim do setup(); PCA9685 respondeu
  OK                   comando concluído
  ERR <motivo>         comando rejeitado ou interrompido
  STATE <junta> <ang|?> <solta|declarada|energizada> <min> <max> <home> <delivery>
  CORNER <k> <base> <altura> <alcance>
  APPROACH <altura> <alcance>
  GRIPPER <aberta> <fechada>
  # <texto>            informação para o operador; o PC ignora
  DET <zona>           sensor detectou; empurrão já em curso        [assíncrono]
  PUSHED <zona>        empurrador chegou e assentou (PUSHER_SETTLE_MS) [assíncrono]
```

Juntas: `base|b`, `garra|g`, `altura|al`, `alcance|ac`; com separação, também
`norte` (o empurrador). Zona: `norte`.

### Contrato de respostas

**Cada comando recebe exatamente uma resposta terminal, `OK` ou `ERR`, na
ordem em que foi enviado.** Linhas `STATE`, `GRIPPER`, `CORNER`, `APPROACH` e `#` que
precedem o `OK` pertencem ao mesmo comando. É o que permite ao orquestrador
parear resposta com comando sem heurística.

- Comandos que movem (`mv` com ângulo, `mv home/dest/area`, `+`, `-`,
  `prep`, `push`, `rest`) respondem **ao concluir** o movimento inteiro, não
  ao receber o comando. Com firmware não bloqueante, "concluir" é a máquina de estados
  chegar ao fim, não o retorno de uma função.
- Com movimento em curso, só `stop`, `off`, `offall` e `ping` são aceitos. O
  resto, inclusive `dump`, recebe `ERR ocupado` de imediato. Sem essa regra,
  um `dump` no meio de um `mv` produziria um `OK` que o PC atribuiria ao
  comando errado.
- Um comando que interrompe o movimento (`stop`, `off` na junta em movimento,
  `offall`) faz o comando pendente responder `ERR interrompido` **antes** da
  própria resposta. O PC vê duas linhas, na ordem dos comandos.
- `DET` e `PUSHED` são as linhas fora desse fluxo: assíncronas, prefixadas,
  podem chegar entre um comando e sua resposta. O leitor serial do PC roteia
  por prefixo.

### Sequências

Toda movimentação é uma sequência de passos, executada um passo por vez. Um
passo é um movimento `(junta, ângulo)` ou uma pausa em ms. **A validação é
feita inteira antes de mover:** se qualquer passo cair fora dos limites ou
numa junta de posição desconhecida, o `ERR` sai na hora e nada anda. Passos
cujo destino já é a posição corrente são pulados; uma junta declarada é
energizada no caminho, sem salto. Pausas não bloqueiam: `stop` age no meio
delas como no meio de um movimento.

| Comando | Passos |
|---|---|
| `mv <j> <ang>`, `+`, `-` | um |
| `mv home` | base, altura, alcance, garra → `ARM_HOME` |
| `mv dest` | alcance, altura, base → `ARM_DELIVERY`; garra → `GRIPPER_OPEN`; **pausa 1 s**; garra → `GRIPPER_CLOSED`; **pausa 1 s**; garra → `ARM_DELIVERY` (repousa) |
| `mv area <k>` | base → `CORNER_BASE[k]`; garra → `GRIPPER_OPEN`; altura → `APPROACH_HEIGHT`; alcance → `APPROACH_REACH`; altura → `CORNER_HEIGHT[k]`; alcance → `CORNER_REACH[k]`; **pausa 1 s**; garra → `GRIPPER_CLOSED`; **pausa 1 s** |

O empurrador não passa pelo `motion` nem por sequências: tem interpolador
próprio em `sorting`, com velocidade própria.

As sequências exigem juntas declaradas: `home` ou `dest` antes do primeiro
`mv home`/`mv dest`/`mv area`, com o braço fisicamente na pose declarada.

As pausas são latências de segurança em torno do fechamento da garra: 1 s
antes, para o braço assentar; 1 s depois, para a caixinha firmar. Em `mv
area` a pausa final segura o `OK`, e com ele o próximo comando; em `mv dest`
ela precede o repouso. Valores em `GRIP_CLOSE_DELAY_MS` e
`GRIP_HOLD_DELAY_MS`.

A ordem em `mv area` (altura antes de alcance na ida) e em `mv dest` (alcance
antes de altura na volta) é a proteção contra o acoplamento do pantógrafo. Não
há validação combinada altura↔alcance; toda combinação que estas sequências
produzem foi testada nessa ordem.

### Junta ativa

`mv <junta>` e `sel <junta>` tornam a junta ativa; `mv <junta> <ang>` também.
`+`, `-`, `?` e `off` sem argumento operam nela. `offall` limpa. É o mesmo
modelo da ferramenta de calibração, para o Monitor Serial; o orquestrador
sempre nomeia a junta.

### Motivos de `ERR`

| Motivo | Quando |
|---|---|
| `comando` | não reconhecido |
| `junta` / `zona` / `canto` | nome ou índice inválido |
| `ativa` | `+`, `-`, `?` ou `off` sem junta ativa |
| `angulo` | não numérico ou fora de 0-180 |
| `sintaxe` | `push` sem `cw`/`ccw` |
| `limite <junta> <min>..<max>` | algum passo fora dos limites da junta |
| `desconhecida <junta>` | algum passo em junta solta; use `home`, `dest` ou `set` antes |
| `energizada` | `set` em junta com pulso; use `off` antes |
| `ocupado` | comando bloqueante com movimento em curso |
| `interrompido` | resposta ao comando de movimento que foi abortado |
| `pca` | PCA9685 não respondeu no boot |

### Limites

Movimento fora de `ARM_MIN`/`ARM_MAX` é rejeitado. É a última linha de
defesa: o orquestrador também valida, mas um bug no PC não pode forçar servo
contra batente. Não há `min`/`max` em tempo de execução como na calibração:
em produção os limites são constantes.

`set` aceita 0-180, fora dos limites inclusive. Uma junta que caiu (brownout,
`offall` com carga) pode estar fisicamente fora da faixa, e `set` é a única
forma de declará-la para que um `mv` a traga de volta.

## Garantias

Herdadas da ferramenta de calibração (`roboarm-calibration-tool`), onde
nasceram de servo queimado ou quase. Não podem regredir.

1. **Nenhum sinal chega a servo algum até comando explícito.** O `setup()`
   corta os 16 canais do PCA9685 antes de tudo. Reset do Arduino, inclusive o
   de abrir o Monitor Serial, **não** reseta o PCA9685; sem o corte os canais
   continuariam pulsando na última posição com o firmware sem saber.
2. **Energização sem salto.** O primeiro pulso a um canal já tem a largura da
   posição declarada. Junta de posição desconhecida não recebe movimento.
3. **Todo movimento é interpolado** com *smoothstep*.
4. **Não bloqueante.** Leitura serial caractere a caractere; `stop` age de
   imediato, no meio de qualquer sequência.
5. **Várias juntas energizadas, uma se move por vez.**
6. **Interrupção em três níveis:** `stop`, `off [<junta>]`, `offall`.
7. **PCA9685 ausente é detectado no boot:** `ERR pca` em vez de `READY`, e
   movimentos recusados.

Consequência de 1: **abrir o Monitor Serial solta o braço.** Não abrir com o
braço erguido ou segurando algo.

## Uso pelo Monitor Serial

Ciclo completo de bancada, validado em todos os cantos, com o braço
fisicamente em `ARM_HOME` e uma caixinha no canto 0:

```
home              declara as quatro e mostra
mv home           energiza todas na pose inicial, sem salto
mv area 0         base, garra abre, aproxima, desce, 1 s, fecha, 1 s
mv dest           leva a esteira, abre, 1 s, fecha, 1 s, repousa
mv home           volta
offall
```

O `mv home` logo após `home` energiza as quatro juntas de uma vez, na pose
em que já estão; sem ele, cada junta seria energizada no meio da primeira
sequência que a usasse, o que também funciona, mas é menos previsível.

Ajuste fino de uma junta:

```
mv ac             mostra o alcance e o torna ativo
+ +               dois passos
?                 onde ficou
```

## Separação: sensor e empurrador

O empurrador tem **interpolador próprio**, independente do `motion` do
braço: se move mesmo com o braço no meio de uma sequência. É o que permite
ao firmware empurrar sozinho na detecção — a zona Norte fica no começo da
esteira e a caixinha chega ao sensor antes de o braço terminar de voltar a
HOME.

O movimento é *smoothstep* a **4 ms por grau** (`PUSHER_STEP_DELAY_MS`,
`PUSHER_SUBSTEPS`), a velocidade validada no `module-tester`: rápido o
bastante para empurrar, suave o bastante para não lançar a caixinha. O braço
anda a 32 ms por grau.

Fluxo por caixinha, comandado pelo PC antes de o braço se mover:

```
prep norte cw      pré-posição do sentido decidido (PUSHER_PRE_CW / _CCW)
arm norte cw       sensor armado com o sentido
                   ... braço pega, entrega, volta ...
DET norte          o firmware começou a mover o empurrador para PUSHER_PUSH_CW / _CCW
PUSHED norte       chegou e assentou (PUSHER_SETTLE_MS); o PC pode iniciar o próximo ciclo
```

Uma detecção por `arm`: o sensor desarma ao disparar. Debounce de
`IR_DEBOUNCE_MS` de nível baixo contínuo. `offall` desarma tudo. O
empurrador fica na posição de empurrão até o próximo `prep` (ou `rest`).

Constantes em `config.h`, **a definir em bancada**: `PUSHER_NEUTRAL`,
`PUSHER_PRE_CW`, `PUSHER_PRE_CCW`, `PUSHER_PUSH_CW`, `PUSHER_PUSH_CCW`,
`PUSHER_SETTLE_MS`. A princípio a pré-posição para `cw` é o neutro.

O empurrador é uma junta comum da tabela, com o nome da zona: `mv norte 90`
funciona e serve para achar as posições. Na primeira energização não há de
onde interpolar: o firmware manda um pulso direto para a posição pedida; sem
carga, um eventual salto é inofensivo.

## Convenções

- Código em inglês, comentários em português sem acento
- Mensagens da serial sem acento
- Strings literais em `F()`; buffers `char[]` de tamanho fixo; sem `String`
- Nada de `delay()` no caminho que lê serial ou sensor
