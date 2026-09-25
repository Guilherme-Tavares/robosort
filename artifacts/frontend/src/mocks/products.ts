import type { Product } from '../types/product';

export const products: Product[] = [
  {
    id: 1,
    name: 'Fone de Ouvido Bluetooth',
    description: 'Fone sem fio com cancelamento de ruído.',
    imageUrl: '/images/product-1.svg',
    type: 'Eletrônico',
    volume: 1,
  },
  {
    id: 2,
    name: 'Smartwatch',
    description: 'Relógio inteligente com monitor de atividades.',
    imageUrl: '/images/product-2.svg',
    type: 'Eletrônico',
    volume: 2,
  },
  {
    id: 3,
    name: 'Pacote de Café',
    description: 'Café torrado e moído, pacote de 500g.',
    imageUrl: '/images/product-3.svg',
    type: 'Alimentício',
    volume: 3,
  },
  {
    id: 4,
    name: 'Caixa de Cereal',
    description: 'Cereal matinal integral, caixa de 300g.',
    imageUrl: '/images/product-4.svg',
    type: 'Alimentício',
    volume: 2,
  },
  {
    id: 5,
    name: 'Camiseta Básica',
    description: 'Camiseta de algodão, cores variadas.',
    imageUrl: '/images/product-5.svg',
    type: 'Vestuário',
    volume: 1,
  },
  {
    id: 6,
    name: 'Jaqueta Corta-Vento',
    description: 'Jaqueta leve e impermeável.',
    imageUrl: '/images/product-6.svg',
    type: 'Vestuário',
    volume: 5,
  },
];
