import { config } from 'dotenv';
import { existsSync } from 'node:fs';
import { createConnection } from 'mysql2/promise';
import { resolve } from 'node:path';

// Derruba e recria o schema, para a demonstracao comecar sempre do zero: as
// migrations recriam as tabelas e o seed (regioes, estados, produtos), e o
// primeiro pedido volta a sair com o marcador 10.
//
// Roda antes das migrations, conectado ao servidor SEM selecionar banco: o
// TypeORM cria tabelas, nao o schema, e nem conseguiria conectar a um banco
// que acabou de ser derrubado.

const envFile = process.env.NODE_ENV === 'production' ? '.env' : '.env.development';
const envPath = resolve(process.cwd(), envFile);
config({ path: existsSync(envPath) ? envPath : resolve(process.cwd(), '.env.example') });

const database = process.env.DB_NAME ?? 'robosort';

// O nome vai cru na instrucao (identificador nao aceita placeholder), entao
// so passam nomes simples.
if (!/^[A-Za-z0-9_]+$/.test(database)) {
  console.error(`DB_NAME invalido: '${database}'. Use apenas letras, numeros e _.`);
  process.exit(1);
}

async function main(): Promise<void> {
  const connection = await createConnection({
    host: process.env.DB_HOST ?? 'localhost',
    port: Number(process.env.DB_PORT ?? 3306),
    user: process.env.DB_USER ?? 'root',
    password: process.env.DB_PASS ?? '',
    multipleStatements: false,
  });

  try {
    await connection.query(`DROP DATABASE IF EXISTS \`${database}\``);
    await connection.query(
      `CREATE DATABASE \`${database}\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci`,
    );
    console.log(`Banco '${database}' derrubado e recriado, vazio.`);
  } finally {
    await connection.end();
  }
}

main().catch((error) => {
  console.error('Erro ao recriar o banco:', error);
  process.exit(1);
});
