import type { Product } from '../../types/product';
import type { StateOption } from '../../types/location';
import './PurchaseSummary.css';

interface PurchaseSummaryProps {
  product: Product | null;
  states: StateOption[];
  uf: string;
  city: string;
  submitting: boolean;
  onUfChange: (uf: string) => void;
  onCityChange: (city: string) => void;
  onConfirm: () => void;
  onCancel: () => void;
}

export function PurchaseSummary({
  product,
  states,
  uf,
  city,
  submitting,
  onUfChange,
  onCityChange,
  onConfirm,
  onCancel,
}: PurchaseSummaryProps) {
  const selectedState = states.find((state) => state.uf === uf) ?? null;
  const canConfirm = Boolean(product) && Boolean(uf) && Boolean(city) && !submitting;

  return (
    <div className="purchase-summary">
      <h2 className="purchase-summary-title">Endereço de entrega</h2>

      <label className="purchase-summary-field">
        <span>Estado</span>
        <select
          value={uf}
          onChange={(event) => onUfChange(event.target.value)}
          disabled={!product}
        >
          <option value="">Selecione o estado</option>
          {states.map((state) => (
            <option key={state.uf} value={state.uf}>
              {state.name}
            </option>
          ))}
        </select>
      </label>

      <label className="purchase-summary-field">
        <span>Município</span>
        <select
          value={city}
          onChange={(event) => onCityChange(event.target.value)}
          disabled={!selectedState}
        >
          <option value="">Selecione o município</option>
          {selectedState?.cities.map((cityOption) => (
            <option key={cityOption} value={cityOption}>
              {cityOption}
            </option>
          ))}
        </select>
      </label>

      <div className="purchase-summary-resume">
        <h3>Resumo</h3>
        <dl>
          <dt>Produto</dt>
          <dd>{product?.name ?? '—'}</dd>
          <dt>Tipo</dt>
          <dd>{product?.type ?? '—'}</dd>
          <dt>Volume</dt>
          <dd>{product ? `${product.volume} volume${product.volume === 1 ? '' : 's'}` : '—'}</dd>
          <dt>Estado</dt>
          <dd>{selectedState?.name ?? '—'}</dd>
          <dt>Município</dt>
          <dd>{city || '—'}</dd>
        </dl>
      </div>

      <div className="purchase-summary-actions">
        <button type="button" className="purchase-summary-cancel" onClick={onCancel} disabled={submitting}>
          Cancelar
        </button>
        <button type="button" className="purchase-summary-confirm" onClick={onConfirm} disabled={!canConfirm}>
          {submitting ? 'Enviando...' : 'Confirmar compra'}
        </button>
      </div>
    </div>
  );
}
