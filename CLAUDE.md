# Claude Code Notes

## Build & Validation

- Do NOT try to install system dependencies (GL, X11, mesa, etc.) or run full native builds locally. The environment doesn't have them and they aren't needed.
- Full compilation and validation is handled by **GitHub Actions CI**. Trust it.
- For local syntax checking, use the **mock-header approach**: create a minimal C file with stubbed sokol/framework types and `#include` the project headers. This is fast and sufficient.
- The web build uses emscripten and doesn't need GL/X11 headers at all.

## Project Structure

- Single translation unit (TU) — all headers are `#include`d into `src/sr_main.c`
- Header-only modules: `sr_combat.h`, `sr_dungeon.h`, `sr_ship.h`, `sr_scene_dungeon.h`, etc.
- Third-party deps (sokol, stb) are downloaded by `install.sh` into `third_party/`
- All rendering is software-rasterized into a framebuffer (`sr_framebuffer`), then uploaded to GPU via sokol

## Key Systems

- **Combat**: Card-based, turn-based in `sr_combat.h`. Elemental effects (ice/acid/fire/lightning).
- **Ship**: FTL-style ship boarding in `sr_ship.h`. Rooms have types, officers have ranks/names, missions tracked.
- **Dungeon**: Grid-based generation in `sr_dungeon.h`, rendering in `sr_scene_dungeon.h`. Each ship deck = one dungeon floor.
