#!/usr/bin/env bash
set -euo pipefail

echo "============================================"
echo "  StarRaster - Install Dependencies"
echo "============================================"
echo

# Check for gcc/clang
if ! command -v gcc &>/dev/null && ! command -v clang &>/dev/null; then
    echo "[!] No C compiler found."
    if [[ "$(uname)" == "Darwin" ]]; then
        echo "    Install Xcode command-line tools: xcode-select --install"
    else
        echo "    Install gcc: sudo apt install build-essential  (Debian/Ubuntu)"
        echo "                 sudo dnf install gcc              (Fedora)"
    fi
    exit 1
fi
echo "[OK] C compiler found"

# Create directories
mkdir -p third_party/sokol third_party/stb build

# Download sokol headers
echo
echo "Downloading sokol headers..."
curl -sL -o third_party/sokol/sokol_app.h   https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h
curl -sL -o third_party/sokol/sokol_gfx.h   https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h
curl -sL -o third_party/sokol/sokol_glue.h   https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h
curl -sL -o third_party/sokol/sokol_log.h    https://raw.githubusercontent.com/floooh/sokol/master/sokol_log.h
curl -sL -o third_party/sokol/sokol_audio.h  https://raw.githubusercontent.com/floooh/sokol/master/sokol_audio.h

# Verify downloads
for f in sokol_app.h sokol_gfx.h sokol_glue.h sokol_log.h sokol_audio.h; do
    if [ ! -f "third_party/sokol/$f" ]; then
        echo "[FAIL] Failed to download $f"
        exit 1
    fi
done
echo "[OK] sokol headers downloaded"

# Download stb headers
echo
echo "Downloading stb headers..."
curl -sL -o third_party/stb/stb_image.h       https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
curl -sL -o third_party/stb/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h

for f in stb_image.h stb_image_write.h; do
    if [ ! -f "third_party/stb/$f" ]; then
        echo "[FAIL] Failed to download $f"
        exit 1
    fi
done
echo "[OK] stb headers downloaded"

# Download minimp3
echo
echo "Downloading minimp3..."
mkdir -p third_party/minimp3
curl -sL -o third_party/minimp3/minimp3.h     https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h
curl -sL -o third_party/minimp3/minimp3_ex.h  https://raw.githubusercontent.com/lieff/minimp3/master/minimp3_ex.h

for f in minimp3.h minimp3_ex.h; do
    if [ ! -f "third_party/minimp3/$f" ]; then
        echo "[FAIL] Failed to download $f"
        exit 1
    fi
done
echo "[OK] minimp3 downloaded"

# Check for ALSA dev headers (Linux audio)
if [[ "$(uname)" == "Linux" ]] && ! pkg-config --exists alsa 2>/dev/null; then
    echo
    echo "[WARN] libasound2-dev not found. Audio may not compile."
    echo "       Install: sudo apt install libasound2-dev"
fi

echo
echo "============================================"
echo "  All dependencies installed successfully!"
echo "  Run ./build-run.sh to build and run."
echo "============================================"
