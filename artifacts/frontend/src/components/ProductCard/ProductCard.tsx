import type { Product } from '../../types/product';
import './ProductCard.css';

interface ProductCardProps {
  product: Product;
  selected?: boolean;
  onSelect: (product: Product) => void;
}

export function ProductCard({ product, selected = false, onSelect }: ProductCardProps) {
  return (
    <article className={selected ? 'product-card product-card--selected' : 'product-card'}>
      <img className="product-card-image" src={product.imageUrl} alt={product.name} />
      <div className="product-card-body">
        <h3 className="product-card-name">{product.name}</h3>
        <p className="product-card-description">{product.description}</p>
        <div className="product-card-meta">
          <span>{product.type}</span>
          <span>
            {product.volume} volume{product.volume === 1 ? '' : 's'}
          </span>
        </div>
        <button
          type="button"
          className="product-card-button"
          onClick={() => onSelect(product)}
        >
          Comprar
        </button>
      </div>
    </article>
  );
}
