import './RegionTable.css';
import type { RegionSummary } from '../../types/dashboard';

interface RegionTableProps {
  regions: RegionSummary[];
}

export function RegionTable({ regions }: RegionTableProps) {
  return (
    <table className="region-table">
      <thead>
        <tr>
          <th>Região</th>
          <th>Tipo</th>
          <th className="region-table-volume">Volume</th>
        </tr>
      </thead>
      <tbody>
        {regions.flatMap((region) =>
          region.products.map((product) => (
            <tr key={`${region.name}-${product.type}`}>
              <td>{region.name}</td>
              <td>{product.type}</td>
              <td className="region-table-volume">{product.volume}</td>
            </tr>
          )),
        )}
      </tbody>
    </table>
  );
}
