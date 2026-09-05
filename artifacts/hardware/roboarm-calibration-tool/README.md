# calibration-tool

Ferramenta de calibração do braço robótico MDF (kit genérico, 4 servos SG90)
sobre Arduino Uno R3.

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

Gravado pelo Arduino IDE. Requer a biblioteca **Servo**. Para validar sem
gravar:

```
arduino-cli lib install Servo
arduino-cli compile --fqbn arduino:avr:uno calibration-tool
```

A pasta precisa ter o mesmo nome do `.ino`, exigência do Arduino IDE.

Ocupa ~9,5 KB de flash (29%) e 458 bytes de RAM (22%) no Uno.

## Monitor Serial

**115200 baud** e terminação de linha em **Newline** (ou "Both NL & CR"). Com
"No line ending" nenhum comando é processado, porque a linha nunca fecha.

## Pinagem

| Junta | Pino |
|---|---|
| Base | 3 |
| Altura | 5 |
| Alcance | 9 |
| Garra | 11 |

Todos digitais com PWM.

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

1. **Nenhum sinal chega a servo algum até comando explícito.** Conectar o servo
   fisicamente não o energiza; só o primeiro `mv` chama `attach()`.
2. **Energização sem salto:** `write()` antes de `attach()`. Sem isso a
   biblioteca Servo ativa a saída em 90°, movendo a junta antes de qualquer
   comando. Essa armadilha já danificou servo em bancada.
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

**Servos nunca no pino 5 V do Arduino.** Alimentação separada, com **GND comum**
entre fonte, servos e Arduino.

**Sinal de brownout:** serial corrompida ou menu de ajuda reaparecendo sozinho
indica reset por queda de tensão. Resposta: `offall`.

## Resultados da calibração

Medidos com esta ferramenta, neste braço. São posições confortáveis, já com
margem, **não** o ponto onde o batente é encontrado.

| Junta | Mín | Centro | Máx | Curso |
|---|---|---|---|---|
| Base | 18 | 98 | 178 | −80 / +80 |
| Altura | 16 | 91 | 136 | −75 / +45 |
| Alcance | 56 | 116 | 176 | −60 / +60 |
| Garra | 88 | 92 | 96 | ~8 |

Base e alcance saíram simétricos em torno do centro, indicando horns montados
alinhados. A altura não: ela desce 75 graus e sobe 45.

**Banda morta da garra.** Abaixo de 88 abre de uma vez, acima de 96 fecha de uma
vez, e entre 90 e 95 não há efeito algum. É folga mecânica acumulada no trem de
ligações de MDF, não defeito do servo. Consequência: "fechada segurando a caixa
sem esmagar" não é alcançável por ângulo; a pressão depende de quanto o servo
continua forçando após o contato.

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

- **Os limites registrados com `min`/`max` vivem em RAM** e somem no reset ou ao
  regravar. Rodar `dump` e copiar a saída a cada ponto de medição, não só ao fim
  da sessão.
- O firmware aceita 0-180 em qualquer junta, **mesmo depois de limites
  registrados**. É deliberado: durante a calibração é preciso poder ultrapassar
  um limite provisório para refiná-lo.
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
