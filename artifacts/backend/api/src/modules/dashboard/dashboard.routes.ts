import { Router } from 'express';
import { DashboardController } from './dashboard.controller';

const controller = new DashboardController();

export const dashboardRouter = Router();

dashboardRouter.get('/dashboard', controller.getData);
