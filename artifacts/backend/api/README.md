# Robosort API

API Node.js + TypeScript + Express + TypeORM + MySQL.

## Configuracao

Use `.env.development` localmente ou copie `.env.example`:

```bash
DB_HOST=localhost
DB_PORT=3306
DB_USER=root
DB_PASS=
DB_NAME=robosort
PORT=3000
```

## Rodar

```bash
npm install
npm run migration:run
npm run dev
```

## Rotas

- `GET /api/products`: lista produtos cadastrados, incluindo `price`.
- `GET /api/states`: lista estados cadastrados com regiao.
- `POST /api/purchase`: cria uma compra com o payload de `PurchaseRequest`.
  Responde `{ id, volume }`; o **volume** é o número do marcador ArUco a colar
  na caixa, gerado a partir de 10 e incrementado a cada compra.
- `GET /api/purchase/:volume`: destino da compra (`state`, `region`). É o que o
  orquestrador consulta depois de a câmera ler a caixa.
- `PATCH /api/purchase/:volume/status`: o orquestrador informa o estágio da
  separação — `sorting` ao começar, `done` ao concluir, `error` se o ciclo
  falhou. Só uma compra fica em `sorting`: ao marcar uma, qualquer outra que
  tenha ficado presa nesse estado cai para `error`.
- `GET /api/queue`: fila da tela — `last` (último concluído), `current` (o que
  está em separação) e `next` (até 5 pendentes, por ordem de marcador).
- `GET /api/dashboard`: totais por região, agregados das compras.

Payload de compra:

```json
{
  "productId": 1,
  "state": "AM",
  "city": "Manaus"
}
```
