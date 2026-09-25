import type { Product } from '../../types/product';
import { ProductCard } from '../ProductCard/ProductCard';
import './ProductGrid.css';

interface ProductGridProps {
  products: Product[];
  selectedId: number | null;
  onSelect: (product: Product) => void;
}

export function ProductGrid({ products, selectedId, onSelect }: ProductGridProps) {
  return (
    <div className="product-grid">
      {products.map((product) => (
        <ProductCard
          key={product.id}
          product={product}
          selected={product.id === selectedId}
          onSelect={onSelect}
        />
      ))}
    </div>
  );
}
