# ds-addon

Operação do braço de calibração por controle DualSense.

Orquestrador provisório: aciona o firmware de
[`calibration-tool/calibration-tool.ino`](../calibration-tool/calibration-tool.ino) pelo controle,
durante a fase de calibração.

**Só fala o protocolo da ferramenta de calibração.** Não funciona com o
`robosort-firmware` de produção, que responde `OK`/`ERR`/`STATE` e rejeita
movimento fora dos limites; lá o L2 não teria efeito. Para testar produção sem o
orquestrador, use `mv home`, `mv dest` e `mv area` pelo Monitor Serial.

## Serial

O firmware roda a **115200 baud**. O Monitor Serial do Arduino IDE precisa
estar nessa velocidade, ou a saída vira lixo.

## Instalação

```
pip install -r requirements.txt
```

`pygame-ce` no lugar do `pygame`: em Python 3.14 o `pygame` ainda não publica
wheel e tenta compilar do código-fonte. A API é a mesma.

## Uso

```
python main.py                 conecta ao Arduino e opera
python main.py --port COM5     força a porta serial
python main.py --dry-run       sem Arduino; só imprime os comandos
python main.py --quiet         não ecoa o tráfego serial

python diag.py                 visão ao vivo de eixos e botões
python diag.py --check         verificação guiada do mapeamento
```

Comece pelo `--dry-run` sempre que mexer no código: ele exercita o controle
inteiro sem energizar nada.

## Controles

| Controle | Ação |
|---|---|
| Options | liga os servos e leva as juntas às posições iniciais |
| R2 + Options | solta todos os servos e encerra |
| Analógico esquerdo, eixo X | base: esquerda aumenta, direita diminui |
| Analógico direito, eixo Y | altura: cima aumenta, baixo diminui |
| R1 + analógico direito | alcance: baixo aumenta, cima diminui |
| L1 (segurando) | precisão: passo de 1 grau, 200 ms entre comandos |
| L2 (segurando) | desativa os limites calibrados; vale só a faixa 0-180 |
| Quadrado | alterna a garra entre o mínimo e o máximo (hoje 82 fechada, 120 aberta) |
| Triângulo | garra vai ao centro a partir do máximo; com centro = fechada, equivale a fechar |

Sem L1, o passo é de 2 graus e a cadência acompanha a confirmação do firmware
(~64 ms por passo, ~30 graus/s). O clamp para exatamente no limite: se faltar
1 grau, o passo vira 1.

A velocidade vem de `STEP_DELAY` no sketch (8 ms por subpasso, 4 subpassos por
grau). Mexer nela exige atualizar `SECONDS_PER_DEGREE` em `main.py`, que
espelha o mesmo cálculo.

## Antes de apertar Options

**O braço precisa estar fisicamente nas posições iniciais.** O script as
imprime ao conectar; é a pose `ARM_HOME` que o firmware declara em `home`.
Energizar um servo o puxa à força até o ângulo declarado, sem interpolação.
Braço longe do centro significa solavanco em cada junta.

## Limites

Centros e limites **não ficam neste script**: ele os lê do firmware ao
conectar, com `home` seguido de `dump`. A tabela de calibração vive apenas
no bloco *limites calibrados* do sketch; editar lá e regravar basta.

Se o `dump` trouxer `?` em algum centro ou limite, a conexão é recusada com
a mensagem correspondente. O modo seco (`--dry-run`) usa uma tabela fictícia
própria, que não precisa acompanhar o sketch.

O clamp usa os limites tal como vêm, sem margem adicional.

**L2 segurado desativa o clamp** e deixa valer só a faixa 0-180 do firmware,
que avisa (`~~`) mas obedece. É o caminho para refinar um limite pelo
controle: com L1 + L2, um grau por vez até a resistência. O script anuncia a
transição no terminal (`LIMITES DESATIVADOS (L2)` / `limites ativos`), porque
com `--quiet` o aviso do firmware não aparece.

Ao soltar L2 com a junta fora do limite, o próximo passo naquela junta volta
direto ao limite, em qualquer direção; não há passo a passo de retorno.

Dois pontos que o script **não** protege, nem com L2 solto:

- **O acoplamento altura↔alcance** não é validado. Cada junta é conferida
  isoladamente, e combinações que forcem o braço continuam alcançáveis.
- **`offall` não trava as juntas.** Solto, o braço fica preso apenas pelo
  atrito da redução, que cede sob carga.

## Mapeamento do controle

Confirmado em bancada, DualSense por Bluetooth, SDL 2.32 com pygame-ce.

Eixos: `0` LX, `1` LY, `2` RX, `3` RY, `4` L2, `5` R2

Botões: `0` X, `1` Círculo, `2` Quadrado, `3` Triângulo, `4` Share, `5` PS,
`6` Options, `7` R3, `8` L3, `9` L1, `10` R1, `11`-`14` D-pad
(cima, baixo, esquerda, direita), `15` Touchpad, `16` Mute (não reportado
por Bluetooth).

Os índices mudam entre USB e Bluetooth. Trocando de conexão, rode
`python diag.py --check` e ajuste as constantes no topo de `main.py`.

## Arquivos

```
arm.py             camada serial: protocolo do firmware, leitura da calibração e espera de confirmação
main.py            orquestrador: lê o controle e emite comandos
diag.py            diagnóstico de mapeamento (não fala com o Arduino)
requirements.txt   dependências
```
