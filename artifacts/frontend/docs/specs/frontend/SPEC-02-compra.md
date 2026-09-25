# SPEC 02 — Tela de Compra e purchaseService

## Objetivo
Passa a existir a tela `/compra`: grade de produtos, escolha de estado/município dependentes,
resumo e confirmação que envia `POST /api/purchases` via `purchaseService`.

## Contexto necessário
- TD: `../Frontend.txt`, seções 2, 10 (Compra), 12, 13 e 16.
- Base pronta (SPEC 01): `src/types/*`, `src/mocks/products.ts`, `src/mocks/locations.ts`,
  `src/services/api.ts` (`USE_MOCK`, `httpPost`, `ApiError`), `src/services/mockBackend.ts`
  (`mockCreatePurchase`), classes `.ui-message*` e variáveis CSS em `src/index.css`.
- Padrão de componente: pasta `src/components/<Nome>/<Nome>.tsx` + `<Nome>.css`.
- Identificadores em inglês; textos ao usuário em pt-BR.

## Escopo
1. `src/services/purchaseService.ts`:
   - `createPurchase(req: PurchaseRequest): Promise<PurchaseResponse>` → se `USE_MOCK`,
     `mockCreatePurchase(req)`; senão `httpPost('/api/purchases', req)`.
   - `getProducts(): Promise<Product[]>` → hoje retorna `products` do mock estático; comentário
     `// Ponto de integração: trocar por httpGet<Product[]>('/api/products') quando existir.`
2. `src/components/ProductCard/`: imagem, nome, descrição, tipo, volume ("N volume(s)"), botão "Comprar".
   Prop `selected` destaca o card escolhido.
3. `src/components/ProductGrid/`: grade responsiva (`grid-template-columns: repeat(auto-fill, minmax(220px, 1fr))`)
   de `ProductCard`, recebe `products`, `selectedId`, `onSelect(product)`.
4. `src/components/PurchaseSummary/`: painel de etapa de endereço + resumo:
   - dropdown Estado (lista de `states`), dropdown Município (desabilitado sem estado; opções
     apenas de `state.cities`). Trocar estado limpa município.
   - Resumo: Produto, Tipo, Volume, Estado, Município.
   - Botão "Confirmar compra" desabilitado se faltar produto, estado ou município, ou durante envio
     (texto "Enviando..." durante envio). Botão "Cancelar" limpa a seleção.
   - Componente controlado: recebe `product`, `uf`, `city`, callbacks, `submitting`; não faz HTTP.
5. `src/pages/Purchase/PurchasePage.tsx` (+ `.css`): orquestra o fluxo.
   - Carrega produtos via `getProducts()` com estados carregando / erro / vazio
     ("Carregando produtos...", "Não foi possível carregar os produtos.", "Nenhum produto disponível.").
   - Clicar em "Comprar" seleciona o produto (um por vez; escolher outro substitui) e mostra o
     `PurchaseSummary`.
   - Confirmar: chama `createPurchase({ productId, state: uf, city })`.
     Sucesso → mensagem `.ui-message--success`: "Compra registrada! O pedido entrou na fila de separação."
     com `Link` "Acompanhar na fila" para `/fila`; limpa produto/estado/município.
     Erro → `.ui-message--error`: "Não foi possível registrar a compra. Tente novamente." (sem erro bruto),
     mantendo a seleção para nova tentativa.
   - Não exibir status de separação nem ID de fila (a resposta HTTP só significa "aceita pelo backend").
   - Layout: grade à esquerda/cima, painel de resumo à direita (desktop) e abaixo (< 900px).

## Fora de escopo
- Carrinho, quantidade, pagamento, saldo, login (proibidos pela TD).
- Fila e dashboard (SPECs 03 e 04). Alterar arquivos da SPEC 01, exceto se estritamente
  necessário para compilar (relatar se fizer).

## Dependências
SPEC-01

## Critérios de aceite
- [ ] Componentes não chamam `fetch` nem importam `mockBackend`; só a página chama o service.
- [ ] Município é dropdown que depende do estado; trocar estado limpa município.
- [ ] Confirmar fica desabilitado sem produto, estado ou município.
- [ ] Payload enviado: `{ productId, state, city }` — nada mais.
- [ ] Sucesso limpa o fluxo e permite nova compra; erro mostra mensagem amigável sem quebrar a tela.
- [ ] `npm run build` passa.

## Como validar
`cd frontend && npm run build`
