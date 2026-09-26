import { Type } from 'class-transformer';
import { IsInt, IsNotEmpty, IsPositive, IsString, Length } from 'class-validator';

export class CreatePurchaseDto {
  @IsInt()
  @IsPositive()
  @Type(() => Number)
  productId!: number;

  @IsString()
  @Length(2, 2)
  state!: string;

  @IsString()
  @IsNotEmpty()
  city!: string;
}
