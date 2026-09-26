import { Column, Entity, OneToMany, PrimaryGeneratedColumn } from 'typeorm';
import { State } from './state.entity';

@Entity('Region')
export class Region {
  @PrimaryGeneratedColumn()
  id!: number;

  @Column({ unique: true })
  name!: string;

  @OneToMany(() => State, (state) => state.region)
  states!: State[];
}
