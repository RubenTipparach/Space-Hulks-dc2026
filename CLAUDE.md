# Claude Code Notes

## Style Rules

- **Never use em-dash (`—`, U+2014) for anything.** Not in source code, comments,
  string literals, documentation, commit messages, or replies. Use a regular
  hyphen (`-`) or restructure the sentence. Also avoid en-dash (`–`) unless it
  is explicitly required by a numeric range.

## Build & Validation

- This is a **Windows development environment**. You can run `build-run.bat` to compile and test the game locally (with user permission).
- Full compilation and validation is also handled by **GitHub Actions CI**.
- The web build uses emscripten and is built via `build-web.sh` or CI.

## Project Structure

- Single translation unit (TU) - all headers are `#include`d into `src/sr_main.c`
- Header-only modules: `sr_combat.h`, `sr_dungeon.h`, `sr_ship.h`, `sr_scene_dungeon.h`, etc.
- Third-party deps (sokol, stb) are downloaded by `install.sh` into `third_party/`
- All rendering is software-rasterized into a framebuffer (`sr_framebuffer`), then uploaded to GPU via sokol

## Key Systems

- **Combat**: Card-based, turn-based in `sr_combat.h`. Elemental effects (ice/acid/fire/lightning). Movement points persist between rounds.
- **Ship**: FTL-style ship boarding in `sr_ship.h`. Rooms have types (unique per ship), officers have ranks/names, missions tracked.
- **Dungeon**: Grid-based generation in `sr_dungeon.h`, rendering in `sr_scene_dungeon.h`. Each ship deck = one dungeon floor. Grid sizes: 20x20 (small), 40x40 (medium), 80x80 (large).

## Game Flow

- New game starts with player ship under attack. Captain tells you to board the enemy ship.
- Player uses teleporter room on hub ship to beam over to enemy ship.
- After mission, player returns to hub, uses star map to jump to next sector (with confirm dialog).

## Teleport Back to Hub Ship

The player returns to their ship when:
- **All enemies killed** - enemy ship neutralized, mission complete
- **Enemy ship destroyed** - hull HP reduced to 0 via subsystem damage (console sabotage)
- **Player finds teleporter room** on the enemy ship and activates it (can board the same ship again later, partial scrap reward)
- **Player dies** in combat - emergency extraction, HP set to 25%
- **Player's ship destroyed** - emergency jump, HP set to 25%

The player does NOT teleport back after winning a normal combat encounter. They stay on the enemy ship to keep exploring. Mission completion only triggers via console sabotage (interacting with room consoles), not random enemy kills.

## Ship Room Types

Every ship has unique rooms (no duplicates). Mandatory: Bridge, Engines, Weapons. Optional (shuffled): Shields, Reactor, Medbay, Cargo, Barracks, Teleporter. Not every ship has a teleporter.

## Hub Ship

- Interactions unified to F key: F -> Dialog -> Action
- All UI is mouse/touch friendly with hover highlights and clickable buttons
- Captain dialog is dynamic: "under attack" when mission available, "ready to jump" when neutralized
- Hub uses `wall_A` texture, no pillars, separate config (`config/hub.yaml`)
- Corridors are 1 tile wide (both hub and enemy ships)
- Rooms separated from corridor by 1-tile wall gap with doorway
