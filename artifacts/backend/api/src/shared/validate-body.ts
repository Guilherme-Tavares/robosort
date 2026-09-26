import { validate } from 'class-validator';
import { plainToInstance } from 'class-transformer';
import { RequestHandler } from 'express';
import { HttpError } from '../shared/http-error';

export function validateBody<T extends object>(Dto: new () => T): RequestHandler {
  return async (req, _res, next) => {
    const dto = plainToInstance(Dto, req.body);
    const errors = await validate(dto, { whitelist: true, forbidNonWhitelisted: true });
    if (errors.length) return next(new HttpError(400, 'Dados inválidos.'));
    req.body = dto;
    return next();
  };
}

