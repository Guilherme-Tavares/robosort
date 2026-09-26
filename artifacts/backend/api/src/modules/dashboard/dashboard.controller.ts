import { RequestHandler } from 'express';
import { DashboardService } from './dashboard.service';

const service = new DashboardService();

export class DashboardController {
  getData: RequestHandler = async (_req, res, next) => {
    try {
      res.json(await service.getData());
    } catch (error) {
      next(error);
    }
  };
}
