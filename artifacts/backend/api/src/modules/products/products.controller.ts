import { RequestHandler } from 'express';
import { ProductsService } from './products.service';

const service = new ProductsService();

export class ProductsController {
  findAll: RequestHandler = async (_req, res, next) => {
    try {
      res.json(await service.findAll());
    } catch (error) {
      next(error);
    }
  };
}
