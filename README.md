# RoboSort

Esteira automática de separação de produtos com Arduino e visão computacional.
A compra no painel gera um marcador ArUco, a câmera o lê na caixa e o braço
robótico a encaminha ao compartimento da região de destino.

Trabalho da disciplina de Sistemas Inteligentes, IFRO Campus Ji-Paraná.
Fluxo completo validado em bancada nas cinco zonas de separação.

## Estrutura

```
artifacts/
  backend/
    api/            API de pedidos (Node, TypeScript, MySQL)
    orchestrator/   console de operação: visão, braço, esteira
    cam/            acesso à câmera e detecção ArUco
    tests/          suíte de regressão do orquestrador
  frontend/         painel do operador (React, Vite)
  hardware/         firmware do Arduino e ferramenta de calibração
assets/             gabaritos de impressão
config/             regras de acesso aos dispositivos no Linux
docs/               documentação acadêmica
images/             registros de bancada
scripts/            instalação do ambiente
requirements.txt    dependências Python de todos os módulos
```

## Preparar o ambiente

Requer Node.js 20 ou superior, Python 3.13 ou superior, MySQL Server 8 e, para
gravar os sketches, o Arduino CLI.

```bash
# Ambiente Python: orquestrador, visão, EV3 e DualSense
py -m venv artifacts/backend/.venv
artifacts\backend\.venv\Scripts\python -m pip install -r requirements.txt

# No Linux, o instalador faz o equivalente
./scripts/instalar-linux.sh

# Dependências Node
cd artifacts/backend/api && npm install
cd artifacts/frontend && npm install
```

Copie `artifacts/backend/api/.env.example` para `.env.development` e ajuste as
credenciais do MySQL. O banco é criado pelo comando da seção
[Recriar o banco](#recriar-o-banco).

### Permissões no Linux

Arduino e EV3 não devem ser executados com `sudo`. Instale as regras do projeto
e coloque o usuário nos grupos de acesso:

```bash
sudo groupadd -f plugdev
sudo usermod -aG dialout,plugdev "$USER"
sudo install -m 0644 config/udev/99-robosort.rules /etc/udev/rules.d/99-robosort.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Encerre a sessão e entre novamente, então reconecte o Arduino e o EV3. A câmera
costuma usar o grupo `video` e o DualSense, o grupo `input`; em instalação sem
ambiente gráfico, acrescente `sudo usermod -aG video,input "$USER"`.

## Executar

Três terminais, nesta ordem. O orquestrador exige a API no ar, porque consulta
nela o destino de cada caixa.

**1. API e banco**

```bash
cd artifacts/backend/api
npm run dev
```

**2. Orquestrador**

```bash
cd artifacts/backend
.venv/Scripts/python orchestrator/main.py --port COM5          # Windows
.venv/bin/python orchestrator/main.py --port /dev/ttyACM0      # Linux
```

Conecta ao Arduino, energiza o braço em HOME e abre o console. A operação
típica é `start`, que entra no modo automático e executa um ciclo por caixa
vista, cuidando da esteira sozinho; `status` mostra o andamento e `stop`
encerra, devolvendo o braço a HOME.

**3. Painel**

```bash
cd artifacts/frontend
npm run dev
```

Painel em `http://localhost:5173` e API em `http://localhost:3000`. A tela de
separação exibe a câmera pelo stream do orquestrador.

## Recriar o banco

```bash
cd artifacts/backend/api
npm run db:reset
```

Derruba o schema, recria vazio e aplica as migrations com o seed de regiões,
estados e produtos. O primeiro pedido volta a sair com o marcador 10, que é o
estado desejado para começar uma demonstração.

## Testes

Suíte de regressão do orquestrador, sem hardware, API ou banco:

```bash
cd artifacts/backend
.venv/Scripts/python tests/executar_testes.py
```

## Documentação

| Documento | Conteúdo |
| --- | --- |
| [docs/tecnologias.md](docs/tecnologias.md) | Tecnologias empregadas em todas as camadas |
| [orchestrator/README.md](artifacts/backend/orchestrator/README.md) | Console, contrato serial e ciclo de separação |
| [robosort-firmware/README.md](artifacts/hardware/robosort-firmware/README.md) | Comandos do firmware, sequências e tabelas de hardware |
| [roboarm-calibration-tool/README.md](artifacts/hardware/roboarm-calibration-tool/README.md) | Calibração do braço e add-on do DualSense |
| [api/README.md](artifacts/backend/api/README.md) | Rotas e configuração da API |
| [frontend/README.md](artifacts/frontend/README.md) | Variáveis de ambiente e execução isolada |

## Licença

MIT
