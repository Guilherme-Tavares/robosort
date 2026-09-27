import type { RegionName } from './dashboard';

export type City = string;

export interface StateOption {
  uf: string;
  name: string;
  // Região do estado. A API já devolve a região dentro do dashboard; aqui
  // ela serve ao mock, que precisa agregar por região sem backend.
  region: RegionName;
  cities: City[];
}
