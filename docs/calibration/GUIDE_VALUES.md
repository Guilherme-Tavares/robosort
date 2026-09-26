# Valores-guia da área de aquisição

Aferidos na `roboarm-calibration-tool`, que é a referência: é nela que os
valores são medidos e refinados em bancada. Depois de cada sessão, copie para
`robosort-firmware/config.h`, que é de onde o orquestrador os lê.

## Área de aquisição

Uma única área, sempre a mesma. A caixinha é posta nela pelo operador ou
chega pela esteira; a pose para pegá-la não depende de onde ela está dentro
da área.

| Junta | Ângulo | Constante |
| --- | --- | --- |
| Base | 18 | `AREA_BASE` |
| Altura | 22 | `AREA_HEIGHT` |
| Alcance | 62 | `AREA_REACH` |

## Aproximação

Altura e alcance aplicados antes de descer à área. A ordem — altura antes de
alcance na ida, o inverso na volta — é a proteção contra o acoplamento do
pantógrafo.

| Junta | Ângulo | Constante |
| --- | --- | --- |
| Altura | 39 | `APPROACH_HEIGHT` |
| Alcance | 78 | `APPROACH_REACH` |

## Sobre a matriz 2x2

Estes valores já foram uma tabela de quatro cantos (`[0]` sup-esq, `[1]`
sup-dir, `[2]` inf-esq, `[3]` inf-dir) sobre um quadrado de 3 x 3 cm. Os
quatro existiam como âncoras para interpolar a pose de qualquer ponto da
área, quando a visão localizasse a caixinha em cm.

Essa localização foi descartada, e com ela os cantos 1, 2 e 3 — que nunca
foram revalidados depois da remontagem do braço. O canto 0 é a área única de
hoje, e seus valores são os da tabela acima. Os antigos 44 / 29 / 56, que
este arquivo registrou por um tempo, são anteriores à remontagem e não valem
mais.
