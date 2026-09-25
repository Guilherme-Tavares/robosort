import type { QueueEntry } from '../../types/queue';
import { StatusBadge } from '../StatusBadge/StatusBadge';
import './QueueItem.css';

type QueueItemVariant = 'last' | 'current' | 'next';

interface QueueItemProps {
  entry: QueueEntry | null;
  variant: QueueItemVariant;
  position?: number;
}

const EMPTY_MESSAGE: Record<QueueItemVariant, string> = {
  last: 'Nenhum item processado ainda',
  current: 'Nenhum item em separação',
  next: 'Posição vazia',
};

export function QueueItem({ entry, variant, position }: QueueItemProps) {
  const classNames = [
    'queue-item',
    `queue-item--${variant}`,
    entry ? '' : 'queue-item--empty',
  ]
    .filter(Boolean)
    .join(' ');

  if (!entry) {
    return (
      <div className={classNames}>
        {variant === 'next' && position !== undefined && (
          <span className="queue-item-position">#{position}</span>
        )}
        <p className="queue-item-empty-text">{EMPTY_MESSAGE[variant]}</p>
      </div>
    );
  }

  const destination = `${entry.city} / ${entry.state}`;

  if (variant === 'next') {
    return (
      <div className={classNames}>
        {position !== undefined && <span className="queue-item-position">#{position}</span>}
        <div className="queue-item-info">
          <span className="queue-item-product">{entry.product.name}</span>
          <span className="queue-item-destination">{destination}</span>
        </div>
        <StatusBadge status={entry.status} />
      </div>
    );
  }

  return (
    <div className={classNames}>
      {variant === 'current' && <span className="queue-item-label">Em separação agora</span>}
      <div className="queue-item-main">
        <img
          className="queue-item-image"
          src={entry.product.imageUrl}
          alt={entry.product.name}
        />
        <div className="queue-item-info">
          <span className="queue-item-product">{entry.product.name}</span>
          <span className="queue-item-destination">{destination}</span>
          <span className="queue-item-meta">
            {entry.product.type} · {entry.product.volume} vol.
          </span>
        </div>
      </div>
      <StatusBadge status={entry.status} />
    </div>
  );
}
