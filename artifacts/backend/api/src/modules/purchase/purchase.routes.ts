import { Router } from 'express';
import { validateBody } from '../../shared/validate-body';
import { validateParams } from '../../shared/validate-params';
import { CreatePurchaseDto } from './dto/create-purchase.dto';
import { GetPurchaseDto } from './dto/get-purchase.dto';
import { UpdateStatusDto } from './dto/update-status.dto';
import { PurchaseController } from './purchase.controller';

const controller = new PurchaseController();

export const purchaseRouter = Router();

purchaseRouter.post('/purchase', validateBody(CreatePurchaseDto), controller.create);
purchaseRouter.get('/purchase/:volume', validateParams(GetPurchaseDto), controller.findLocation);
purchaseRouter.patch(
  '/purchase/:volume/status',
  validateParams(GetPurchaseDto),
  validateBody(UpdateStatusDto),
  controller.updateStatus,
);
purchaseRouter.get('/queue', controller.queue);
