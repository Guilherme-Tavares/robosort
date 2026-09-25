# SPEC 04 — Dashboard e dashboardService

## Objetivo
Passa a existir a tela `/dashboard` que consome `GET /api/dashboard` (dados já agregados pelo
backend) e exibe totais, gráfico de barras de volume por região e tabela região × tipo × volume.

## Contexto necessário
- TD: `../Frontend.txt`, seções 7, 8, 9, 10 (Dashboard), 12 e 17.
- Base pronta (SPEC 01): `src/types/dashboard.ts` (`DashboardData`, `RegionSummary`, `RegionName`),
  `src/services/api.ts` (`USE_MOCK`, `httpGet`), `src/services/mockBackend.ts` (`mockGetDashboard`),
  variáveis CSS e `.ui-message*` em `src/index.css`.
- Padrão de componente: `src/components/<Nome>/<Nome>.tsx` + `<Nome>.css`.
- **Regra central:** o frontend NÃO agrega nem mapeia estado → região. Só exibe o que recebe.

## Escopo
1. `src/services/dashboardService.ts`: `getDashboard(): Promise<DashboardData>` →
   `USE_MOCK ? mockGetDashboard() : httpGet('/api/dashboard')`.
2. `src/components/StatCard/`: cartão com rótulo e valor numérico grande.
3. `src/components/RegionChart/`: gráfico de barras horizontal feito com HTML/CSS (sem biblioteca):
   uma barra por região recebida, largura proporcional a `totalVolume / max(totalVolume)`, rótulo da
   região e valor "N volumes". Se todos os valores forem 0, barras com largura 0 (sem dividir por zero).
   Acessível: container com `role="img"` + `aria-label` resumindo os valores, ou lista semântica.
4. `src/components/RegionTable/`: tabela com colunas Região | Tipo | Volume (volume alinhado à direita),
   uma linha por `(região, tipo)` na ordem recebida; regiões sem produtos não geram linha.
5. `src/pages/Dashboard/DashboardPage.tsx` (+ `.css`):
   - Busca ao montar via `getDashboard()`; botão "Atualizar" para recarregar (sem polling).
   - Topo: 3 `StatCard`: "Itens processados" (`totalItems`), "Volumes totais" (`totalVolume`),
     "Regiões atendidas" (quantidade de regiões com `totalVolume > 0` — contagem de exibição, não agregação).
   - Meio: `RegionChart` com título "Volumes por região".
   - Baixo: `RegionTable` com título "Tipos de produto por região".
   - Estados: "Carregando indicadores..." / "Não foi possível carregar os indicadores." (com botão
     "Tentar novamente") / "Nenhum dado disponível." (quando `regions` vazio ou `totalItems === 0`).
   - Uma frase curta explicando que os indicadores vêm das compras registradas no backend.

## Fora de escopo
- Qualquer cálculo de agregação no frontend, filtros por período, exportação, biblioteca de gráficos.
- Compra (SPEC 02) e fila (SPEC 03). Alterar arquivos da SPEC 01, exceto se estritamente
  necessário para compilar (relatar se fizer).

## Dependências
SPEC-01

## Critérios de aceite
- [ ] Nenhum `reduce`/agrupamento de compras no frontend; valores exibidos vêm do `DashboardData`.
- [ ] Gráfico de barras e tabela região × tipo × volume renderizados a partir da resposta.
- [ ] Estados carregando / erro / vazio com os textos da TD.
- [ ] Componentes não chamam `fetch` nem importam `mockBackend`.
- [ ] `npm run build` passa.

## Como validar
`cd frontend && npm run build`
