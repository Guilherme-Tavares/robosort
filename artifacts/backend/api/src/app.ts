import cors from 'cors';
import express, { ErrorRequestHandler } from 'express';
import { purchaseRouter } from './modules/purchase/purchase.routes';
import { productsRouter } from './modules/products/products.routes';
import { statesRouter } from './modules/states/states.routes';
import { HttpError } from './shared/http-error';

export const app = express();

app.use(cors());
app.use(express.json());
app.use('/api', purchaseRouter);
app.use('/api', productsRouter);
app.use('/api', statesRouter);

const errorHandler: ErrorRequestHandler = (error, _req, res, _next) => {
  const status = error instanceof HttpError ? error.status : 500;
  res.status(status).json({ error: error.message ?? 'Erro interno.' });
};

app.use(errorHandler);