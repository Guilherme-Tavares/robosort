import { AppDataSource } from '../../database/data-source';
import { State } from './entities/state.entity';

export class StatesService {
  private readonly stateRepository = AppDataSource.getRepository(State);

  findAll() {
    return this.stateRepository.find({
      relations: { region: true },
      order: { uf: 'ASC' },
    });
  }
}