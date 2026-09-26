# Separação Inteligente — Frontend

Frontend do projeto acadêmico de Sistemas Inteligentes: interface para realizar uma compra,
acompanhar a fila de separação física do produto e visualizar indicadores agregados.

## Stack

React, TypeScript, Vite, React Router.

## Como instalar

```bash
npm install
```

## Como rodar

```bash
npm run dev
```

A aplicação sobe em `http://localhost:5173`.

## Variáveis de ambiente

Copie `.env.example` para `.env.local` (ou ajuste `.env.development`) e configure:

| Variável              | Descrição                                                                 |
| ---------------------- | -------------------------------------------------------------------------- |
| `VITE_API_BASE_URL`    | URL base do backend. Vazio = mesma origem (usa o proxy do Vite para `/api`). |
| `VITE_USE_MOCK`        | `true` para usar o mock isolado em `src/services/mockBackend.ts` em vez do backend real. |
| `VITE_CAMERA_URL`      | URL da câmera (MJPEG, HLS `.m3u8` ou vídeo HTTP). Vazio = fallback "Câmera indisponível". Aponte para o stream MJPEG do orchestrator, ex.: `http://<ip-do-orchestrator>:8090/stream.mjpg`. |

## Integração

Com `VITE_USE_MOCK=false` (o padrão de `.env.development`), o front consome a
API em `artifacts/backend/api`, que sobe na porta 3000; o proxy do Vite
encaminha `/api` para lá. Suba os três na ordem:

```bash
cd artifacts/backend/api && npm run migration:run && npm run dev   # API + MySQL
cd artifacts/backend && .venv/Scripts/python orchestrator/main.py  # orquestrador (câmera e braço)
cd artifacts/frontend && npm run dev                               # este front
```

| Tela | De onde vêm os dados |
| --- | --- |
| Compra | `GET /api/products`, `POST /api/purchase` — a resposta traz o **volume**, o número do marcador ArUco a colar na caixa |
| Fila | `GET /api/queue` — `current` é a compra que o orquestrador marcou como `sorting` |
| Painel | `GET /api/dashboard` — agregação das compras por região |
| Câmera | stream MJPEG do orquestrador (`VITE_CAMERA_URL`), fora do proxy |

O ciclo fecha assim: a compra gera o marcador; a câmera lê o marcador; o
orquestrador pergunta o destino à API e informa de volta o estágio da
separação, que é o que a tela de fila mostra.

## Build

```bash
npm run build
```
