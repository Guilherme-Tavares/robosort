import { Type } from 'class-transformer';
import { IsInt, IsPositive } from 'class-validator';

export class GetPurchaseDto {
  @IsInt()
  @IsPositive()
  @Type(() => Number)
  volume!: number;
}
