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

## Build

```bash
npm run build
```
