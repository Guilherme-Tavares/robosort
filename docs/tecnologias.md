# RoboSort: tecnologias empregadas

Versões conferidas no ambiente em 30/09/2026.

O sistema se divide em quatro camadas de software (interface, serviço de
pedidos, orquestração e firmware), somadas ao hardware físico. A ordem a
seguir acompanha o caminho de um pedido: ele nasce na interface, é persistido
pelo serviço, lido pela visão computacional e executado pelo braço.

---

## 1. Interface web

| Tecnologia | Versão | Função no sistema |
| --- | --- | --- |
| **React** | 19.3 | Biblioteca de construção da interface, em componentes. Abrange o React DOM, responsável pela renderização, e o React Router, que faz a navegação entre as três telas: painel, compra e separação. |
| **TypeScript** | 6.0 | Linguagem da interface, com tipagem estática verificada em tempo de compilação. |
| **Vite** | 8.3 | Ferramenta de build e servidor de desenvolvimento com recarga a quente. |

A interface não usa framework de CSS nem biblioteca de gráficos. O gráfico de
barras por região é implementado em CSS próprio, com rótulo acessível (ARIA).
A escolha reduz dependências num sistema cuja interface tem três telas.

## 2. Serviço de pedidos

| Tecnologia | Versão | Função no sistema |
| --- | --- | --- |
| **Node.js** | 22.21 | Ambiente de execução do serviço. |
| **Express** | 4.22 | Servidor HTTP e roteamento da API REST. |
| **TypeORM** | 0.3.31 | Mapeamento objeto-relacional e versionamento do esquema por *migrations*. |
| **MySQL** | 8.0.44 | Banco de dados relacional, com codificação `utf8mb4`. |
| **TypeScript** | 5.9 | Linguagem do serviço. |

A API expõe o ciclo de vida do pedido: criação da compra, consulta de destino
pelo número do marcador, atualização do estágio da separação, fila e
indicadores agregados por região.

## 3. Orquestração e visão computacional

Camada executada no computador, que concentra as decisões. O Arduino apenas
executa o que recebe.

| Tecnologia | Versão | Função no sistema |
| --- | --- | --- |
| **Python** | 3.14.5 | Linguagem do orquestrador. |
| **OpenCV** | 5.0.0 | Visão computacional: captura de imagem, detecção e decodificação dos marcadores. |
| **ArUco** | dicionário `DICT_4X4_50` | Marcadores fiduciais colados nas caixas, que identificam o pedido. Constituem o elo entre o objeto físico e o registro no banco de dados. A implementação vem do módulo `aruco` do OpenCV. |
| **pySerial** | 3.5 | Comunicação serial com o firmware. |
| **ev3-dc** | 0.9.10 | Controle do bloco LEGO EV3 que aciona a esteira, preservando o firmware original do fabricante. |
| **HIDAPI** e **libusb** | 0.15 e 1.3.1 | Acesso USB ao bloco EV3. O HIDAPI atende o Windows e dispensa a troca de driver; o libusb, acessado pelo pyusb, atende o Linux. |
| **pygame-ce** | 2.5.8 | Leitura do controle DualSense na ferramenta de calibração do braço. |

Duas soluções desta camada dispensam bibliotecas externas. O servidor **MJPEG**
que transmite a câmera para a interface usa apenas o módulo `http.server` da
biblioteca padrão do Python, e o cliente HTTP que consulta a API usa `urllib`,
também da biblioteca padrão.

## 4. Firmware

| Tecnologia | Versão | Função no sistema |
| --- | --- | --- |
| **Arduino UNO R4 WiFi** | core 1.6.0 | Placa de destino, com microcontrolador Renesas RA4M1 de 32 bits. O mesmo código compila para o UNO R3 (core AVR 1.8.8), ocupando 52% da memória de programa. |
| **Adafruit PWM Servo Driver Library** | 3.0.3 | Biblioteca de controle do driver PCA9685. |
| **Arduino CLI** | 1.5.1 | Compilação e gravação por linha de comando. |

O firmware é escrito em C++, no dialeto do Arduino, e se comunica com o
orquestrador por um protocolo textual próprio sobre a porta serial.

## 5. Hardware

| Componente | Especificação | Função no sistema |
| --- | --- | --- |
| **PCA9685** | 16 canais PWM, I²C no endereço `0x40`, 50 Hz | Driver que aciona todos os servomotores, liberando os pinos do microcontrolador. |
| **Servomotores SG90** | 9 unidades | Quatro no braço robótico (base, altura, alcance e garra) e cinco nos empurradores, um por zona de destino. |
| **Sensores infravermelhos FC-51** | 5 unidades | Detecção da caixa em cada zona da esteira. O nível lógico baixo indica obstáculo. |
| **LEGO Mindstorms EV3** | firmware original do fabricante | Bloco e motor que tracionam a esteira transportadora. |
| **Logitech C270** | câmera USB | Captura das imagens para leitura dos marcadores e transmissão à interface. |
| **Controle DualSense** | Sony | Operação manual do braço durante a calibração. |

