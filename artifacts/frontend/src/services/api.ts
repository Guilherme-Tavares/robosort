// Camada HTTP base do frontend. Usa fetch nativo, sem retry, interceptors ou cache.

export const USE_MOCK = import.meta.env.VITE_USE_MOCK === 'true';

export class ApiError extends Error {
  status?: number;

  constructor(message: string, status?: number) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
  }
}

function buildUrl(path: string): string {
  return `${import.meta.env.VITE_API_BASE_URL ?? ''}${path}`;
}

async function parseJson<T>(response: Response): Promise<T> {
  try {
    return (await response.json()) as T;
  } catch {
    throw new ApiError('Resposta inválida do servidor.', response.status);
  }
}

export async function httpGet<T>(path: string): Promise<T> {
  let response: Response;
  try {
    response = await fetch(buildUrl(path));
  } catch {
    throw new ApiError('Não foi possível conectar ao servidor.');
  }

  if (!response.ok) {
    throw new ApiError('O servidor retornou um erro.', response.status);
  }

  return parseJson<T>(response);
}

export async function httpPost<TBody, TResponse>(
  path: string,
  body: TBody,
): Promise<TResponse> {
  let response: Response;
  try {
    response = await fetch(buildUrl(path), {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
  } catch {
    throw new ApiError('Não foi possível conectar ao servidor.');
  }

  if (!response.ok) {
    throw new ApiError('O servidor retornou um erro.', response.status);
  }

  return parseJson<TResponse>(response);
}
