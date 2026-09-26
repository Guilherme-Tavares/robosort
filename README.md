# RoboSort

Esteira automática de separação de produtos com Arduino e visão computacional.

O produto é identificado por marcador ArUco e encaminhado ao compartimento
correspondente ao seu destino, separado por região do Brasil e por estado.

Trabalho da disciplina de Sistemas Inteligentes, IFRO Campus Ji-Paraná.

## Estrutura

```
artifacts/
  backend/
    api/         API de pedidos (Node, TypeScript, MySQL)
    orchestrator/ console de operação: visão, braço, esteira
    cam/         diagnóstico de câmera e ArUco
    tests/       suíte de regressão do orquestrador
  frontend/    painel do operador (React, Vite)
  hardware/    firmware do Arduino e ferramenta de calibração
assets/        utilitários gerais
config/        regras de acesso aos dispositivos no Linux
docs/          documentação acadêmica
images/        registros de bancada
scripts/       instalação e utilitários do ambiente
requirements.txt  dependências Python de todos os módulos
```

## Estado

Em desenvolvimento. O ciclo fecha ponta a ponta: a compra no site gera o
número do marcador, a câmera o lê, o orquestrador consulta o destino na API e
informa de volta o estágio da separação. A caixinha é sempre pega na mesma
área de aquisição: a localização por visão foi descartada.

## Rodar no Windows

Caminho mais direto: não há regra de dispositivo para instalar nem driver
para trocar. O Arduino aparece como porta COM e o EV3 é falado por HID, sem
Zadig e sem `libusb`.

Precisa de Python (testado no 3.14) e, para compilar os sketches, do
`arduino-cli` ou da Arduino IDE. Os comandos partem da raiz do repositório.

```
py -m venv artifacts\backend\.venv
artifacts\backend\.venv\Scripts\python -m pip install -r requirements.txt
```

O `requirements.txt` da raiz cobre o orquestrador, a visão, o EV3 e o add-on
do DualSense num só ambiente. No Windows ele traz também o `hidapi`, que é o
que permite conversar com o EV3 sem trocar o driver.

### Executar

Chamando o Python do ambiente direto, sem ativá-lo. Cada linha vale igual no
Prompt de Comando e no PowerShell.

Orquestrador completo:

```
artifacts\backend\.venv\Scripts\python artifacts\backend\orchestrator\main.py --port COM5
```

Sem EV3 e sem câmera, para testar o console e o braço:

```
artifacts\backend\.venv\Scripts\python artifacts\backend\orchestrator\main.py --port COM5 --assume-conveyor --no-camera
```

Diagnóstico da câmera e do DualSense:

```
artifacts\backend\.venv\Scripts\python artifacts\backend\cam\leitor_aruco.py --listar
artifacts\backend\.venv\Scripts\python artifacts\hardware\roboarm-calibration-tool\ds-addon\diag.py --check
```

Troque `COM5` pela porta da sua placa: no Gerenciador de Dispositivos, o Uno
R4 aparece em **Portas (COM e LPT)** como *USB Serial Device*, sem o nome
"Arduino". Sem `--port`, os programas tentam achá-la pelo VID USB.

Para a suíte de testes e a aplicação web, veja [Testes](#testes) e
[Aplicação web](#aplicação-web) — nenhuma das duas depende da bancada.

## Rodar no Linux

Os comandos abaixo partem da raiz do repositório. Em Debian/Ubuntu, instale
primeiro as bibliotecas de sistema usadas pela interface gráfica, pelo USB do
EV3 e pelo ambiente virtual:

```bash
sudo apt update
sudo apt install python3-venv libusb-1.0-0 libgl1 libglib2.0-0 udev
./scripts/instalar-linux.sh
```

O instalador cria `.venv` e instala, em um único ambiente, o orquestrador, a
visão, o controle do EV3 e o add-on do DualSense. Para ativá-lo:

```bash
source .venv/bin/activate
```

### Permissões dos dispositivos

Arduino e EV3 não devem ser executados com `sudo`. Instale as regras do
projeto e coloque o usuário nos grupos de acesso:

```bash
sudo groupadd -f plugdev
sudo usermod -aG dialout,plugdev "$USER"
sudo install -m 0644 config/udev/99-robosort.rules /etc/udev/rules.d/99-robosort.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Depois, encerre a sessão e entre novamente, ou reinicie o computador, e
reconecte o Arduino e o EV3. A câmera normalmente usa o grupo `video` e o
DualSense, o grupo `input`; em uma instalação sem ambiente gráfico, adicione
o usuário a esses grupos se o sistema negar acesso:

```bash
sudo usermod -aG video,input "$USER"
```

### Executar

```bash
# Orquestrador completo
python artifacts/backend/orchestrator/main.py --port /dev/ttyACM0

# Sem EV3 e sem câmera, para testar o console e o braço
python artifacts/backend/orchestrator/main.py \
  --port /dev/ttyACM0 --assume-conveyor --no-camera

# Diagnóstico da câmera
python artifacts/backend/cam/leitor_aruco.py --listar

# Diagnóstico do DualSense
python artifacts/hardware/roboarm-calibration-tool/ds-addon/diag.py --check
```

A porta pode ser `/dev/ttyACM0` (Arduino oficial) ou `/dev/ttyUSB0` (alguns
clones). Sem `--port`, os programas tentam localizar o Arduino pelo VID USB.
Para compilar os sketches, instale também o `arduino-cli` e siga o README de
cada firmware.

## Aplicação web

A API de pedidos e o painel não dependem do Linux nem da bancada montada.
Exigem **MySQL Server** rodando em `localhost:3306` — o MySQL Workbench
sozinho é apenas o cliente gráfico. O banco precisa existir antes da primeira
migration, porque o TypeORM cria as tabelas, não o schema:

```sql
CREATE DATABASE robosort CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

Depois, com `artifacts/backend/api/.env.development` configurado a partir de
`.env.example`:

```bash
cd artifacts/backend/api && npm install && npm run migration:run && npm run dev
cd artifacts/frontend  && npm install && npm run dev
```

A API sobe em `http://localhost:3000` e o painel em `http://localhost:5173`.
Detalhes de rotas e variáveis nos READMEs de
[api](artifacts/backend/api/README.md) e
[frontend](artifacts/frontend/README.md).

## Testes

A suíte de regressão do orquestrador não toca em hardware, API ou banco:

```bash
cd artifacts/backend
.venv/Scripts/python tests/executar_testes.py   # Windows
.venv/bin/python tests/executar_testes.py       # Linux
```

O que cada teste cobre está em [artifacts/backend/tests/](artifacts/backend/tests/README.md).

## Licença

MIT
