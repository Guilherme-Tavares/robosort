import './RegionChart.css';
import type { RegionSummary } from '../../types/dashboard';

interface RegionChartProps {
  regions: RegionSummary[];
}

export function RegionChart({ regions }: RegionChartProps) {
  const maxVolume = Math.max(0, ...regions.map((region) => region.totalVolume));

  const summary = regions
    .map((region) => `${region.name}: ${region.totalVolume} volumes`)
    .join(', ');

  return (
    <div className="region-chart" role="img" aria-label={`Volumes por região: ${summary}`}>
      {regions.map((region) => {
        const width = maxVolume > 0 ? (region.totalVolume / maxVolume) * 100 : 0;

        return (
          <div className="region-chart-row" key={region.name}>
            <span className="region-chart-label">{region.name}</span>
            <div className="region-chart-track">
              <div className="region-chart-bar" style={{ width: `${width}%` }} />
            </div>
            <span className="region-chart-value">{region.totalVolume} volumes</span>
          </div>
        );
      })}
    </div>
  );
}
