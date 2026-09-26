import { AppDataSource } from '../../database/data-source';
import { Product } from '../products/entities/product.entity';
import { HttpError } from '../../shared/http-error';
import { Purchase } from './entities/purchase.entity';
import { State } from '../states/entities/state.entity';
import { CreatePurchaseDto } from './dto/create-purchase.dto';

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
}
