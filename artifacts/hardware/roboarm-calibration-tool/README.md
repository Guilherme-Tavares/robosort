# calibration-tool

Ferramenta de calibração do braço robótico MDF (kit genérico, 4 servos SG90)
sobre Arduino Uno R4 WiFi (ou R3), com os servos acionados por um PCA9685 via I²C.

Serve para **mapear os limites mecânicos das juntas** e ensaiar o ciclo de
preensão antes do firmware de produção. Também ensaia a **zona de separação**
(sensor IR + servo empurrador) com o mesmo ciclo do `robosort-firmware`. É
descartável por natureza: o propósito é produzir números.

## Estrutura

```
calibration-tool/calibration-tool.ino   firmware (sketch Arduino)
ds-addon/                               operação por controle DualSense
```

O add-on é opcional e tem [documentação própria](ds-addon/README.md). O firmware
funciona sozinho pelo Monitor Serial.

## Compilar e gravar

Gravado pelo Arduino IDE. Requer a biblioteca **Adafruit PWM Servo Driver
Library** (a `Wire` já vem com o core). Para validar sem gravar:

```
arduino-cli lib install "Adafruit PWM Servo Driver Library"
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi calibration-tool   # R3: arduino:avr:uno
```

A pasta precisa ter o mesmo nome do `.ino`, exigência do Arduino IDE.

Ocupa ~15,7 KB de flash (49%) e 694 bytes de RAM (33%) no Uno.

## Monitor Serial

**115200 baud** e terminação de linha em **Newline** (ou "Both NL & CR"). Com
"No line ending" nenhum comando é processado, porque a linha nunca fecha.

## Ligações

O Arduino fala com o PCA9685 por I²C: **A4 → SDA**, **A5 → SCL**, endereço
`0x40` (padrão do módulo, sem jumpers de endereço soldados). O `VCC` do módulo
vem dos 5 V do Arduino; o `V+` dos servos vem da fonte separada. **GND comum**
entre Arduino, módulo e fonte.

| Junta | Canal do PCA9685 |
|---|---|
| Base | 1 |
| Altura | 5 |
| Alcance | 0 |
| Garra | 4 |
| Empurrador `norte` | 8 |

O sensor IR (FC-51, `LOW` = obstáculo) da zona `norte` vai no **pino digital
2** do Uno.

## Largura de pulso e oscilador

O firmware converte ângulo em pulso de **544 a 2400 µs**, os mesmos valores
padrão da biblioteca `Servo` do Arduino. É isso que mantém válidos os números
da seção *Resultados da calibração*, medidos com a versão anterior deste
firmware, que usava `Servo` direto nos pinos do Uno.

A precisão desse pulso depende do oscilador interno do PCA9685, que a
documentação dá como 25 MHz mas varia por chip; a Adafruit mede algo perto de
27 MHz, valor adotado em `OSC_FREQ`. **Confira uma vez, com o braço em
repouso:**

```
home
mv b 98          a base não deve se mexer ao energizar
```

Se houver solavanco, `OSC_FREQ` está errado para este módulo: um chip mais
lento que o declarado encurta todos os pulsos, e vice-versa. Ajuste, regrave,
repita. Faça o teste na base, que não tem carga de gravidade e tem batentes
rígidos nos dois sentidos; na garra um solavanco pode forçar os dedos.

## Comandos

```
home / dest      declara as 4 juntas em HOME / DELIVERY (não energiza)
set <j> <ang>    declara onde a junta está AGORA (não move, não energiza)
sel <j>          torna ativa e informa o ângulo atual (não move)
mv <j> <ang>     torna ativa, energiza sem salto e vai suave até <ang>
+ / -            move a junta ativa (garra 1 grau, demais 2)
mv home          sequência: base, altura, alcance, garra → HOME
mv dest          sequência: DELIVERY, DROP, garra abre/fecha, volta a DELIVERY
mv area          sequência: canto 0, aproximação, descida, garra fecha
mv norte <id>    ciclo da zona: pré-posição, arma o sensor, empurra e volta
                 (id par = cw, ímpar = ccw; aceita `cw`/`ccw` direto)
stop             interrompe movimento, sequência e ciclo da zona; mantém energizado
off [<j>]        solta a junta indicada (ou `norte`), ou a ativa
offall           solta todas as juntas (pânico)
min / max        registra o ângulo atual como limite da junta ativa
dump             tabela de todas as juntas e do empurrador
? / h            estado da junta ativa / ajuda
```

