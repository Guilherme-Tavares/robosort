import { Column, Entity, JoinColumn, ManyToOne, OneToMany, PrimaryColumn } from 'typeorm';
import { Purchase } from '../../purchase/entities/purchase.entity';
import { Region } from './region.entity';

@Entity('State')
export class State {
  @PrimaryColumn({ length: 2 })
  uf!: string;

  @Column({ unique: true })
  name!: string;

  @ManyToOne(() => Region, (region) => region.states, { nullable: false })
  @JoinColumn({ name: 'region_id' })
  region!: Region;

  @OneToMany(() => Purchase, (purchase) => purchase.state)
  purchases!: Purchase[];
}
