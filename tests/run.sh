#!/usr/bin/env bash
# Compila e roda a validação numérica do gerador, sem abrir janela.
set -uo pipefail
cd "$(dirname "$0")/.."

RAYLIB_SRC=build/_deps/raylib-src/src
RAYLIB_LIB=build/_deps/raylib-build/raylib/libraylib.a

if [ ! -f "$RAYLIB_LIB" ]; then
    echo "raylib ainda nao compilada; rode ./build.sh antes" >&2
    exit 1
fi

FRAMEWORKS="-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL"
FONTES="src/world/TerrainGenerator.cpp src/world/Noise.cpp"
falhas=0

for teste in tests/*_validation.cpp; do
    nome=$(basename "$teste" .cpp)
    echo "=== $nome ==="
    clang++ -std=c++2c -I src -I "$RAYLIB_SRC" -O2 \
        "$teste" $FONTES "$RAYLIB_LIB" $FRAMEWORKS -o "/tmp/$nome" || { falhas=$((falhas+1)); continue; }
    "/tmp/$nome" || falhas=$((falhas+1))
    echo
done

echo "=================================="
if [ "$falhas" -eq 0 ]; then echo "tudo passou"; else echo "$falhas teste(s) falharam"; fi
exit "$falhas"
