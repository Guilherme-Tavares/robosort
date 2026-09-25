import { USE_MOCK, httpPost } from './api';
import { mockCreatePurchase } from './mockBackend';
import { products } from '../mocks/products';
import type { Product } from '../types/product';
import type { PurchaseRequest, PurchaseResponse } from '../types/purchase';

export async function createPurchase(req: PurchaseRequest): Promise<PurchaseResponse> {
  if (USE_MOCK) {
    return mockCreatePurchase(req);
  }
  return httpPost<PurchaseRequest, PurchaseResponse>('/api/purchases', req);
}

export async function getProducts(): Promise<Product[]> {
  // Ponto de integração: trocar por httpGet<Product[]>('/api/products') quando existir.
  return products;
}
