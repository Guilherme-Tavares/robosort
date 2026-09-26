import { Product } from './entities/product.entity';
import { AppDataSource } from '../../database/data-source';

export class ProductsService {
  private readonly productRepository = AppDataSource.getRepository(Product);

  findAll() {
    return this.productRepository.find({ order: { id: 'ASC' } });
  }
}
