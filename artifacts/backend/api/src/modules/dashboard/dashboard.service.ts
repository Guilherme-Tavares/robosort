import { AppDataSource } from '../../database/data-source';
import { Purchase } from '../purchase/entities/purchase.entity';
import { Region } from '../states/entities/region.entity';

// Painel por regiao: quantas compras e quanto volume cada uma acumulou, e o
// volume por tipo de produto. Historico, entao vem do banco (e nao do
// orquestrador, que so sabe do ciclo corrente). Toda regiao aparece, mesmo
// sem compra, para a tela nao mudar de forma conforme o movimento.
export class DashboardService {
  private readonly purchaseRepository = AppDataSource.getRepository(Purchase);
  private readonly regionRepository = AppDataSource.getRepository(Region);

  async getData() {
    const [regions, purchases] = await Promise.all([
      this.regionRepository.find({ order: { id: 'ASC' } }),
      this.purchaseRepository.find({ relations: { product: true, state: { region: true } } }),
    ]);

    const porRegiao = new Map<string, Purchase[]>(regions.map((r) => [r.name, []]));
    for (const compra of purchases) {
      porRegiao.get(compra.state.region.name)?.push(compra);
    }

    const resumo = regions.map((region) => {
      const compras = porRegiao.get(region.name) ?? [];
      const porTipo = new Map<string, number>();
      for (const compra of compras) {
        porTipo.set(compra.product.type, (porTipo.get(compra.product.type) ?? 0) + compra.product.volume);
      }
      return {
        name: region.name,
        totalItems: compras.length,
        totalVolume: compras.reduce((soma, c) => soma + c.product.volume, 0),
        products: [...porTipo.entries()]
          .map(([type, volume]) => ({ type, volume }))
          .sort((a, b) => a.type.localeCompare(b.type)),
      };
    });

    return {
      totalItems: purchases.length,
      totalVolume: purchases.reduce((soma, c) => soma + c.product.volume, 0),
      regions: resumo,
    };
  }
}
