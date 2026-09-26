import { RequestHandler } from 'express';
import { PurchaseService } from './purchase.service';
import { GetPurchaseDto } from './dto/get-purchase.dto';
import { UpdateStatusDto } from './dto/update-status.dto';

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

  // Chamado pelo orquestrador ao iniciar e ao concluir a separacao.
  updateStatus: RequestHandler = async (req, res, next) => {
    try {
      const { volume } = req.params as unknown as GetPurchaseDto;
      const { status } = req.body as UpdateStatusDto;
      res.json(await service.updateStatus(volume, status));
    } catch (error) {
      next(error);
    }
  };

  // Fila da tela: ultimo concluido, o que esta em separacao e os proximos.
  queue: RequestHandler = async (_req, res, next) => {
    try {
      res.json(await service.getQueue());
    } catch (error) {
      next(error);
    }
  };
}
