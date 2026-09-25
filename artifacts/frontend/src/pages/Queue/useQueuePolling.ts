import { useEffect, useRef, useState } from 'react';
import { getQueue } from '../../services/queueService';
import type { QueueSnapshot } from '../../types/queue';

const POLL_INTERVAL_MS = 2500;

interface QueuePollingState {
  data: QueueSnapshot | null;
  loading: boolean;
  error: boolean;
  lastUpdated: Date | null;
}

export function useQueuePolling(): QueuePollingState {
  const [data, setData] = useState<QueueSnapshot | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(false);
  const [lastUpdated, setLastUpdated] = useState<Date | null>(null);

  const timeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    let active = true;

    async function fetchQueue() {
      try {
        const snapshot = await getQueue();
        if (!active) return;
        setData(snapshot);
        setError(false);
        setLastUpdated(new Date());
      } catch {
        if (!active) return;
        // Mantém o último `data` válido na tela; só sinaliza o erro.
        setError(true);
      }

      setLoading(false);
      timeoutRef.current = setTimeout(fetchQueue, POLL_INTERVAL_MS);
    }

    fetchQueue();

    return () => {
      active = false;
      if (timeoutRef.current !== null) {
        clearTimeout(timeoutRef.current);
      }
    };
  }, []);

  return { data, loading, error, lastUpdated };
}
