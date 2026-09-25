import type { QueueStatus } from '../../types/queue';
import './StatusBadge.css';

interface StatusBadgeProps {
  status: QueueStatus;
}

const STATUS_CLASS: Record<QueueStatus, string> = {
  Aguardando: 'status-badge--aguardando',
  Separando: 'status-badge--separando',
  Concluído: 'status-badge--concluido',
  Erro: 'status-badge--erro',
};

export function StatusBadge({ status }: StatusBadgeProps) {
  return <span className={`status-badge ${STATUS_CLASS[status]}`}>{status}</span>;
}