## 6. Artefatos de apoio ao desenvolvimento

Programas construídos à parte, que deram forma aos artefatos principais. Cada
um isolou um problema antes que ele fosse incorporado ao sistema final.

| Artefato | Natureza | Contribuição |
| --- | --- | --- |
| **Ferramenta de calibração do braço** | Firmware interativo em C++ | Descobre, por tentativa em bancada, os limites de cada junta, as poses de repouso e entrega e os valores-guia da área de aquisição. Todos os números do firmware de produção vêm dela, e é nela que são revisados a cada sessão. |
| **Add-on do DualSense** | Programa em Python | Acopla-se à ferramenta de calibração e traduz o controle em comandos do firmware, permitindo conduzir o braço com as mãos em vez de digitar ângulos. |
| **Testador de módulo** | Firmware em C++ | Isola um único sensor infravermelho e um único empurrador sobre o PCA9685. Foi onde se determinou o tempo de interpolação do empurrão antes de ele entrar no firmware de produção. |
| **Leitor de marcadores** | Módulo em Python | Concentra o acesso à câmera e a detecção ArUco. Permanece em produção, importado pelo módulo de visão do orquestrador, e também roda sozinho para diagnóstico da câmera. |
| **Controlador manual da esteira** | Programa em Python | Validou o acionamento do EV3 por USB, incluindo a solução que faz o `ev3_dc` falar com o bloco no Windows sem troca de driver. É esse caminho que o orquestrador reaproveita. |

## 7. Protocolos e padrões

| Protocolo | Onde atua |
| --- | --- |
| **HTTP/REST com JSON** | Interface e API, e também orquestrador e API. |
| **MJPEG sobre HTTP** | Transmissão da câmera do orquestrador para a interface, em `multipart/x-mixed-replace`. |
| **Serial USB CDC** | Orquestrador e firmware, a 115200 bps, 8N1. |
| **I²C** | Microcontrolador e PCA9685. |
| **USB HID** e **USB bulk** | Computador e bloco EV3, conforme o sistema operacional. |
| **PWM por largura de pulso** | PCA9685 e servomotores, de 544 a 2400 µs para 0° a 180°. |

## 8. Soluções desenvolvidas no projeto

Não são tecnologias de terceiros, e sim decisões de engenharia próprias.

**Contrato serial FIFO.** Cada comando recebe exatamente uma resposta terminal,
na ordem de envio, o que permite parear resposta e comando sem heurística.
Eventos assíncronos, como a detecção e o empurrão, trafegam fora dessa fila.

**Interpolação suavizada não bloqueante.** O movimento dos servos é dividido em
subpassos, sem travar o laço principal do firmware.

**Calibração do oscilador do PCA9685.** O oscilador interno tem valor nominal de
25 MHz, mas varia de exemplar para exemplar. O valor de 27 MHz foi aferido em
bancada e é o que faz os ângulos calibrados corresponderem à posição real.

**Fonte única de parâmetros.** Limites, poses e valores-guia existem apenas no
firmware. O orquestrador os lê no momento da conexão e não guarda cópia.

**Empurrador autônomo.** A zona norte fica no início da esteira, e a caixa chega
antes de o braço retornar. Por isso o empurrão é decidido pelo próprio
firmware, ao detectar a caixa, e não pelo computador.

## 9. Ferramentas de desenvolvimento

| Ferramenta | Versão | Função |
| --- | --- | --- |
| **Git** | 2.51 | Controle de versão. |
| **npm** | 11.12 | Gerenciamento de pacotes de Node.js. |
| **oxlint** | 1.85 | Análise estática da interface. |
| **ts-node** | 10.9 | Execução direta de TypeScript no desenvolvimento da API. |

## Dependências de apoio

Instaladas como suporte às tecnologias principais, sem papel próprio na
arquitetura:

**NumPy** (2.5.2), exigido pelo OpenCV e usado diretamente apenas nas janelas
de diagnóstico, para montar o vetor de identificadores dos marcadores;
**class-validator** e **class-transformer**, na validação dos dados recebidos
pela API; **mysql2**, driver do banco; **dotenv**, configuração por ambiente;
**reflect-metadata**, exigido pelo TypeORM; **cors**; **Adafruit BusIO**,
abstração de barramento; **thread-task** e **gTTS**, exigidos pelo ev3-dc;
**@vitejs/plugin-react**.

---

## Lista consolidada

As tecnologias essenciais e distintas do sistema:

> React, TypeScript, Vite, Node.js, Express, TypeORM, MySQL, Python, OpenCV
> com marcadores ArUco, pySerial, ev3-dc, Arduino UNO R4 WiFi, PCA9685 e
> LEGO Mindstorms EV3.

O projeto está publicado sob licença MIT.
