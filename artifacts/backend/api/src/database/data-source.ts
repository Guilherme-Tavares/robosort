import { config } from 'dotenv';
import { existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { DataSource } from 'typeorm';
import { Product } from '../modules/products/entities/product.entity';
import { Purchase } from '../modules/purchase/entities/purchase.entity';
import { Region } from '../modules/states/entities/region.entity';
import { State } from '../modules/states/entities/state.entity';

const envFile = process.env.NODE_ENV === 'production' ? '.env' : '.env.development';
const envPath = resolve(process.cwd(), envFile);
config({ path: existsSync(envPath) ? envPath : resolve(process.cwd(), '.env.example') });

export const AppDataSource = new DataSource({
  type: 'mysql',
  host: process.env.DB_HOST ?? 'localhost',
  port: Number(process.env.DB_PORT ?? 3306),
  username: process.env.DB_USER ?? 'root',
  password: process.env.DB_PASS ?? '',
  database: process.env.DB_NAME ?? 'robosort',
  entities: [Product, Purchase, Region, State],
  migrations: ['src/migrations/*.ts'],
  synchronize: false,
});
