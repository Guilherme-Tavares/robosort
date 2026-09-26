# orchestrator

Orquestrador do RoboSort: comanda o braço junta a junta pelo
`robosort-firmware`, executa o ciclo de aquisição e, nas próximas fases,
localiza a caixinha por visão e aciona a esteira.

**O PC decide, o Arduino executa.** O firmware move, lê o sensor e confirma;
a sequência do ciclo, a decisão de destino e a visão ficam aqui.

## Estado

Fase atual: **console de operação com ciclo completo de separação**, atrás
de flags em `config.py`:

| Flag | Padrão | Efeito |
|---|---|---|
| `ENABLE_VISION` | on | a câmera identifica o ID da caixinha; sem ela, só `cycle N` |
| `ENABLE_LOCALIZATION` | **off** | a visão localiza a caixinha e o alvo é interpolado; off: alvo fixo em `FIXED_CORNER` (0) |
| `ENABLE_CONVEYOR` | **on** | esteira LEGO pelo EV3, operada pelo console; off (ou `--assume-conveyor`): a esteira sai da jogada e é assumida ligada — pelo brick, por outro PC ou com a caixinha levada à mão |
| `ENABLE_SORTING` | on | prepara e arma o empurrador; exige firmware com `ENABLE_SORTING=1` |

A localização fica desligada até os valores-guia dos quatro cantos voltarem
a ser confiáveis (a remontagem do braço deslocou as posições; só o canto 0
está validado). O código existe e é testado contra o gabarito da folha.

```
config.py         flags, geometria da área, tempos, roteamento provisório
serial_io.py      ligação com o firmware: thread leitora, fila de DET, contrato FIFO
vision.py         câmera, ArUco, identificação, homografia congelada, localização
kinematics.py     interpolação bilinear dos valores-guia
ev3_io.py         esteira contínua pelo EV3 (ev3_dc), ou AssumedConveyor
orchestrator.py   Arm (pick, deliver, go_home, safe_stop), run_sorting_cycle, Runner
main.py           console de operação
requirements.txt
```

## Instalação

No Linux, a partir da raiz do repositório:

```bash
./scripts/instalar-linux.sh
source .venv/bin/activate
```

