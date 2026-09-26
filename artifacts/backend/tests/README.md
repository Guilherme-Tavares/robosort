# Testes do orquestrador

Suíte de regressão do orquestrador e do contrato com o firmware. Nenhum teste
toca em hardware, na API real ou no banco: todos sobem servidores HTTP falsos
e dublês de serial, câmera e esteira. Rodam em qualquer máquina do time, com a
bancada desmontada e a API desligada.

## Como rodar

A partir de `artifacts/backend/`:

```bash
.venv/Scripts/python tests/executar_testes.py   # Windows
.venv/bin/python tests/executar_testes.py       # Linux
```

O runner devolve código de saída diferente de zero se qualquer teste falhar.
Cada arquivo também roda sozinho:

```bash
.venv/Scripts/python tests/test_routing.py
```

## O que cada um cobre

| Teste | Cobertura |
|---|---|
| `test_parse` | Parser de `mv re <r> es <e> [--arm]`: regiões e estados fora da faixa, espaços extras, sufixo inválido. |
| `test_r4_boot` | Conexão ao Uno R4, cujo USB nativo não reinicia a placa: o `READY` do boot se perdeu, então a sonda `ping` é que confirma a porta. Cobre também placa trocada. |
| `test_routing` | Destino pelas duas fontes. Que o destino é do **pedido**, não do ID (10 → SP, não RO); pedido inexistente; API fora do ar; resposta inválida; região desconhecida; e que `api` **nunca** cai no mock. |
| `test_report` | `routing.report()` informando `sorting`/`done`/`error`. Falha ao reportar devolve aviso e não levanta: o hardware já agiu. |
| `test_queue_belt` | Uma operação por ID mesmo lido em vários frames; menor ID quando há mais de uma caixinha; reuso de ID já operado; esteira liga ao ver caixinha e desliga após `CONVEYOR_IDLE_STOP`. |

## Duas limitações conhecidas

`test_parse.py` é uma **tradução** do parser do sketch para Python, não o código
do firmware. Se o parser mudar em `robosort-firmware`, o teste não acusa sozinho —
a tradução precisa acompanhar. O `test_parse.cpp` ao lado compila a lógica real
em C++ e existe para conferir a tradução, mas exige MSVC e por isso fica fora do
runner:

```bat
cl /nologo /EHsc /Fe:test_parse.exe test_parse.cpp && test_parse.exe
```

`test_queue_belt` fixa `config.ROUTE_SOURCE = "mock"` de propósito: ele cobre
fila e esteira, e sem isso passaria a depender do que estiver gravado no banco.
