import { AppDataSource } from '../../database/data-source';
import { Product } from '../products/entities/product.entity';
import { HttpError } from '../../shared/http-error';
import { Purchase, PurchaseStatus } from './entities/purchase.entity';
import { State } from '../states/entities/state.entity';
import { CreatePurchaseDto } from './dto/create-purchase.dto';

// O front mostra o estagio em portugues; o banco guarda em ingles.
const STATUS_LABEL: Record<PurchaseStatus, string> = {
  pending: 'Aguardando',
  sorting: 'Separando',
  done: 'Concluído',
  error: 'Erro',
};

const MAX_NEXT = 5;

export class PurchaseService {
  private readonly purchaseRepository = AppDataSource.getRepository(Purchase);
  private readonly productRepository = AppDataSource.getRepository(Product);
  private readonly stateRepository = AppDataSource.getRepository(State);

  async create(dto: CreatePurchaseDto) {
    const [product, state] = await Promise.all([
      this.productRepository.findOne({ where: { id: dto.productId } }),
      this.stateRepository.findOne({ where: { uf: dto.state.toUpperCase() } }),
    ]);

    if (!product) throw new HttpError(400, 'Produto inválido.');
    if (!state) throw new HttpError(400, 'Estado inválido.');

    const lastVolume = await this.purchaseRepository
      .createQueryBuilder('purchase')
      .select('MAX(purchase.volume)', 'max')
      .getRawOne<{ max: string | null }>();

    const saved = await this.purchaseRepository.save(
      this.purchaseRepository.create({
        volume: Math.max(Number(lastVolume?.max ?? 9) + 1, 10),
        product,
        state,
        city: dto.city,
        status: 'pending',
        sortedAt: null,
      }),
    );

    return { id: saved.id, volume: saved.volume };
  }

  async findLocation(volume: number) {
    const row = await this.purchaseRepository.findOne({
      where: { volume },
      relations: { state: { region: true } },
    });
    if (!row) throw new HttpError(404, 'Compra não encontrada.');

    return {
      volume: row.volume,
      state: { uf: row.state.uf, name: row.state.name },
      region: row.state.region.name,
    };
  }

  // Chamado pelo orquestrador: 'sorting' quando o braço começa, 'done' ou
  // 'error' quando termina. Só uma compra fica em 'sorting' por vez — se
  // outra estava, ela cai para 'error': o ciclo dela nunca foi confirmado.
  async updateStatus(volume: number, status: PurchaseStatus) {
    const row = await this.purchaseRepository.findOne({ where: { volume } });
    if (!row) throw new HttpError(404, 'Compra não encontrada.');

    if (status === 'sorting') {
      await this.purchaseRepository
        .createQueryBuilder()
        .update(Purchase)
        .set({ status: 'error' })
        .where('status = :sorting AND volume <> :volume', { sorting: 'sorting', volume })
        .execute();
    }

    row.status = status;
    row.sortedAt = status === 'done' || status === 'error' ? new Date() : null;
    await this.purchaseRepository.save(row);

    return { volume: row.volume, status: row.status };
  }

  async getQueue() {
    const rows = await this.purchaseRepository.find({
      relations: { product: true, state: true },
      order: { volume: 'ASC' },
    });

    const entry = (row: Purchase) => ({
      id: row.id,
      volume: row.volume,
      product: {
        id: row.product.id,
        name: row.product.name,
        imageUrl: row.product.imageUrl,
        type: row.product.type,
        volume: row.product.volume,
      },
      state: row.state.uf,
      city: row.city,
      status: STATUS_LABEL[row.status],
    });

    const done = rows.filter((r) => r.status === 'done' || r.status === 'error');
    const last = done.reduce<Purchase | null>(
      (mais, r) => (!mais || (r.sortedAt ?? 0) >= (mais.sortedAt ?? 0) ? r : mais),
      null,
    );

    return {
      last: last ? entry(last) : null,
      current: rows.filter((r) => r.status === 'sorting').map(entry)[0] ?? null,
      next: rows.filter((r) => r.status === 'pending').slice(0, MAX_NEXT).map(entry),
    };
  }
}
