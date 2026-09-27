import { useCallback, useEffect, useState } from 'react';
import './DashboardPage.css';
import { StatCard } from '../../components/StatCard/StatCard';
import { RegionChart } from '../../components/RegionChart/RegionChart';
import { RegionTable } from '../../components/RegionTable/RegionTable';
import { getDashboard } from '../../services/dashboardService';
import type { DashboardData } from '../../types/dashboard';

type LoadState = 'loading' | 'success' | 'error';

// O painel agrega todas as compras, então é mais pesado que a fila: um
// intervalo maior basta para a tela não envelhecer.
const POLL_INTERVAL_MS = 10000;

export function DashboardPage() {
  const [state, setState] = useState<LoadState>('loading');
  const [data, setData] = useState<DashboardData | null>(null);

  const load = useCallback(() => {
    setState('loading');
    getDashboard()
      .then((result) => {
        setData(result);
        setState('success');
      })
      .catch(() => {
        setState('error');
      });
  }, []);

  useEffect(() => {
    load();
    const id = setInterval(load, POLL_INTERVAL_MS);
    return () => clearInterval(id);
  }, [load]);

  const servedRegionsCount =
    data?.regions.filter((region) => region.totalVolume > 0).length ?? 0;
  // Durante uma recarga os dados anteriores continuam visíveis até a nova resposta.
  const showData = (state === 'success' || state === 'loading') && data !== null;
  const isEmpty = showData && (data.regions.length === 0 || data.totalItems === 0);

  return (
    <div className="dashboard-page">
      {state === 'loading' && !data && <p className="ui-message">Carregando indicadores...</p>}

      {state === 'error' && (
        <div className="ui-message ui-message--error">
          <p>Não foi possível carregar os indicadores.</p>
          <button type="button" onClick={load}>
            Tentar novamente
          </button>
        </div>
      )}

      {isEmpty && <p className="ui-message">Nenhum dado disponível.</p>}

      {showData && !isEmpty && (
        <>
          <div className="dashboard-stats">
            <StatCard label="Itens processados" value={data.totalItems} />
            <StatCard label="Volumes totais" value={data.totalVolume} />
            <StatCard label="Regiões atendidas" value={servedRegionsCount} />
          </div>

          <section className="dashboard-section">
            <h2>Volumes por região</h2>
            <RegionChart regions={data.regions} />
          </section>

          <section className="dashboard-section">
            <h2>Tipos de produto por região</h2>
            <RegionTable regions={data.regions} />
          </section>
        </>
      )}
    </div>
  );
}
