export interface PurchaseRequest {
  productId: number;
  state: string;
  city: string;
}

export interface PurchaseResponse {
  id: number;
}