O procedimento completo, incluindo as permissões de Arduino e EV3, está no
[README principal](../../../README.md#rodar-no-linux). Para instalar somente
este módulo manualmente:

```
python3 -m venv .venv
.venv/bin/python -m pip install -r artifacts/backend/orchestrator/requirements.txt
```

EV3 pela USB (porta `PC` do brick, firmware LEGO original): no Linux o
`ev3_io.py` usa `pyusb`/`libusb` e a regra
`config/udev/99-robosort.rules`. Ligue o brick e conecte o cabo antes de abrir
o console. No Windows, o código usa `hidapi`, sem trocar o driver — nada de
Zadig nem `libusb-1.0.dll`.

## Uso

```
python main.py [--port /dev/ttyACM0] [--camera K] [--echo] [--no-camera] [--assume-conveyor]

python serial_io.py [--port /dev/ttyACM0]   REPL sobre a serial; DET e PUSHED aparecem quando chegam
python vision.py [--camera K]       janela ao vivo: marcadores, homografia, coordenadas
```

O `main.py` conecta, lê a configuração, energiza o braço em HOME e abre o
console. Os ciclos rodam numa thread própria; o console continua respondendo.

```
on / off       liga e desliga a esteira (contínua, velocidade CONVEYOR_SPEED)
vel N          velocidade da esteira em %
start          automático: um ciclo a cada caixinha vista, com a esteira ligada
pause          para de iniciar ciclos (o atual termina)
resume         retoma o automático
cycle N        um ciclo com ID N, ignorando a câmera
status         esteira, modo, ciclos feitos
stop           interrompe, volta a HOME, solta o braço e sai
```

`--assume-conveyor` (ou `ENABLE_CONVEYOR = False`): a esteira sai da jogada
e é assumida ligada desde o início — pelo brick, por outro PC ou com a
caixinha levada à mão até o sensor; `on`/`off` só mudam a suposição.
`--no-camera`: sem visão; só `cycle N`. Se a câmera não abrir, o console
avisa e segue sem ela.

### O ciclo

Condições para o automático iniciar um ciclo: esteira ligada **e** câmera
vendo uma caixinha. `cycle N` ignora as duas (avisa se a esteira estiver
desligada).

1. Identifica o ID pela câmera (mesmo ID em `IDENTIFY_MIN_HITS` frames) ou usa o N de `cycle`
2. Roteia: par → Rondônia → `cw`; ímpar → Acre → `ccw`
3. `prep norte <sentido>` — empurrador declarado e energizado na pré-posição, antes de o
   braço se mover; `PREP_SETTLE_DELAY` para assentar
4. Espera `DELAY_BEFORE_PICK`; pega no canto 0 (ou no alvo interpolado); entrega: avança ao
   ponto de soltura, **`arm norte <sentido>` e só então abre a garra** — o sensor já
   escuta quando a caixinha cai, e antes disso nada deve passar por ele; fecha, recua; HOME
5. Espera `PUSHED norte` — o firmware empurrou sozinho na detecção, segurou 1 s e voltou à
   pré-posição; o `DET` pode ter chegado durante o HOME e fica na fila até aqui

O próximo ciclo só começa com o `PUSHED` recebido e o braço em HOME. Sem
`PUSHED` em `DET_TIMEOUT`, o ciclo é abandonado com mensagem e o sensor
desarmado; o braço já está em HOME.

**Antes de qualquer ciclo o braço precisa estar fisicamente em HOME.** O
orquestrador declara e energiza as juntas lá (`home` + um `mv` por junta ao
próprio home, o equivalente de `mv home`). Não basta declarar: entre
`offall` e a energização o braço cede pela gravidade, e o primeiro pulso
puxa cada junta de volta ao declarado. Feito parado, é um acerto de poucos
graus; feito dentro da primeira sequência, vira um movimento abrupto no meio
dela. Braço fora de HOME significa solavanco maior.

`--builtin` é a referência: executa `mv area`, `mv dest` e `mv home` do
próprio firmware. Se ele funciona e o ciclo junta a junta não, o problema é
do orquestrador.

## De onde vêm os números

**Nenhum limite, pose ou valor-guia vive aqui.** Na conexão, `serial_io`
envia `dump` e `corners` e monta:

- por junta: ângulo atual, estado, `min`, `max`, `home`, `delivery`
- garra: ângulo de `aberta` e de `fechada` (não são os limites: qual extremo
  abre depende da montagem do horn)
- por canto 0-3: `base`, `altura`, `alcance`
- aproximação: `altura`, `alcance`

Fonte única: `robosort-firmware/config.h`. Ajustar lá e regravar. O
orquestrador valida cada `mv` contra os limites lidos antes de enviar; o
firmware valida de novo.

O que fica em `config.py` é política, não calibração: tempos de espera,
geometria da folha (para a visão), mapa de IDs dos marcadores de referência,
regra de roteamento provisória.

## Contrato serial

Implementado em `serial_io.py`, a partir do README do firmware:

- **Uma resposta terminal por comando, na ordem de envio.** Uma thread lê a
  serial e entrega cada linha à resposta pendente mais antiga; `OK` ou
  `ERR` a fecham. Comandos normais são serializados por um lock.
- **`stop`, `off` e `offall` furam o lock**, porque existem para interromper
  um comando em curso. O firmware responde ao interrompido
  (`ERR interrompido`) antes do interruptor (`OK`); a fila de pendentes
  pareia as duas na ordem certa. Testado.
- **`DET` e `PUSHED` são fora de banda:** vão para `Arduino.events` (uma
  `Queue` de `(tipo, zona)`), nunca para uma resposta. `wait_event()` espera
  um deles descartando os outros.
- **Timeout** (`ACK_TIMEOUT`, ou `SEQUENCE_TIMEOUT` para sequências) levanta
  `AckTimeout` e marca a ligação como dessincronizada: comandos são recusados
  até `resync()`, que descarta pendentes, espera a serial silenciar e
  confirma com `ping`.
- **Conexão:** no Uno R3 abrir a porta reseta a placa e o firmware emite
  `READY`; no Uno R4 (USB nativo) não reseta, e o `READY` do boot se perdeu.
  `open()` espera o `READY` e, em paralelo, sonda com `ping` a cada
  `PROBE_INTERVAL`: `OK` também vale como vivo. Depois espera a serial
  silenciar, para nenhuma linha atrasada parear com o primeiro comando.
  `ERR pca` recusa a conexão. A porta é achada pelo VID USB (o R4 aparece
  como "USB Serial Device" genérico no Windows).

## Ciclo

`Arm.pick(base, altura, alcance)`, `Arm.deliver()` e `Arm.go_home()` seguem
exatamente as ordens de `mv area`, `mv dest` e `mv home` do firmware,
validadas em bancada, incluindo as pausas de 1 s antes e depois de a garra
fechar. A diferença é que o alvo de `pick` pode ser interpolado pela visão em
vez de um canto.

`Arm.safe_stop()` é o aborto: `stop`, tentar voltar a HOME, `offall`. Cada
etapa é tentada mesmo se a anterior falhar. Um `offall` com o braço
estendido o deixa cair; ainda assim é melhor que deixá-lo energizado sem
supervisão.

## Convenções

- Código em inglês, comentários e mensagens em português sem acento
- Sem números do braço no código; ver "De onde vêm os números"
- Nada bloqueante na thread leitora
