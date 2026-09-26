import { Column, CreateDateColumn, Entity, JoinColumn, ManyToOne, PrimaryGeneratedColumn } from 'typeorm';
import { Product } from '../../products/entities/product.entity';
import { State } from '../../states/entities/state.entity';

// Estagio da separacao, informado pelo orquestrador. 'pending' e o padrao de
// quem acabou de ser comprado; o resto vem de PATCH /purchase/:volume/status.
export type PurchaseStatus = 'pending' | 'sorting' | 'done' | 'error';

export const PURCHASE_STATUSES: PurchaseStatus[] = ['pending', 'sorting', 'done', 'error'];

@Entity('Purchase')
export class Purchase {
  @PrimaryGeneratedColumn()
  id!: number;

  // Numero do marcador ArUco colado na caixa: gerado a partir de 10 e
  // incrementado a cada compra. E por ele que o orquestrador pergunta o
  // destino depois de ler a caixa com a camera.
  @Column({ unique: true })
  volume!: number;

  @ManyToOne(() => Product, (product) => product.purchases, { nullable: false })
  @JoinColumn({ name: 'product_id' })
  product!: Product;

  @ManyToOne(() => State, (state) => state.purchases, { nullable: false })
  @JoinColumn({ name: 'state_uf' })
  state!: State;

  @Column()
  city!: string;

  @Column({ type: 'varchar', length: 16, default: 'pending' })
  status!: PurchaseStatus;

  @Column({ name: 'sorted_at', type: 'datetime', nullable: true })
  sortedAt!: Date | null;

  @CreateDateColumn({ name: 'created_at' })
  createdAt!: Date;
}
