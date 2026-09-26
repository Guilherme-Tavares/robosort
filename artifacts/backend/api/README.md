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

O TypeORM cria as **tabelas**, nao o **banco**: `migration:run` falha com
`Unknown database 'robosort'` se o schema ainda nao existir. Crie-o uma vez,
pelo MySQL Workbench ou pela linha de comando:

```sql
CREATE DATABASE robosort CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
```

Depois, a cada clone:

```bash
npm install
npm run migration:run
npm run dev
```

O MySQL Workbench sozinho nao basta: ele e o cliente grafico. E preciso o
**MySQL Server** rodando em `localhost:3306` (o instalador oficial para
Windows traz os dois).

## Rotas

- `GET /api/products`: lista produtos cadastrados, incluindo `price`.
- `GET /api/states`: lista estados cadastrados com regiao. O seed traz dois
  estados por regiao, e so eles: RO, AC (Norte), BA, CE (Nordeste), GO, MT
  (Centro-Oeste), SP, RJ (Sudeste), PR, RS (Sul). Um `state` fora dessa lista
  e recusado com 400, e os mesmos dez aparecem em `SENTIDO_POR_UF` no
  orquestrador, que sabe de que lado da esteira fica cada compartimento.
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
  "state": "SP",
  "city": "Campinas"
}
```