Juntas por nome ou alias: `base|b`, `garra|g`, `altura|al`, `alcance|ac`.

Prefixos das respostas: `>>` informação, `!!` recusa ou erro, `~~` aviso (o
comando é executado mesmo assim). O único `~~` hoje é o de movimento além do
limite registrado.

Durante um movimento em curso só são aceitos `stop`, `off`, `offall`, `dump`,
`?`, `h`, `+` e `-`. Os demais pedem `stop` antes, porque `sel` e `mv` trocam a
junta ativa, e trocá-la no meio de uma interpolação mandaria os ângulos do
movimento em curso para o servo errado.

## Poses: espelho da produção

O bloco *poses* no topo do sketch tem **os mesmos nomes e colunas** de
`robosort-firmware/config.h`: `ARM_HOME`, `ARM_DELIVERY`, `GRIPPER_OPEN/CLOSED`,
`DROP_HEIGHT/REACH`, `CORNER0_*`, `APPROACH_*` e as pausas da garra. É para
copiar e colar entre os dois arquivos.

O fluxo de ajuste: `home` → `mv home` → `mv area` → `mv dest` → `mv home`, as
mesmas sequências e ordens do firmware de produção. Uma pose errada aparece
aqui, onde dá para corrigir com `+`/`-` e `?` sem regravar a produção; achado o
valor, edite o bloco, regrave a calibração, repita, e só então leve para o
`config.h`. As sequências usam o mesmo interpolador do `mv`, uma junta por vez,
e respeitam os avisos `~~` de limite. `stop` interrompe no meio, inclusive
numa pausa.

Os centros da calibração (coluna *Centro* da tabela abaixo) não estão mais no
sketch: `home` declara em `ARM_HOME`, como na produção.

## Uso típico

Com o braço fisicamente nas posições de repouso:

```
home                 declara as quatro de uma vez
mv al 91             energiza a altura, sem salto
mv ac 96             energiza o alcance; a altura segue firme
+ + +                ajuste fino até encontrar resistência
stop
max                  registra o limite
dump                 copie a saída ANTES de desligar
```

Se o braço não estiver nos centros (foi movido com a mão, houve `offall`, ou
reset por brownout), declare cada junta com `set <junta> <ângulo>` antes do
primeiro `mv`. O firmware recusa mover uma junta de posição desconhecida em vez
de assumir um valor e arriscar o salto.

## Zona de separação

`mv norte <id>` executa, na bancada, o ciclo que o `robosort-firmware` faz
em produção (`prep` → `arm` → `DET` → empurrão → `PUSHED`), com as mesmas
constantes — copiadas de `robosort-firmware/config.h` para o bloco *zona de
separacao* do sketch; ao ajustar lá, ajuste aqui. O sentido vem da paridade
do ID, como no roteamento provisório do orquestrador: **par = cw, ímpar =
ccw**.

```
mv norte 10        >> norte cw: 60 -> 120 -> 60
                   >> norte em 60, sensor armado: passe a caixinha
                   ... caixinha passa no sensor ...
                   DET norte
                   (espera 1 s, vai a 120, segura 1 s, volta a 60, assenta 200 ms)
                   PUSHED norte
                   >> norte de volta em 60
```

Fases: energiza o empurrador **direto na pré-posição** do sentido (sem
pulso ainda, um pulso só; com pulso, interpola a 4 ms/grau) → arma o sensor
→ na detecção (`IR_DEBOUNCE_MS` de nível baixo) espera `PUSHER_DET_DELAY_MS`
(a caixinha anda do sensor até o empurrador) → vai à posição de empurrão →
segura `PUSHER_HOLD_MS` → volta à pré-posição → assenta `PUSHER_SETTLE_MS`
→ `PUSHED`. O empurrador termina onde começou.

