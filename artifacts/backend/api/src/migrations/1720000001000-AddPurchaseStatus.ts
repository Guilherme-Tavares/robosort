import { MigrationInterface, QueryRunner } from 'typeorm';

// O orquestrador informa em que pe esta a separacao de cada compra: ao
// comecar marca 'sorting', ao terminar 'done' (ou 'error'). A fila da tela
// sai daqui: 'sorting' e o item atual, 'pending' sao os proximos e o ultimo
// 'done' e o anterior. Sem isso a fila so existiria na memoria do
// orquestrador e sumiria a cada reinicio.
export class AddPurchaseStatus1720000001000 implements MigrationInterface {
  name = 'AddPurchaseStatus1720000001000';

  public async up(queryRunner: QueryRunner): Promise<void> {
    await queryRunner.query(`
      ALTER TABLE Purchase
        ADD COLUMN status varchar(16) NOT NULL DEFAULT 'pending',
        ADD COLUMN sorted_at datetime NULL
    `);
    await queryRunner.query(`
      CREATE INDEX IDX_PURCHASE_STATUS ON Purchase (status)
    `);
  }

  public async down(queryRunner: QueryRunner): Promise<void> {
    await queryRunner.query('DROP INDEX IDX_PURCHASE_STATUS ON Purchase');
    await queryRunner.query(`
      ALTER TABLE Purchase
        DROP COLUMN sorted_at,
        DROP COLUMN status
    `);
  }
}
