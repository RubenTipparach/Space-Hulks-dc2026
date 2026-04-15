@echo off
setlocal

rem Regenerate indexed textures (only if source PNG is newer than .idx)
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update-textures.ps1"

echo [BUILD] Drake's Void (native)...

if not exist "build" mkdir "build"

windres resources.rc -o build/resources.o

gcc -O2 -std=c99 ^
    -I src ^
    -I third_party ^
    src/sr_main.c ^
    src/sr_raster.c ^
    src/sr_texture.c ^
    src/sr_gif.c ^
    build/resources.o ^
    -o build/drakesvoid.exe ^
    -lopengl32 -lgdi32 -luser32 -lkernel32 -lshell32 -lole32 -lwinmm -lm

if %errorlevel% neq 0 (
    echo [FAIL] Build failed.
    pause
    exit /b 1
)

echo [OK] Build succeeded.

echo [COPY] Copying assets to build folder...
robocopy "%~dp0assets" "%~dp0build\assets" /MIR /NJH /NJS /NDL /NC /NS >nul 2>&1
robocopy "%~dp0config" "%~dp0build\config" /MIR /NJH /NJS /NDL /NC /NS >nul 2>&1
robocopy "%~dp0levels" "%~dp0build\levels" /MIR /NJH /NJS /NDL /NC /NS >nul 2>&1
echo [OK] Assets copied.

echo [RUN] Starting Drake's Void...
echo.

cd /d "%~dp0"
build\drakesvoid.exe
