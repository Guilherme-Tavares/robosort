# RoboSort

Esteira automática de separação de produtos com Arduino e visão computacional.

O produto é identificado por marcador ArUco e encaminhado ao compartimento
correspondente ao seu destino, separado por região do Brasil e por estado.

Trabalho da disciplina de Sistemas Inteligentes, IFRO Campus Ji-Paraná.

## Estrutura

```
artifacts/
  backend/     aplicação servidora (visão, banco, orquestrador)
  frontend/    painel do operador
  hardware/    firmware do Arduino
assets/        utilitários gerais
config/        regras de acesso aos dispositivos no Linux
docs/          documentação acadêmica
images/        registros de bancada
scripts/       instalação e utilitários do ambiente
requirements.txt  dependências Python de todos os módulos
```

## Estado

Em desenvolvimento. Calibração do braço robótico em andamento.

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

## Licença

MIT