O comando é recusado se o sensor **já** estiver em obstáculo (`!! sensor ja
em obstaculo`): é o sintoma de trimpot alto demais (o sensor vê a esteira) ou
de pino solto, e sem essa recusa o empurrão sairia na hora, antes da
caixinha. O `dump` mostra a leitura instantânea do sensor na linha do
empurrador (`sensor livre` / `OBSTACULO`), útil para ajustar o trimpot.

O empurrador é **independente do braço**: tem interpolador próprio, não
entra em `moving`, e `mv norte` é aceito com o braço em movimento — em
produção o sensor fica armado enquanto o braço entrega e volta a HOME. `stop`
desarma e para o empurrador onde estiver, energizado; `off norte` e `offall`
o soltam. O ds-addon ignora a seção do empurrador no `dump`.

## Garantias do firmware

1. **Nenhum sinal chega a servo algum até comando explícito.** O `setup()`
   corta os 16 canais do PCA9685 antes de qualquer outra coisa. Isso é
   necessário porque um reset do Arduino **não** reseta o PCA9685: sem o
   corte, os canais continuariam pulsando na última posição enquanto o
   firmware, recém-reiniciado, os consideraria soltos. Só o primeiro `mv`
   envia pulso a uma junta.
2. **Energização sem salto:** o primeiro pulso enviado a um canal já tem a
   largura do ângulo declarado. No PCA9685 não existe `attach()`; o servo não
   recebe nada até o primeiro `writeMicroseconds()`, e esse primeiro é na
   posição correta. (Na versão anterior, com a biblioteca `Servo`, isso
   exigia `write()` antes de `attach()`; sem essa ordem a saída era ativada
   em 90°, e a armadilha danificou servo em bancada.)
3. **Todo movimento é interpolado** com *smoothstep*, nunca `write()` direto.
   Exceção opcional: a garra, com `GRIPPER_DIRECT 1`, vai num pulso só, para
   fechar de uma vez e morder. O padrão é `0` (interpolada). As juntas que
   carregam o braço são sempre interpoladas.
4. **Movimento não bloqueante:** o `loop()` continua lendo a serial durante o
   deslocamento, então `stop` age de imediato. A leitura é caractere a
   caractere, porque `Serial.readStringUntil()` bloqueia até 1 s e anularia
   essa garantia.
5. **Várias juntas energizadas ao mesmo tempo**, uma delas ativa. Necessário
   para mapear o acoplamento altura↔alcance, onde a altura precisa sustentar
   o braço enquanto o alcance se move.
6. **Interrupção em três níveis:** `stop` (para, mantém energizado), `off`
   (solta uma junta), `offall` (solta todas).

Alterações no sketch devem preservar essas garantias. Todas nasceram de servo
queimado ou quase queimado em bancada.

## Velocidade

```
STEP_DELAY = 8 ms     intervalo entre subpassos
SUBSTEPS   = 4        subpassos por grau
```

Resulta em 32 ms por grau, ou **~30 graus por segundo**. Reduzir `STEP_DELAY`
acelera; aumentar dá mais margem de reação ao procurar um batente.

O add-on espelha esse cálculo na constante `SECONDS_PER_DEGREE`. Mexer em um
exige atualizar o outro.

## Segurança

**Zumbido é sinal de corte imediato.** Servo forçando contra batente zumbe antes
de queimar. Resposta: `offall`.

**A garra é a junta mais perigosa.** Na abertura há batente rígido, mas no
fechamento os dedos apenas se encontram e o servo continua forçando.

**`off` não trava a junta.** Solta significa que ela para de resistir; o que a
segura passa a ser apenas o atrito da caixa de redução, que cede sob carga. Uma
junta parada após `off` não está travada, e não se deve confiar nela para
sustentar o braço.

**Servos nunca no pino 5 V do Arduino.** O `V+` do PCA9685 vem da fonte
separada, com **GND comum** entre fonte, módulo e Arduino.

