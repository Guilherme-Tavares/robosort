# calibration-tool

Ferramenta de calibração do braço robótico MDF (kit genérico, 4 servos SG90)
sobre Arduino Uno R3, com os servos acionados por um PCA9685 via I²C.

Serve para **mapear os limites mecânicos das juntas** e ensaiar o ciclo de
preensão antes do firmware de produção. É descartável por natureza: o propósito
é produzir números.

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
arduino-cli compile --fqbn arduino:avr:uno calibration-tool
```

A pasta precisa ter o mesmo nome do `.ino`, exigência do Arduino IDE.

Ocupa ~12,9 KB de flash (40%) e 646 bytes de RAM (31%) no Uno.

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
| Base | 8 |
| Altura | 12 |
| Alcance | 0 |
| Garra | 15 |

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
home             declara as 4 juntas nos centros conhecidos (não energiza)
set <j> <ang>    declara onde a junta está AGORA (não move, não energiza)
sel <j>          torna ativa e informa o ângulo atual (não move)
mv <j> <ang>     torna ativa, energiza sem salto e vai suave até <ang>
+ / -            move a junta ativa (garra 1 grau, demais 2)
stop             interrompe o movimento, mantém energizado
off [<j>]        solta a junta indicada, ou a ativa
offall           solta todas as juntas (pânico)
min / max        registra o ângulo atual como limite da junta ativa
dump             tabela de todas as juntas
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

## Uso típico

Com o braço fisicamente nas posições de repouso:

```
home                 declara as quatro de uma vez
mv al 91             energiza a altura, sem salto
mv ac 116            energiza o alcance; a altura segue firme
+ + +                ajuste fino até encontrar resistência
stop
max                  registra o limite
dump                 copie a saída ANTES de desligar
```

Se o braço não estiver nos centros (foi movido com a mão, houve `offall`, ou
reset por brownout), declare cada junta com `set <junta> <ângulo>` antes do
primeiro `mv`. O firmware recusa mover uma junta de posição desconhecida em vez
de assumir um valor e arriscar o salto.

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

**Abrir o Monitor Serial reseta o Arduino e solta todas as juntas.** É o
`setup()` cortando os canais do PCA9685. O braço cai para onde a gravidade o
levar, então não abra o monitor com o braço erguido ou segurando algo.

**Sinal de brownout:** serial corrompida ou menu de ajuda reaparecendo sozinho
indica reset por queda de tensão. Resposta: `offall`.

**PCA9685 ausente:** se o módulo não responder no I²C durante o boot, o
firmware avisa e bloqueia `mv`. Os demais comandos seguem funcionando, mas
nenhum servo se move. Conferir SDA/SCL, VCC do módulo e GND comum, e
reiniciar.

## Resultados da calibração

Medidos com esta ferramenta, neste braço. São posições confortáveis, já com
margem, **não** o ponto onde o batente é encontrado.

Os mesmos valores estão no bloco *limites calibrados* no topo do sketch
(`centers[]`, `knownMin[]`, `knownMax[]`), de onde o firmware os carrega no
boot. Esta tabela é a cópia legível; o sketch é o que vale. Ao atualizar um,
atualize o outro.

| Junta | Mín | Centro | Máx | Curso |
|---|---|---|---|---|
| Base | 18 | 98 | 178 | −80 / +80 |
| Altura | 16 | 91 | 136 | −75 / +45 |
| Alcance | 56 | 116 | 176 | −60 / +60 |
| Garra | 84 | 90 | 91 | ~7 |

Base e alcance saíram simétricos em torno do centro, indicando horns montados
alinhados. A altura não: ela desce 75 graus e sobe 45.

**Banda morta da garra.** Abaixo de 84 abre de uma vez; acima de 91 os dedos
já se encontraram e o servo passa a forçar; entre 84 e 91 não há efeito
algum. Ou seja, todo o curso registrado é folga mecânica acumulada no trem de
ligações de MDF, não defeito do servo. Na prática a garra não tem uma faixa,
tem três posições: 84 (aberta), 91 (fechada, forçando) e 90, onde o servo
para de zumbir. Esse 90 é o centro da tabela e existe para aliviar a pressão
sem abrir; é o que o Triângulo do add-on envia. Consequência: "fechada
segurando a caixa sem esmagar" não é alcançável por ângulo; a pressão depende
de quanto o servo continua forçando após o contato.

**Envelope altura↔alcance, incompleto.** Os limites das duas juntas são
interdependentes: uma dada altura restringe a faixa segura de alcance, e
vice-versa. Método adotado: fixar o alcance e mapear a faixa de altura.

| Alcance | Altura mín | Altura máx |
|---|---|---|
| 56 (recolhido) | pendente | pendente |
| 116 (centro) | 16 | 136 |
| 176 (estendido) | pendente | pendente |

O firmware de produção deve validar combinações contra esse envelope antes de
enviá-las aos servos.

## Limitações conhecidas

- **Os limites registrados com `min`/`max` vivem em RAM** e voltam aos valores
  do sketch no reset ou ao regravar. Rodar `dump` e copiar a saída para
  `knownMin[]`/`knownMax[]` a cada ponto de medição, não só ao fim da sessão.
- O firmware aceita 0-180 em qualquer junta, **mesmo depois de limites
  registrados**. É deliberado: durante a calibração é preciso poder ultrapassar
  um limite provisório para refiná-lo. Ultrapassar gera o aviso `~~`, mas o
  movimento acontece.
- `dump` guarda um par mín/máx por junta, insuficiente para a grade do envelope,
  que precisa de uma tripla por ponto de alcance. O registro da grade é manual.
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
