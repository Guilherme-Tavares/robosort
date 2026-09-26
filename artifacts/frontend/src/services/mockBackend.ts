// ATENÇÃO: este arquivo é um mock TEMPORÁRIO do backend real.
// Ele só é utilizado quando USE_MOCK (VITE_USE_MOCK=true) e será substituído
// pela integração com a API assim que o backend estiver disponível.
// Nenhum componente deve importar este arquivo diretamente — apenas os
// serviços em `src/services/` (purchaseService, queueService, dashboardService).

import { ApiError } from './api';
import { products } from '../mocks/products';
import type { Product } from '../types/product';
import type { PurchaseRequest, PurchaseResponse } from '../types/purchase';
import type { QueueEntry, QueueProduct, QueueSnapshot } from '../types/queue';
import type { DashboardData } from '../types/dashboard';

function delay(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function toQueueProduct(product: Product): QueueProduct {
  const { id, name, imageUrl, type, volume } = product;
  return { id, name, imageUrl, type, volume };
}

let nextEntryId = 100;
// Espelha a API: o numero do marcador comeca em 10 e sobe a cada compra.
let nextVolume = 10;

function buildEntry(product: Product, state: string, city: string): QueueEntry {
  return {
    id: nextEntryId++,
    volume: nextVolume++,
    product: toQueueProduct(product),
    state,
    city,
    status: 'Aguardando',
  };
}

let current: QueueEntry | null = buildEntry(products[0], 'RO', 'Porto Velho');
current.status = 'Separando';

let last: QueueEntry | null = null;

const waiting: QueueEntry[] = [
  buildEntry(products[1], 'CE', 'Fortaleza'),
  buildEntry(products[2], 'GO', 'Goiânia'),
  buildEntry(products[3], 'SP', 'Campinas'),
];

// Simula o avanço do processamento do backend: a cada ~6s o item atual é
// concluído e o próximo da fila passa a ser processado.
let tickStarted = false;

function startTick(): void {
  if (tickStarted) return;
  tickStarted = true;

  setInterval(() => {
    if (current) {
      current.status = 'Concluído';
      last = current;
      current = null;
    }

    if (!current && waiting.length > 0) {
      const promoted = waiting.shift();
      if (promoted) {
        promoted.status = 'Separando';
        current = promoted;
      }
    }
  }, 6000);
}

export async function mockCreatePurchase(req: PurchaseRequest): Promise<PurchaseResponse> {
  await delay(400);

  const product = products.find((item) => item.id === req.productId);
  if (!product) {
    throw new ApiError('Produto não encontrado.', 404);
  }

  const entry = buildEntry(product, req.state, req.city);
  waiting.push(entry);

  return { id: entry.id, volume: entry.volume };
}

export async function mockGetQueue(): Promise<QueueSnapshot> {
  // O timer só começa na primeira consulta, para não rodar quando USE_MOCK for false.
  startTick();
  await delay(150);

  return {
    last,
    current,
    next: waiting.slice(0, 5),
  };
}

const dashboardFixture: DashboardData = {
  totalItems: 90,
  totalVolume: 90,
  regions: [
    {
      name: 'Norte',
      totalVolume: 25,
      totalItems: 25,
      products: [
        { type: 'Eletrônico', volume: 10 },
        { type: 'Alimentício', volume: 8 },
        { type: 'Vestuário', volume: 7 },
      ],
    },
    {
      name: 'Nordeste',
      totalVolume: 14,
      totalItems: 14,
      products: [
        { type: 'Eletrônico', volume: 4 },
        { type: 'Alimentício', volume: 6 },
        { type: 'Vestuário', volume: 4 },
      ],
    },
    {
      name: 'Centro-Oeste',
      totalVolume: 8,
      totalItems: 8,
      products: [
        { type: 'Eletrônico', volume: 2 },
        { type: 'Alimentício', volume: 3 },
        { type: 'Vestuário', volume: 3 },
      ],
    },
    {
      name: 'Sudeste',
      totalVolume: 32,
      totalItems: 32,
      products: [
        { type: 'Eletrônico', volume: 14 },
        { type: 'Alimentício', volume: 10 },
        { type: 'Vestuário', volume: 8 },
      ],
    },
    {
      name: 'Sul',
      totalVolume: 11,
      totalItems: 11,
      products: [
        { type: 'Eletrônico', volume: 5 },
        { type: 'Alimentício', volume: 4 },
        { type: 'Vestuário', volume: 2 },
      ],
    },
  ],
};

export async function mockGetDashboard(): Promise<DashboardData> {
  await delay(150);
  return dashboardFixture;
}
