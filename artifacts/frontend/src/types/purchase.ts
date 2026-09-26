export interface PurchaseRequest {
  productId: number;
  state: string;
  city: string;
}

export interface PurchaseResponse {
  id: number;
  // Numero do marcador ArUco a colar na caixa: e por ele que a camera
  // identifica o pedido e o orquestrador descobre o destino.
  volume: number;
}
