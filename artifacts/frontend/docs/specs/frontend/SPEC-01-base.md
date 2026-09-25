# SPEC 01 — Base do projeto (scaffold, tipos, mocks estáticos, serviços base, rotas e navegação)

## Objetivo
Passa a existir um projeto React + TypeScript + Vite executável, com tipos, dados estáticos,
camada HTTP base, mock de backend isolado em `services/`, rotas `/dashboard`, `/compra`, `/fila`
e uma Navbar — com páginas placeholder que as SPECs 02, 03 e 04 vão preencher.

## Contexto necessário
- Diretório do projeto: `frontend/` (já é um repositório git vazio, contém apenas `docs/`).
- A TD completa está em `../Frontend.txt` (leia as seções 1, 4, 8, 11, 14 e 20).
- Stack obrigatória: React, TypeScript, Vite, React Router (v6+ / `react-router-dom`), CSS simples
  por componente (arquivo `.css` ao lado do `.tsx`), `fetch` nativo. **Sem** Redux, sem Axios,
  sem biblioteca de UI, sem biblioteca de gráficos.
- Identificadores em inglês; textos exibidos ao usuário em português (pt-BR com acentos).

## Escopo
1. Scaffold Vite `react-ts` dentro de `frontend/` (sem sobrescrever `docs/`), `npm install`,
   adicionar `react-router-dom`. Remover o conteúdo demo do template (logo, contador, App.css demo).
2. `.env.example` e `.env.development` com:
   ```env
   # URL base do backend. Vazio = mesma origem (use o proxy do Vite).
   VITE_API_BASE_URL=
   # true = usa o mock isolado em src/services/mockBackend.ts em vez do backend real.
   VITE_USE_MOCK=true
   # URL da câmera (MJPEG, HLS .m3u8 ou vídeo HTTP). Vazio = fallback "Câmera indisponível".
   VITE_CAMERA_URL=
   ```
   e `src/vite-env.d.ts` tipando essas três variáveis em `ImportMetaEnv`.
   `vite.config.ts`: proxy de `/api` para `http://localhost:5000` (comentário dizendo que é o backend).
3. Tipos em `src/types/` (exatamente estes contratos, exportados com `export interface`/`export type`):
   - `product.ts`: `Product { id: number; name: string; description: string; imageUrl: string; type: string; volume: number; }`
   - `purchase.ts`: `PurchaseRequest { productId: number; state: string; city: string; }` e
     `PurchaseResponse { id: number; }` (o frontend não usa nada operacional além disso).
   - `queue.ts`: `QueueStatus = 'Aguardando' | 'Separando' | 'Concluído' | 'Erro'`;
     `QueueProduct = Pick<Product, 'id' | 'name' | 'imageUrl' | 'type' | 'volume'>`;
     `QueueEntry { id: number; product: QueueProduct; state: string; city: string; status: QueueStatus; }`;
     `QueueSnapshot { last: QueueEntry | null; current: QueueEntry | null; next: QueueEntry[]; }`.
   - `dashboard.ts`: `RegionName = 'Norte' | 'Nordeste' | 'Centro-Oeste' | 'Sudeste' | 'Sul'`;
     `ProductTypeVolume { type: string; volume: number; }`;
     `RegionSummary { name: RegionName; totalVolume: number; totalItems: number; products: ProductTypeVolume[]; }`;
     `DashboardData { totalItems: number; totalVolume: number; regions: RegionSummary[]; }`.
   - `location.ts`: `City = string`; `StateOption { uf: string; name: string; cities: City[]; }`.
4. Mocks estáticos em `src/mocks/`:
   - `products.ts`: `export const products: Product[]` com 6 produtos cobrindo 3 tipos
     (`Eletrônico`, `Alimentício`, `Vestuário`), volumes inteiros 1–5, `imageUrl` apontando para
     SVGs em `public/images/` (criar 6 SVGs simples, um por produto, só ícone/forma + cor neutra).
   - `locations.ts`: `export const states: StateOption[]` com 1 estado por região (5 no total),
     3 municípios cada: RO (Ji-Paraná, Porto Velho, Ariquemes), CE (Fortaleza, Juazeiro do Norte, Sobral),
     GO (Goiânia, Anápolis, Rio Verde), SP (São Paulo, Campinas, Santos), PR (Curitiba, Londrina, Maringá).
     **Não** incluir o mapeamento estado → região (isso é do backend).
