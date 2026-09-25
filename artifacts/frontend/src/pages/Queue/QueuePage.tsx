import { Link } from 'react-router-dom';
import { QueueBoard } from '../../components/QueueBoard/QueueBoard';
import { CameraViewer } from '../../components/CameraViewer/CameraViewer';
import { useQueuePolling } from './useQueuePolling';
import './QueuePage.css';

function formatTime(date: Date): string {
  return date.toLocaleTimeString('pt-BR', { hour12: false });
}

export function QueuePage() {
  const { data, loading, error, lastUpdated } = useQueuePolling();

  const isQueueEmpty = !!data && !data.last && !data.current && data.next.length === 0;

  return (
    <div className="queue-page">
      <header className="queue-page-header">
        <h1 className="queue-page-title">Fila de separação</h1>
        <p className="queue-page-subtitle">
          Acompanhe em tempo real o andamento da fila processada pelo backend e pelo sistema
          físico de separação.
        </p>
        {lastUpdated && (
          <span className="queue-page-updated">Atualizado às {formatTime(lastUpdated)}</span>
        )}
        {error && data && (
          <p className="ui-message ui-message--error queue-page-warning">
            Sem conexão com o servidor. Exibindo a última informação recebida.
          </p>
        )}
      </header>

      {loading && !data && <p className="ui-message">Carregando fila...</p>}

      {!loading && !data && error && (
        <p className="ui-message ui-message--error">Não foi possível carregar a fila.</p>
      )}

      {data && isQueueEmpty && (
        <p className="ui-message">
          Nenhum item na fila. <Link to="/compra">Fazer uma compra</Link>
        </p>
      )}

      {data && !isQueueEmpty && (
        <div className="queue-page-layout">
          <QueueBoard snapshot={data} />
          <CameraViewer url={import.meta.env.VITE_CAMERA_URL} />
        </div>
      )}
    </div>
  );
}
