// Camada de serviço da fila de separação. Isola o consumo do mock/API real,
// para que os componentes nunca precisem saber a origem dos dados.

import { USE_MOCK, httpGet } from './api';
import { mockGetQueue } from './mockBackend';
import type { QueueEntry, QueueSnapshot } from '../types/queue';

const MAX_NEXT_ITEMS = 5;

function normalizeSnapshot(raw: QueueSnapshot): QueueSnapshot {
  const next: QueueEntry[] = Array.isArray(raw.next) ? raw.next : [];

  return {
    last: raw.last ? { ...raw.last } : null,
    current: raw.current ? { ...raw.current } : null,
    next: next.slice(0, MAX_NEXT_ITEMS).map((entry) => ({ ...entry })),
  };
}

export async function getQueue(): Promise<QueueSnapshot> {
  const raw = USE_MOCK ? await mockGetQueue() : await httpGet<QueueSnapshot>('/api/queue');
  return normalizeSnapshot(raw);
}
