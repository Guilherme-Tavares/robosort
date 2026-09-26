import { Column, Entity, OneToMany, PrimaryGeneratedColumn } from 'typeorm';
import { Purchase } from '../../purchase/entities/purchase.entity';

@Entity('Product')
export class Product {
  @PrimaryGeneratedColumn()
  id!: number;

  @Column()
  name!: string;

  @Column()
  description!: string;

  @Column({ name: 'image_url' })
  imageUrl!: string;

  @Column()
  type!: string;

  @Column()
  volume!: number;

  @Column({ type: 'decimal', precision: 10, scale: 2 })
  price!: string;

  @OneToMany(() => Purchase, (purchase) => purchase.product)
  purchases!: Purchase[];
}
