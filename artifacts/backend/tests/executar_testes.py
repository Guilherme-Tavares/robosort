"""Roda a suite do orquestrador.

Cada teste e um script autonomo: imprime uma linha por verificacao e sai com
codigo != 0 se alguma falhar. Nenhum toca em hardware, na API real ou no
banco -- sobem servidores HTTP falsos e dubles de serial, camera e esteira.
Por isso rodam em qualquer maquina do time, sem bancada montada.

    .venv/Scripts/python tests/executar_testes.py        (Windows)
    .venv/bin/python tests/executar_testes.py            (Linux)
"""

import subprocess
import sys
from pathlib import Path

# O console do Windows costuma ser cp1252: sem isto, um acento vindo de um
# teste derruba o runner inteiro com UnicodeEncodeError.
for fluxo in (sys.stdout, sys.stderr):
    try:
        fluxo.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, OSError):
        pass

AQUI = Path(__file__).resolve().parent

# Ordem: dos modulos isolados para os que integram varios.
TESTES = [
    ("test_parse", "parser de 'mv re <r> es <e>' do firmware"),
    ("test_r4_boot", "conexao ao Uno R4 (USB nativo, sem READY)"),
    ("test_routing", "destino pela API e pelo mock"),
    ("test_report", "orquestrador informando o estagio a API"),
    ("test_queue_belt", "fila de ciclos e esteira automatica"),
]


def executarSuite():
    falhas = []
    for nome, descricao in TESTES:
        print(f"\n=== {nome}: {descricao}")
        processo = subprocess.run(
            [sys.executable, str(AQUI / f"{nome}.py")],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        print(processo.stdout.rstrip())
        if processo.stderr.strip():
            print(processo.stderr.rstrip(), file=sys.stderr)
        if processo.returncode != 0:
            falhas.append(nome)

    print("\n" + "-" * 60)
    if falhas:
        print(f"FALHOU: {', '.join(falhas)}")
        return 1
    print(f"{len(TESTES)} testes, todos passaram")
    return 0


if __name__ == "__main__":
    raise SystemExit(executarSuite())
