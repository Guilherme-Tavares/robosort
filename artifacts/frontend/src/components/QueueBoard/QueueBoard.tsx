import type { QueueSnapshot } from '../../types/queue';
import { QueueItem } from '../QueueItem/QueueItem';
import './QueueBoard.css';

interface QueueBoardProps {
  snapshot: QueueSnapshot;
}

const NEXT_SLOTS = 5;

export function QueueBoard({ snapshot }: QueueBoardProps) {
  const nextSlots = Array.from({ length: NEXT_SLOTS }, (_, index) => snapshot.next[index] ?? null);

  return (
    <div className="queue-board">
      <section className="queue-board-section">
        <h2 className="queue-board-title">Último processado</h2>
        <QueueItem entry={snapshot.last} variant="last" />
      </section>

      <section className="queue-board-section">
        <h2 className="queue-board-title">Em separação agora</h2>
        <QueueItem entry={snapshot.current} variant="current" />
      </section>

      <section className="queue-board-section">
        <h2 className="queue-board-title">Próximos</h2>
        <ul className="queue-board-next-list">
          {nextSlots.map((entry, index) => (
            <li key={entry?.id ?? `empty-${index}`}>
              <QueueItem entry={entry} variant="next" position={index + 1} />
            </li>
          ))}
        </ul>
      </section>
    </div>
  );
}
