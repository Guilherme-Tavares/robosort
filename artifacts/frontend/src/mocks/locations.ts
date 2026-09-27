import type { StateOption } from '../types/location';

// Os dez estados que o seed da API cadastra, dois por região: são os únicos
// que `POST /api/purchase` aceita, e os mesmos de SENTIDO_POR_UF no
// orquestrador, que sabe de que lado da esteira fica cada compartimento.
// Os municípios não existem no banco — a compra grava o nome como texto —,
// então esta lista é a fonte deles.
export const states: StateOption[] = [
  {
    uf: 'RO',
    name: 'Rondônia',
    region: 'Norte',
    cities: ['Ji-Paraná', 'Porto Velho', 'Ariquemes'],
  },
  {
    uf: 'AC',
    name: 'Acre',
    region: 'Norte',
    cities: ['Rio Branco', 'Cruzeiro do Sul', 'Sena Madureira'],
  },
  {
    uf: 'BA',
    name: 'Bahia',
    region: 'Nordeste',
    cities: ['Salvador', 'Feira de Santana', 'Vitória da Conquista'],
  },
  {
    uf: 'CE',
    name: 'Ceará',
    region: 'Nordeste',
    cities: ['Fortaleza', 'Juazeiro do Norte', 'Sobral'],
  },
  {
    uf: 'GO',
    name: 'Goiás',
    region: 'Centro-Oeste',
    cities: ['Goiânia', 'Anápolis', 'Rio Verde'],
  },
  {
    uf: 'MT',
    name: 'Mato Grosso',
    region: 'Centro-Oeste',
    cities: ['Cuiabá', 'Várzea Grande', 'Rondonópolis'],
  },
  {
    uf: 'SP',
    name: 'São Paulo',
    region: 'Sudeste',
    cities: ['São Paulo', 'Campinas', 'Santos'],
  },
  {
    uf: 'RJ',
    name: 'Rio de Janeiro',
    region: 'Sudeste',
    cities: ['Rio de Janeiro', 'Niterói', 'Petrópolis'],
  },
  {
    uf: 'PR',
    name: 'Paraná',
    region: 'Sul',
    cities: ['Curitiba', 'Londrina', 'Maringá'],
  },
  {
    uf: 'RS',
    name: 'Rio Grande do Sul',
    region: 'Sul',
    cities: ['Porto Alegre', 'Caxias do Sul', 'Pelotas'],
  },
];
