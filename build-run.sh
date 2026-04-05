#!/usr/bin/env bash
set -euo pipefail

echo "[BUILD] Drake's Void (native)..."

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
        -o build/drakesvoid \
        -framework Cocoa -framework OpenGL -framework QuartzCore -framework AudioToolbox \
        -lm
    rm -f sr_main.o sr_raster.o sr_texture.o sr_gif.o
elif [[ "$OS" == MINGW* ]] || [[ "$OS" == MSYS* ]] || [[ "$OS" == CYGWIN* ]]; then
    # Windows (MSYS2/MinGW)
    $CC -O2 -std=c99 \
        -I src \
        -I third_party \
        src/sr_main.c \
        src/sr_raster.c \
        src/sr_texture.c \
        src/sr_gif.c \
        -o build/drakesvoid.exe \
        -lgdi32 -luser32 -lshell32 -lole32 -lwinmm -lopengl32 -lm
else
    # Linux: link GL, X11, Xi, Xcursor, dl, pthread
    $CC -O2 -std=c99 \
        -I src \
        -I third_party \
        src/sr_main.c \
        src/sr_raster.c \
        src/sr_texture.c \
        src/sr_gif.c \
        -o build/drakesvoid \
        -lGL -lX11 -lXi -lXcursor -ldl -lpthread -lasound -lm
fi

if [ $? -ne 0 ]; then
    echo "[FAIL] Build failed."
    exit 1
fi

echo "[OK] Build succeeded."
echo "[RUN] Starting Drake's Void..."
echo

cd "$(dirname "$0")"
if [[ -f ./build/drakesvoid.exe ]]; then
    ./build/drakesvoid.exe
else
    ./build/drakesvoid
fi
