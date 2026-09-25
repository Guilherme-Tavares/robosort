#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "Este instalador e exclusivo para Linux." >&2
  exit 1
fi

pasta_repositorio="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
python_bin="${PYTHON_BIN:-python3}"

if ! command -v "$python_bin" >/dev/null 2>&1; then
  echo "Python 3 nao encontrado: $python_bin" >&2
  exit 1
fi

if ! "$python_bin" -m venv "$pasta_repositorio/.venv"; then
  echo "Falha ao criar .venv. Em Debian/Ubuntu, instale o pacote python3-venv." >&2
  exit 1
fi

"$pasta_repositorio/.venv/bin/python" -m pip install --upgrade pip
"$pasta_repositorio/.venv/bin/python" -m pip install -r "$pasta_repositorio/requirements.txt"

PYGAME_HIDE_SUPPORT_PROMPT=1 "$pasta_repositorio/.venv/bin/python" -c \
  "import cv2, ev3_dc, numpy, pygame, serial, usb.core; print('Dependencias Python verificadas.')"

echo
echo "Ambiente pronto em $pasta_repositorio/.venv"
echo "Ative com: source .venv/bin/activate"
echo "Para acessar Arduino e EV3 sem sudo, instale as regras udev descritas no README.md."
