#!/usr/bin/env bash
# Configura e COMPILA. A etapa `cmake --build` e a que gera o binario -
# `cmake -S -B` sozinho so escreve os arquivos de projeto.
set -euo pipefail

cd "$(dirname "$0")"

cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build "$@"

echo
echo "pronto: ./build/CppGame"
