#!/usr/bin/env bash
set -euo pipefail

echo "[BUILD] StarRaster (native)..."

mkdir -p build

OS="$(uname)"
CC="${CC:-cc}"

if [[ "$OS" == "Darwin" ]]; then
    # macOS: sokol includes ObjC headers, so compile sr_main as ObjC
    $CC -O2 -std=c99 -c \
        -I src -I third_party \
        src/sr_raster.c src/sr_texture.c src/sr_gif.c \
        -lm
    $CC -O2 -x objective-c \
        -I src -I third_party \
        -c src/sr_main.c -o sr_main.o
    $CC -O2 \
        sr_main.o sr_raster.o sr_texture.o sr_gif.o \
        -o build/starraster \
        -framework Cocoa -framework OpenGL -framework QuartzCore \
        -lm
    rm -f sr_main.o sr_raster.o sr_texture.o sr_gif.o
else
    # Linux: link GL, X11, Xi, Xcursor, dl, pthread
    $CC -O2 -std=c99 \
        -I src \
        -I third_party \
        src/sr_main.c \
        src/sr_raster.c \
        src/sr_texture.c \
        src/sr_gif.c \
        -o build/starraster \
        -lGL -lX11 -lXi -lXcursor -ldl -lpthread -lm
fi

if [ $? -ne 0 ]; then
    echo "[FAIL] Build failed."
    exit 1
fi

echo "[OK] Build succeeded."
echo "[RUN] Starting StarRaster..."
echo

cd "$(dirname "$0")"
./build/starraster
