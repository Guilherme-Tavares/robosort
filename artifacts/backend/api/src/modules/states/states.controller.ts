import { RequestHandler } from 'express';
import { StatesService } from './states.service';

const service = new StatesService();

export class StatesController {
  findAll: RequestHandler = async (_req, res, next) => {
    try {
      res.json(await service.findAll());
    } catch (error) {
      next(error);
    }
  };
}