import { RequestHandler } from 'express';
import { PurchaseService } from './purchase.service';
import { GetPurchaseDto } from './dto/get-purchase.dto';

const service = new PurchaseService();

export class PurchaseController {
  create: RequestHandler = async (req, res, next) => {
    try {
      res.status(201).json(await service.create(req.body));
    } catch (error) {
      next(error);
    }
  };

  findLocation: RequestHandler = async (req, res, next) => {
    try {
      const { volume } = req.params as unknown as GetPurchaseDto;
      res.json(await service.findLocation(volume));
    } catch (error) {
      next(error);
    }
  };
}
