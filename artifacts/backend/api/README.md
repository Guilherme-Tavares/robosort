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
- `POST /api/purchases`: alias compativel com o frontend atual.
- `GET /api/purchase/:volume`: retorna estado e regiao usando o volume da compra, nao o id.

Payload de compra:

```json
{
  "productId": 1,
  "state": "AM",
  "city": "Manaus"
}
```
