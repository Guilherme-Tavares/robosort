import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { ProductGrid } from '../../components/ProductGrid/ProductGrid';
import { PurchaseSummary } from '../../components/PurchaseSummary/PurchaseSummary';
import { createPurchase, getProducts } from '../../services/purchaseService';
import { states } from '../../mocks/locations';
import type { Product } from '../../types/product';
import './PurchasePage.css';

type LoadState = 'loading' | 'success' | 'error';

export function PurchasePage() {
  const [products, setProducts] = useState<Product[]>([]);
  const [loadState, setLoadState] = useState<LoadState>('loading');

  const [selectedProduct, setSelectedProduct] = useState<Product | null>(null);
  const [uf, setUf] = useState('');
  const [city, setCity] = useState('');

  const [submitting, setSubmitting] = useState(false);
  const [feedback, setFeedback] = useState<'success' | 'error' | null>(null);

  useEffect(() => {
    let active = true;

    getProducts()
      .then((data) => {
        if (!active) return;
        setProducts(data);
        setLoadState('success');
      })
      .catch(() => {
        if (!active) return;
        setLoadState('error');
      });

    return () => {
      active = false;
    };
  }, []);

  function handleSelectProduct(product: Product) {
    setSelectedProduct(product);
    setFeedback(null);
  }

  function handleUfChange(nextUf: string) {
    setUf(nextUf);
    setCity('');
  }

  function clearSelection() {
    setSelectedProduct(null);
    setUf('');
    setCity('');
  }

  async function handleConfirm() {
    if (!selectedProduct || !uf || !city) return;

    setSubmitting(true);
    setFeedback(null);

    try {
      await createPurchase({ productId: selectedProduct.id, state: uf, city });
      setFeedback('success');
      clearSelection();
    } catch {
      setFeedback('error');
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <div className="purchase-page">
      <h1>Compra</h1>

      {feedback === 'success' && (
        <p className="ui-message ui-message--success">
          Compra registrada! O pedido entrou na fila de separação.{' '}
          <Link to="/fila">Acompanhar na fila</Link>
        </p>
      )}
      {feedback === 'error' && (
        <p className="ui-message ui-message--error">
          Não foi possível registrar a compra. Tente novamente.
        </p>
      )}

      {loadState === 'loading' && <p className="ui-message">Carregando produtos...</p>}
      {loadState === 'error' && (
        <p className="ui-message ui-message--error">Não foi possível carregar os produtos.</p>
      )}
      {loadState === 'success' && products.length === 0 && (
        <p className="ui-message">Nenhum produto disponível.</p>
      )}

      {loadState === 'success' && products.length > 0 && (
        <div className="purchase-page-layout">
          <ProductGrid
            products={products}
            selectedId={selectedProduct?.id ?? null}
            onSelect={handleSelectProduct}
          />

          {selectedProduct && (
            <PurchaseSummary
              product={selectedProduct}
              states={states}
              uf={uf}
              city={city}
              submitting={submitting}
              onUfChange={handleUfChange}
              onCityChange={setCity}
              onConfirm={handleConfirm}
              onCancel={clearSelection}
            />
          )}
        </div>
      )}
    </div>
  );
}
