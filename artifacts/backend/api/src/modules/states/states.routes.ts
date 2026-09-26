import { Router } from 'express';
import { StatesController } from './states.controller';

const controller = new StatesController();

export const statesRouter = Router();

statesRouter.get('/states', controller.findAll);