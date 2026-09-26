import type { Product } from './product';

export type QueueStatus = 'Aguardando' | 'Separando' | 'Concluído' | 'Erro';

export type QueueProduct = Pick<Product, 'id' | 'name' | 'imageUrl' | 'type' | 'volume'>;

export interface QueueEntry {
  id: number;
  // Numero do marcador ArUco na caixa (o 'volume' da compra).
  volume: number;
  product: QueueProduct;
  state: string;
  city: string;
  status: QueueStatus;
}

export interface QueueSnapshot {
  last: QueueEntry | null;
  current: QueueEntry | null;
  next: QueueEntry[];
}
