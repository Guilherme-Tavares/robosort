import { Router } from 'express';
import { ProductsController } from './products.controller';

const controller = new ProductsController();

export const productsRouter = Router();

productsRouter.get('/products', controller.findAll);
