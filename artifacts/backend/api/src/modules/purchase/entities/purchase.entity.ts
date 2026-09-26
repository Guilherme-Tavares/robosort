import { Column, CreateDateColumn, Entity, JoinColumn, ManyToOne, PrimaryGeneratedColumn } from 'typeorm';
import { Product } from '../../products/entities/product.entity';
import { State } from '../../states/entities/state.entity';

@Entity('Purchase')
export class Purchase {
  @PrimaryGeneratedColumn()
  id!: number;

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

  @CreateDateColumn({ name: 'created_at' })
  createdAt!: Date;
}