**No Uno R3, abrir o Monitor Serial reseta o Arduino e solta todas as
juntas** — é o `setup()` cortando os canais do PCA9685; o braço cai para onde
a gravidade o levar. **No Uno R4 (USB nativo) abrir a porta não reseta:** o
firmware e as juntas continuam como estavam, e o menu de ajuda do boot não
aparece; digite `h`.

**Sinal de brownout:** serial corrompida ou menu de ajuda reaparecendo sozinho
indica reset por queda de tensão. Resposta: `offall`.

**PCA9685 ausente:** se o módulo não responder no I²C durante o boot, o
firmware avisa e bloqueia `mv`. Os demais comandos seguem funcionando, mas
nenhum servo se move. Conferir SDA/SCL, VCC do módulo e GND comum, e
reiniciar.

## Resultados da calibração

Medidos com esta ferramenta, neste braço. São posições confortáveis, já com
margem, **não** o ponto onde o batente é encontrado.

Os limites estão no bloco *limites calibrados* no topo do sketch
(`centers[]`, `knownMin[]`, `knownMax[]`), de onde o firmware os carrega no
boot. Esta tabela é a cópia legível; o sketch é o que vale. Ao atualizar um,
atualize o outro.

| Junta | Mín | Centro | Máx | Curso |
|---|---|---|---|---|
| Base | 18 | 98 | 178 | −80 / +80 |
| Altura | 16 | 91 | 136 | −75 / +45 |
| Alcance | 36 | 96 | 176 | −60 / +80 |
| Garra | 82 (fechada) | 82 | 120 (aberta) | 38 |

A base saiu simétrica em torno do centro, indicando horn montado alinhado.
Altura e alcance não: a altura desce 75 graus e sobe 45; o alcance recolhe 60
e estende 80.

**A garra estava com um servo de rotação contínua.** Descoberto em
2026-09-17 e trocado por um de 180°. Num servo contínuo o pulso é velocidade,
não posição: abaixo do ponto de parada gira num sentido, acima gira no outro,
e em torno dele nada acontece. Era exatamente o que se via — "abre de uma vez",
"fecha forçando", "sem efeito entre 81 e 91", "para de zumbir em 86". Toda a
caracterização anterior da garra foi descartada; a linha da tabela acima está
remedida com o servo novo: **82 fechada, 120 aberta, repouso em 82**. Note o
sentido: aqui o mínimo fecha e o máximo abre; depende da montagem do horn, e
por isso o firmware de produção usa constantes próprias (`GRIPPER_OPEN`,
`GRIPPER_CLOSED`) em vez de inferir dos limites. Para referência, no servo
contínuo 84 girava no sentido horário e 91 no anti-horário; o ponto de parada
ficava entre 86 e 90.

## Limitações conhecidas

- **Os limites registrados com `min`/`max` vivem em RAM** e voltam aos valores
  do sketch no reset ou ao regravar. Rodar `dump` e copiar a saída para
  `knownMin[]`/`knownMax[]` a cada ponto de medição, não só ao fim da sessão.
- O firmware aceita 0-180 em qualquer junta, **mesmo depois de limites
  registrados**. É deliberado: durante a calibração é preciso poder ultrapassar
  um limite provisório para refiná-lo. Ultrapassar gera o aviso `~~`, mas o
  movimento acontece.
- O acoplamento altura↔alcance é invisível ao firmware; cada junta é tratada
  isoladamente.
- **O servo não tem retorno de posição.** Se uma junta não executar o comando,
  por contato intermitente ou trava mecânica, o firmware conclui a interpolação
  e reporta sucesso mesmo assim. Não há como detectar isso por software.

## Convenções do sketch

- Código em inglês, comentários em português apenas onde a intenção não é óbvia
- Mensagens da serial **sem acento**, para não depender da codificação do
  Monitor Serial
- Strings literais sempre em `F()`, para mantê-las em flash
- Sem `String`; buffers `char[]` de tamanho fixo, para evitar fragmentação de
  heap no Uno
- Nada de `delay()` no caminho de execução

## Nota

Nada aqui é validado por simulação. Toda medição veio de operação manual com o
braço real. Alterações que afetem energização, amplitude de movimento ou limites
precisam ser testadas em bancada antes de serem consideradas corretas.
