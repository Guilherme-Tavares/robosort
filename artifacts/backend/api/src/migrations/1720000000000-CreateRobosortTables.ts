import { MigrationInterface, QueryRunner } from 'typeorm';

export class CreateRobosortTables1720000000000 implements MigrationInterface {
  name = 'CreateRobosortTables1720000000000';

  public async up(queryRunner: QueryRunner): Promise<void> {
    await queryRunner.query(`
      CREATE TABLE Region (
        id int NOT NULL AUTO_INCREMENT,
        name varchar(255) NOT NULL,
        UNIQUE INDEX IDX_REGION_NAME (name),
        PRIMARY KEY (id)
      ) ENGINE=InnoDB
    `);

    await queryRunner.query(`
      CREATE TABLE State (
        uf varchar(2) NOT NULL,
        name varchar(255) NOT NULL,
        region_id int NOT NULL,
        UNIQUE INDEX IDX_STATE_NAME (name),
        PRIMARY KEY (uf),
        CONSTRAINT FK_STATE_REGION FOREIGN KEY (region_id) REFERENCES Region(id)
      ) ENGINE=InnoDB
    `);

    await queryRunner.query(`
      CREATE TABLE Product (
        id int NOT NULL AUTO_INCREMENT,
        name varchar(255) NOT NULL,
        description varchar(255) NOT NULL,
        image_url varchar(255) NOT NULL,
        type varchar(255) NOT NULL,
        volume int NOT NULL,
        price decimal(10,2) NOT NULL,
        PRIMARY KEY (id)
      ) ENGINE=InnoDB
    `);

    await queryRunner.query(`
      CREATE TABLE Purchase (
        id int NOT NULL AUTO_INCREMENT,
        volume int NOT NULL,
        product_id int NOT NULL,
        state_uf varchar(2) NOT NULL,
        city varchar(255) NOT NULL,
        created_at datetime(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
        UNIQUE INDEX IDX_PURCHASE_VOLUME (volume),
        PRIMARY KEY (id),
        CONSTRAINT FK_PURCHASE_PRODUCT FOREIGN KEY (product_id) REFERENCES Product(id),
        CONSTRAINT FK_PURCHASE_STATE FOREIGN KEY (state_uf) REFERENCES State(uf)
      ) ENGINE=InnoDB
    `);

    await queryRunner.query(`
      INSERT INTO Region (id, name) VALUES
      (1, 'Norte'), (2, 'Nordeste'), (3, 'Centro-Oeste'), (4, 'Sudeste'), (5, 'Sul')
    `);

    await queryRunner.query(`
      INSERT INTO State (uf, name, region_id) VALUES
      ('RO', 'Rondônia', 1),
      ('AC', 'Acre', 1),
      ('BA', 'Bahia', 2),
      ('CE', 'Ceará', 2),
      ('GO', 'Goiás', 3),
      ('MT', 'Mato Grosso', 3),
      ('SP', 'São Paulo', 4),
      ('RJ', 'Rio de Janeiro', 4),
      ('PR', 'Paraná', 5),
      ('RS', 'Rio Grande do Sul', 5)
    `);

    await queryRunner.query(`
      INSERT INTO Product (id, name, description, image_url, type, volume, price) VALUES
      (1, 'Fone de Ouvido Bluetooth', 'Fone sem fio com cancelamento de ruído.', '/images/product-1.svg', 'Eletrônico', 1, 149.90),
      (2, 'Smartwatch', 'Relógio inteligente com monitor de atividades.', '/images/product-2.svg', 'Eletrônico', 2, 299.90),
      (3, 'Pacote de Café', 'Café torrado e moído, pacote de 500g.', '/images/product-3.svg', 'Alimentício', 3, 24.90),
      (4, 'Caixa de Cereal', 'Cereal matinal integral, caixa de 300g.', '/images/product-4.svg', 'Alimentício', 2, 18.50),
      (5, 'Camiseta Básica', 'Camiseta de algodão, cores variadas.', '/images/product-5.svg', 'Vestuário', 1, 39.90),
      (6, 'Jaqueta Corta-Vento', 'Jaqueta leve e impermeável.', '/images/product-6.svg', 'Vestuário', 5, 189.90)
    `);
  }

  public async down(queryRunner: QueryRunner): Promise<void> {
    await queryRunner.query('DROP TABLE Purchase');
    await queryRunner.query('DROP TABLE Product');
    await queryRunner.query('DROP TABLE State');
    await queryRunner.query('DROP TABLE Region');
  }
}
