// ATENÇÃO: este arquivo é um mock TEMPORÁRIO do backend real.
// Ele só é utilizado quando USE_MOCK (VITE_USE_MOCK=true) e será substituído
// pela integração com a API assim que o backend estiver disponível.
// Nenhum componente deve importar este arquivo diretamente — apenas os
// serviços em `src/services/` (purchaseService, queueService, dashboardService).

import { ApiError } from './api';
import { products } from '../mocks/products';
import { states } from '../mocks/locations';
import type { Product } from '../types/product';
import type { PurchaseRequest, PurchaseResponse } from '../types/purchase';
import type { QueueEntry, QueueProduct, QueueSnapshot } from '../types/queue';
import type { DashboardData, RegionName, RegionSummary } from '../types/dashboard';

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

// Histórico de tudo que passou pelo mock. A fila move entradas entre
// `waiting`, `current` e `last`, então sem esta lista o dashboard perderia
// de vista o que já foi concluído.
const allEntries: QueueEntry[] = [];

function buildEntry(product: Product, state: string, city: string): QueueEntry {
  const entry: QueueEntry = {
    id: nextEntryId++,
    volume: nextVolume++,
    product: toQueueProduct(product),
    state,
    city,
    status: 'Aguardando',
  };
  allEntries.push(entry);
  return entry;
}

// A fila começa vazia: nada está em separação antes de alguém comprar.
// Antes havia pedidos pré-carregados aqui, e a tela subia já mostrando
// itens em andamento — além de consumir os marcadores 10 a 13, fazendo a
// primeira compra de verdade receber o 14 em vez do 10 da API.
let current: QueueEntry | null = null;

let last: QueueEntry | null = null;

const waiting: QueueEntry[] = [];

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

// Regiões na mesma ordem do seed da API, todas sempre presentes.
const REGION_ORDER: RegionName[] = ['Norte', 'Nordeste', 'Centro-Oeste', 'Sudeste', 'Sul'];

const REGION_BY_UF = new Map(states.map((state) => [state.uf, state.region]));

// Espelha o DashboardService da API: agrega as compras por região, somando
// `product.volume` (quantos volumes o item ocupa), não o número do marcador.
export async function mockGetDashboard(): Promise<DashboardData> {
  await delay(150);

  const regions: RegionSummary[] = REGION_ORDER.map((name) => {
    const entries = allEntries.filter((entry) => REGION_BY_UF.get(entry.state) === name);

    const volumeByType = new Map<string, number>();
    for (const entry of entries) {
      const current = volumeByType.get(entry.product.type) ?? 0;
      volumeByType.set(entry.product.type, current + entry.product.volume);
    }

    return {
      name,
      totalItems: entries.length,
      totalVolume: entries.reduce((sum, entry) => sum + entry.product.volume, 0),
      products: [...volumeByType.entries()]
        .map(([type, volume]) => ({ type, volume }))
        .sort((a, b) => a.type.localeCompare(b.type)),
    };
  });

  return {
    totalItems: allEntries.length,
    totalVolume: allEntries.reduce((sum, entry) => sum + entry.product.volume, 0),
    regions,
  };
}
