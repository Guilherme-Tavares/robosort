import { IsIn } from 'class-validator';
import { PURCHASE_STATUSES, PurchaseStatus } from '../entities/purchase.entity';

export class UpdateStatusDto {
  @IsIn(PURCHASE_STATUSES)
  status!: PurchaseStatus;
}