5. `src/services/api.ts`:
   - `export const USE_MOCK = import.meta.env.VITE_USE_MOCK === 'true';`
   - `export class ApiError extends Error` (mensagem amigável; guarda `status?: number`).
   - `export async function httpGet<T>(path: string): Promise<T>` e
     `export async function httpPost<TBody, TResponse>(path: string, body: TBody): Promise<TResponse>`
     usando `fetch(`${import.meta.env.VITE_API_BASE_URL ?? ''}${path}`)`, JSON, e lançando
     `ApiError` em status não-2xx ou falha de rede. Nada de retry, interceptors ou cache.
6. `src/services/mockBackend.ts` — **único** lugar com simulação de backend, cabeçalho em comentário
   deixando explícito que é temporário e só é usado quando `USE_MOCK` for true. Estado em memória
   (módulo), exporta:
   - `mockCreatePurchase(req: PurchaseRequest): Promise<PurchaseResponse>` — procura o produto em
     `mocks/products`, cria um `QueueEntry` com status `'Aguardando'`, id incremental, adiciona ao fim
     da fila; latência simulada ~400 ms; rejeita com `ApiError` se o produto não existir.
   - `mockGetQueue(): Promise<QueueSnapshot>` — simula o processamento do backend por tempo:
     a cada ~6 s o `current` passa a `'Concluído'` e vira `last`, o primeiro `'Aguardando'` vira
     `current` com `'Separando'`. Retorna `next` com no máximo 5 itens. Pode iniciar com 3–4 itens de
     exemplo na fila para a demonstração.
   - `mockGetDashboard(): Promise<DashboardData>` — retorna um **objeto agregado fixo** (fixture de
     exemplo das 5 regiões, com tipos por região e totais coerentes entre si). **Não** calcular
     agregação a partir das compras (regra da TD: agrupamento é do backend).
7. Rotas em `src/routes/index.tsx` com `createBrowserRouter` (ou `<Routes>`): layout com `Navbar` +
   `<Outlet/>`; `/` redireciona para `/dashboard`; `/dashboard`, `/compra`, `/fila`; rota `*`
   redireciona para `/dashboard`. `main.tsx` renderiza o router.
8. `src/components/Navbar/Navbar.tsx` + `Navbar.css`: nome do sistema ("Separação Inteligente") e 3
   `NavLink`s (Dashboard, Compra, Fila de separação) com estado ativo destacado.
9. Páginas placeholder `src/pages/Dashboard/DashboardPage.tsx`, `src/pages/Purchase/PurchasePage.tsx`,
   `src/pages/Queue/QueuePage.tsx` (cada uma só com um `<h1>` do título da tela).
10. `src/index.css`: reset leve, fonte do sistema, variáveis CSS em `:root` (cores neutras + 1 cor de
    destaque + cores das badges: aguardando=cinza, separando=azul, concluído=verde, erro=vermelho),
    container central `max-width: 1200px`, e classes utilitárias de estado de UI reutilizáveis:
    `.ui-message`, `.ui-message--error`, `.ui-message--success` (usar só estas, sem framework).
11. `README.md` mínimo em `frontend/`: como instalar, rodar (`npm run dev`), variáveis de ambiente.
12. `.gitignore` do template (garantir `node_modules`, `dist`, `.env.local`).

## Fora de escopo
- Conteúdo das telas de compra (SPEC 02), fila e câmera (SPEC 03) e dashboard (SPEC 04).
- `purchaseService.ts`, `queueService.ts`, `dashboardService.ts` (criados nas SPECs 02/03/04).
- Testes automatizados, lint extra, Prettier, bibliotecas adicionais.

## Dependências
nenhuma

## Critérios de aceite
- [ ] `npm run build` passa sem erros de TypeScript.
- [ ] Tipos exatamente como descritos no item 3.
- [ ] `locations.ts` tem 5 estados × 3 municípios e não contém nenhuma informação de região.
- [ ] Nenhum componente importa `mockBackend.ts`; ele só existe em `services/`.
- [ ] `mockGetDashboard` não lê as compras nem mapeia estado → região.
- [ ] Navegação entre as 3 rotas funciona e `/` redireciona para `/dashboard`.
- [ ] `.env.example` documenta as 3 variáveis.

## Como validar
`cd frontend && npm run build`
