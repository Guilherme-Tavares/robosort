export type RegionName = 'Norte' | 'Nordeste' | 'Centro-Oeste' | 'Sudeste' | 'Sul';

export interface ProductTypeVolume {
  type: string;
  volume: number;
}

export interface RegionSummary {
  name: RegionName;
  totalVolume: number;
  totalItems: number;
  products: ProductTypeVolume[];
}

export interface DashboardData {
  totalItems: number;
  totalVolume: number;
  regions: RegionSummary[];
}
