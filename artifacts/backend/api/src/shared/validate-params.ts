import { plainToInstance } from 'class-transformer';
import { validate } from 'class-validator';
import { RequestHandler } from 'express';
import { HttpError } from '../shared/http-error';

export function validateParams<T extends object>(Dto: new () => T): RequestHandler {
  return async (req, _res, next) => {
    const dto = plainToInstance(Dto, req.params, { enableImplicitConversion: true });
    const errors = await validate(dto, { whitelist: true, forbidNonWhitelisted: true });
    if (errors.length) return next(new HttpError(400, 'Parametros inválidos.'));
    req.params = dto as typeof req.params;
    return next();
  };
}
