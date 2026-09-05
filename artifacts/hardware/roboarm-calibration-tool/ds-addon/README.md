# ds-addon

Operação do braço de calibração por controle DualSense.

Orquestrador provisório: aciona o firmware de
[`calibration-tool/calibration-tool.ino`](../calibration-tool/calibration-tool.ino) pelo controle,
durante a fase de calibração.

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
| Quadrado | alterna a garra entre 88 e 96 |
| Triângulo | garra volta a 92, apenas se estiver em 96 |

Sem L1, o passo é de 2 graus e a cadência acompanha a confirmação do firmware
(~64 ms por passo, ~30 graus/s). O clamp para exatamente no limite: se faltar
1 grau, o passo vira 1.

A velocidade vem de `STEP_DELAY` no sketch (8 ms por subpasso, 4 subpassos por
grau). Mexer nela exige atualizar `SECONDS_PER_DEGREE` em `main.py`, que
espelha o mesmo cálculo.

## Antes de apertar Options

**O braço precisa estar fisicamente nas posições iniciais** (base 98,
altura 91, alcance 116, garra 92). Energizar um servo o puxa à força até o
ângulo declarado, sem interpolação. Braço longe do centro significa solavanco
em cada junta.

## Limites

| Junta | Mín | Máx |
|---|---|---|
| Base | 18 | 178 |
| Altura | 16 | 136 |
| Alcance | 56 | 176 |
| Garra | 88, 92, 96 | |

São os valores medidos na calibração, sem margem adicional.

Dois pontos que o script **não** protege:

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
arm.py             camada serial: protocolo do firmware e espera de confirmação
main.py            orquestrador: lê o controle e emite comandos
diag.py            diagnóstico de mapeamento (não fala com o Arduino)
requirements.txt   dependências
```
