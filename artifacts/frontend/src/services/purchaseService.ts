import { USE_MOCK, httpGet, httpPost } from './api';
import { mockCreatePurchase } from './mockBackend';
import { products } from '../mocks/products';
import type { Product } from '../types/product';
import type { PurchaseRequest, PurchaseResponse } from '../types/purchase';

export async function createPurchase(req: PurchaseRequest): Promise<PurchaseResponse> {
  if (USE_MOCK) {
    return mockCreatePurchase(req);
  }
  return httpPost<PurchaseRequest, PurchaseResponse>('/api/purchase', req);
}

export async function getProducts(): Promise<Product[]> {
  if (USE_MOCK) {
    return products;
  }
  return httpGet<Product[]>('/api/products');
}
