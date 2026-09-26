import 'reflect-metadata';
import { app } from './app';
import { AppDataSource } from './database/data-source';

const port = Number(process.env.PORT ?? 3000);

AppDataSource.initialize()
  .then(() => {
    app.listen(port, () => console.log(`API em http://localhost:${port}`));
  })
  .catch((error) => {
    console.error('Erro ao conectar no banco:', error);
    process.exit(1);
  });
