# SPEC 03 — Tela de Fila de separação, queueService (polling) e CameraViewer

## Objetivo
Passa a existir a tela `/fila`: último processado, item atual em destaque e 5 próximos, atualizada
por polling de `GET /api/queue`, com a câmera do sistema físico ao lado.

## Contexto necessário
- TD: `../Frontend.txt`, seções 3, 4, 5, 6, 10 (Fila), 12 e 16.
- Base pronta (SPEC 01): `src/types/queue.ts` (`QueueSnapshot`, `QueueEntry`, `QueueStatus`),
  `src/services/api.ts` (`USE_MOCK`, `httpGet`), `src/services/mockBackend.ts` (`mockGetQueue`),
  variáveis CSS das badges e `.ui-message*` em `src/index.css`, `VITE_CAMERA_URL` em `vite-env.d.ts`.
- Padrão de componente: `src/components/<Nome>/<Nome>.tsx` + `<Nome>.css`.

## Escopo
1. `src/services/queueService.ts`: `getQueue(): Promise<QueueSnapshot>` → `USE_MOCK ? mockGetQueue() : httpGet('/api/queue')`.
   Normalizar defensivamente: `next` ausente → `[]`, e cortar em 5 itens.
2. `src/pages/Queue/useQueuePolling.ts` — hook simples:
   - intervalo de 2500 ms (constante nomeada), busca imediata ao montar;
   - **um único** timer (usar `setTimeout` encadeado após a resposta, ou `setInterval` guardado em
     `useRef`), limpo no unmount; flag `cancelled`/`active` para não fazer `setState` após desmontar;
   - retorna `{ data, loading, error, lastUpdated }`; em erro mantém o último `data` válido e seta
     `error = true` (não zera os dados).
3. `src/components/StatusBadge/`: badge com as 4 cores de status (usa as variáveis CSS).
4. `src/components/QueueItem/`: props `entry: QueueEntry | null`, `variant: 'last' | 'current' | 'next'`,
   `position?: number`.
   - `last` e `current`: imagem, produto, destino (município / UF), tipo, volume, `StatusBadge`.
   - `next`: produto, destino (município / UF), status (imagem pequena opcional).
   - `current` com destaque visual claro (borda na cor de destaque, maior, rótulo "Em separação agora").
   - `entry === null` → placeholder neutro ("Posição vazia" / "Nenhum item processado ainda" /
     "Nenhum item em separação"), sem inventar dados.
5. `src/components/QueueBoard/`: recebe `snapshot: QueueSnapshot`, renderiza 7 posições:
   bloco "Último processado", bloco "Atual", e lista "Próximos" sempre com 5 posições
   (itens reais + placeholders até completar 5).
6. `src/components/CameraViewer/`: prop `url?: string` (a página passa `import.meta.env.VITE_CAMERA_URL`;
   o componente **não** lê env). Regras:
   - vazio → fallback visual "Câmera indisponível";
   - termina em `.m3u8` → `<video>` nativo (funciona em Safari; comentário indicando que para HLS em
     Chrome/Firefox pode-se adicionar hls.js depois — **não** instalar agora);
   - extensões de vídeo (`.mp4`, `.webm`, `.ogg`) → `<video autoPlay muted loop playsInline>`;
   - qualquer outra URL → `<img>` (MJPEG);
   - `onError` de img/vídeo → fallback "Câmera indisponível".
   - Comentário curto: RTSP não roda direto no navegador; precisa de conversão no backend.
   - Título "Câmera do sistema físico".
7. `src/pages/Queue/QueuePage.tsx` (+ `.css`):
   - Título "Fila de separação" e subtítulo explicando que a fila é processada pelo backend/sistema físico.
   - Indicador "Atualizado às HH:MM:SS" e, quando `error` com dados antigos, aviso discreto
     "Sem conexão com o servidor. Exibindo a última informação recebida."
   - Primeira carga: "Carregando fila..."; erro sem nenhum dado: "Não foi possível carregar a fila.";
     fila totalmente vazia (last, current null e next vazio): "Nenhum item na fila." + link para `/compra`.
   - Layout: `QueueBoard` ocupa a largura principal; `CameraViewer` ao lado (desktop ≥ 1100px) ou
     abaixo. Tela deve ser a mais visual das três, sem exagero de cor.

## Fora de escopo
- WebSocket/SignalR, hls.js, controles de câmera, qualquer ação sobre a fila (reordenar, cancelar).
- Compra (SPEC 02) e dashboard (SPEC 04).
- Alterar arquivos da SPEC 01, exceto se estritamente necessário para compilar (relatar se fizer).

## Dependências
SPEC-01

## Critérios de aceite
- [ ] Componentes não chamam `fetch` nem importam `mockBackend`.
- [ ] 7 posições visíveis: 1 último, 1 atual (destacado), 5 próximos (placeholders quando faltar).
- [ ] Polling de 2–3 s com um único timer, limpo no unmount.
- [ ] Erro de rede mantém a última fila válida na tela.
- [ ] `CameraViewer` recebe a URL por prop e mostra "Câmera indisponível" quando vazia ou com erro.
- [ ] `npm run build` passa.

## Como validar
`cd frontend && npm run build`
