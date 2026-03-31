/*  Space Hulks — Dungeon Crawler Engine
 *  Entry point: Sokol app with dungeon scene.
 */

#if defined(__EMSCRIPTEN__)
    #define SOKOL_GLES3
    #include <emscripten.h>
#else
    #define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#include "../third_party/sokol/sokol_app.h"
#include "../third_party/sokol/sokol_gfx.h"
#include "../third_party/sokol/sokol_glue.h"
#include "../third_party/sokol/sokol_log.h"

#include "sr_math.h"
#include "sr_raster.h"
#include "sr_texture.h"
#include "sr_gif.h"
#include "sr_font.h"
#include "sr_dungeon.h"

#ifdef _WIN32
    #include <direct.h>
    #pragma comment(lib, "winmm.lib")
    #include <mmsystem.h>
#else
    #include <sys/stat.h>
    #ifndef __EMSCRIPTEN__
        #include <unistd.h>
    #endif
#endif

/* ── Module includes (order matters — header-only, single TU) ────── */

#include "sr_app.h"
#include "sr_lighting.h"
#include "sr_sprites.h"
#include "sr_scene_dungeon.h"
#include "sr_combat.h"

/* ── UI mouse state (framebuffer coords) ─────────────────────────── */
static float ui_mouse_x = -1, ui_mouse_y = -1;
static bool  ui_mouse_clicked = false;
static float ui_click_x = -1, ui_click_y = -1;

static bool ui_button(uint32_t *px, int W, int H, int bx, int by, int bw, int bh,
                      const char *label, uint32_t base_col, uint32_t hover_col, uint32_t click_col) {
    bool hovered = (ui_mouse_x >= bx && ui_mouse_x < bx + bw &&
                    ui_mouse_y >= by && ui_mouse_y < by + bh);
    bool clicked = hovered && ui_mouse_clicked &&
                   (ui_click_x >= bx && ui_click_x < bx + bw &&
                    ui_click_y >= by && ui_click_y < by + bh);

    uint32_t bg = clicked ? click_col : (hovered ? hover_col : base_col);

    for (int ry = by; ry < by + bh && ry < H; ry++)
        for (int rx = bx; rx < bx + bw && rx < W; rx++)
            if (rx >= 0 && ry >= 0) px[ry * W + rx] = bg;

    /* Border = brightened version of current bg color */
    uint32_t br = (bg >> 0) & 0xFF, bgr = (bg >> 8) & 0xFF, bb = (bg >> 16) & 0xFF;
    int bo = hovered ? 100 : 60; /* brightness offset */
    uint32_t border = 0xFF000000 |
        (((bb + bo > 255 ? 255 : bb + bo)) << 16) |
        (((bgr + bo > 255 ? 255 : bgr + bo)) << 8) |
        ((br + bo > 255 ? 255 : br + bo));
    for (int rx = bx; rx < bx + bw && rx < W; rx++) {
        if (by >= 0 && by < H) px[by * W + rx] = border;
        if (by+bh-1 >= 0 && by+bh-1 < H) px[(by+bh-1) * W + rx] = border;
    }
    for (int ry = by; ry < by + bh && ry < H; ry++) {
        if (bx >= 0 && bx < W) px[ry * W + bx] = border;
        if (bx+bw-1 >= 0 && bx+bw-1 < W) px[ry * W + bx+bw-1] = border;
    }

    int llen = 0; for (const char *c = label; *c; c++) llen++;
    int tx = bx + (bw - llen * 6) / 2;
    int ty = by + (bh - 8) / 2;
    uint32_t tcol = hovered ? 0xFFFFFFFF : 0xFFCCCCCC;
    sr_draw_text_shadow(px, W, H, tx, ty, label, tcol, 0xFF000000);

    return clicked;
}

/* Helper: hoverable row — returns true if row was clicked */
static bool ui_row_hover(int bx, int by, int bw, int bh, bool *out_hovered) {
    bool hovered = (ui_mouse_x >= bx && ui_mouse_x < bx + bw &&
                    ui_mouse_y >= by && ui_mouse_y < by + bh);
    bool clicked = hovered && ui_mouse_clicked &&
                   (ui_click_x >= bx && ui_click_x < bx + bw &&
                    ui_click_y >= by && ui_click_y < by + bh);
    *out_hovered = hovered;
    return clicked;
}

#include "sr_ship.h"
#include "sr_scene_ship_hub.h"
#include "sr_menu.h"
static void handle_screen_tap(float sx, float sy); /* forward decl */
#include "sr_mobile_input.h"

static void game_init_ship(void); /* forward decl */

/* Ship grid size based on sector difficulty: small(20), medium(40), large(80) */
static void game_ship_size(int sector, int *out_w, int *out_h) {
    if (sector >= 6) { *out_w = 80; *out_h = 80; }
    else if (sector >= 3) { *out_w = 40; *out_h = 40; }
    else { *out_w = 20; *out_h = 20; }
}

/* ── Save / Load ─────────────────────────────────────────────────── */

typedef struct {
    uint32_t magic;
    int selected_class;
    int current_floor;
    int player_gx, player_gy, player_dir;
    /* persistent player */
    int hp, hp_max;
    int persistent_deck_count;
    int persistent_deck[COMBAT_DECK_MAX];
    /* dungeon RNG seed */
    uint32_t seed_base;
    /* combat state (if in combat) */
    int in_combat;
} save_data;

static void game_save(void) {
    save_data sd;
    memset(&sd, 0, sizeof(sd));
    sd.magic = SAVE_MAGIC;
    sd.selected_class = selected_class;
    sd.current_floor = dng_state.current_floor;
    sd.player_gx = dng_state.player.gx;
    sd.player_gy = dng_state.player.gy;
    sd.player_dir = dng_state.player.dir;
    sd.hp = g_player.hp;
    sd.hp_max = g_player.hp_max;
    sd.persistent_deck_count = g_player.persistent_deck_count;
    memcpy(sd.persistent_deck, g_player.persistent_deck,
           sizeof(int) * g_player.persistent_deck_count);
    sd.seed_base = dng_state.seed_base;
    sd.in_combat = (app_state == STATE_COMBAT) ? 1 : 0;

    FILE *f = fopen(SAVE_FILE, "wb");
    if (f) {
        fwrite(&sd, sizeof(sd), 1, f);
        fclose(f);
    }
}

static bool game_load(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return false;
    save_data sd;
    if (fread(&sd, sizeof(sd), 1, f) != 1 || sd.magic != SAVE_MAGIC) {
        fclose(f); return false;
    }
    fclose(f);

    selected_class = sd.selected_class;
    player_persist_init(selected_class);
    g_player.hp = sd.hp;
    g_player.hp_max = sd.hp_max;
    g_player.persistent_deck_count = sd.persistent_deck_count;
    memcpy(g_player.persistent_deck, sd.persistent_deck,
           sizeof(int) * sd.persistent_deck_count);

    /* Regenerate dungeon with same seeds */
    memset(&dng_state, 0, sizeof(dng_state));
    dng_state.seed_base = sd.seed_base;
    game_ship_size(player_sector, &dng_state.grid_w, &dng_state.grid_h);
    for (int fl = 0; fl <= sd.current_floor; fl++) {
        bool is_last = (fl >= current_ship.num_decks - 1);
        dng_generate(&dng_state.floors[fl], dng_state.grid_w, dng_state.grid_h,
                     fl > 0, !is_last,
                     dng_state.seed_base + (uint32_t)fl * 777, fl);
        dng_state.floor_generated[fl] = true;
    }
    dng_state.current_floor = sd.current_floor;
    dng_state.dungeon = &dng_state.floors[sd.current_floor];
    dng_player_init(&dng_state.player, sd.player_gx, sd.player_gy, sd.player_dir);
    dng_initialized = true;
    game_init_ship();

    return true;
}

static bool game_has_save(void) {
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    bool ok = (fread(&magic, 4, 1, f) == 1 && magic == SAVE_MAGIC);
    fclose(f);
    return ok;
}

/* ── Title screen ────────────────────────────────────────────────── */

static void draw_title_screen(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;
    uint32_t yellow = 0xFF00DDDD;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0D0D11;

    sr_draw_text_shadow(px, W, H, W/2 - 45, 60, "SPACE HULKS", white, shadow);
    sr_draw_text_shadow(px, W, H, W/2 - 50, 80, "DUNGEON CRAWLER", gray, shadow);

    /* New Game button */
    {
        int bx = W/2 - 50, by = 120, bw = 100, bh = 22;
        bool sel = (title_cursor == 0);
        combat_draw_rect(px, W, H, bx, by, bw, bh, sel ? 0xFF222244 : 0xFF111122);
        combat_draw_rect_outline(px, W, H, bx, by, bw, bh, sel ? yellow : gray);
        sr_draw_text_shadow(px, W, H, bx + 22, by + 7, "NEW GAME", sel ? yellow : white, shadow);
    }

    /* Continue button */
    {
        int bx = W/2 - 50, by = 150, bw = 100, bh = 22;
        bool sel = (title_cursor == 1);
        uint32_t col = save_exists ? (sel ? yellow : white) : 0xFF444444;
        combat_draw_rect(px, W, H, bx, by, bw, bh, sel ? 0xFF222244 : 0xFF111122);
        combat_draw_rect_outline(px, W, H, bx, by, bw, bh, sel ? yellow : gray);
        sr_draw_text_shadow(px, W, H, bx + 22, by + 7, "CONTINUE", col, shadow);
        if (!save_exists)
            sr_draw_text_shadow(px, W, H, bx + 12, by + bh + 4, "NO SAVE FOUND", 0xFF444444, shadow);
    }
}

/* ── Ship-mode game initialization ──────────────────────────────── */

static int last_player_gx = -1, last_player_gy = -1;
static int current_combat_room = -1; /* ship room index of current combat */
static bool console_combat = false;  /* true if combat was triggered by console sabotage */
static char dng_hud_msg[64];         /* temporary HUD message for dungeon scene */
static int  dng_hud_msg_timer = 0;   /* frames remaining to show message */

static void game_init_ship(void) {
    /* Generate ship based on current floor as difficulty */
    int difficulty = dng_state.current_floor;
    uint32_t ship_seed = dng_state.seed_base + 9999;
    ship_generate(&current_ship, difficulty, ship_seed);
    dng_state.max_floors = current_ship.num_decks;

    /* Regenerate all existing floors with correct stair flags for num_decks */
    for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
        if (!dng_state.floor_generated[fl]) continue;
        bool is_last = (fl >= current_ship.num_decks - 1);
        dng_generate(&dng_state.floors[fl], dng_state.grid_w, dng_state.grid_h,
                     fl > 0, !is_last,
                     dng_state.seed_base + (uint32_t)fl * 777, fl);
    }
    dng_state.dungeon = &dng_state.floors[dng_state.current_floor];

    /* For each deck, populate the dungeon floor with ship room data */
    for (int deck = 0; deck < current_ship.num_decks && deck < DNG_MAX_FLOORS; deck++) {
        if (!dng_state.floor_generated[deck]) continue;
        sr_dungeon *dd = &dng_state.floors[deck];

        /* Clear existing entities (will be re-placed below) */
        memset(dd->aliens, 0, sizeof(dd->aliens));
        memset(dd->alien_names, 0, sizeof(dd->alien_names));
        memset(dd->consoles, 0, sizeof(dd->consoles));

        /* Build dng_room array from stored room info */
        dng_room rooms[DNG_MAX_ROOMS];
        for (int r = 0; r < dd->room_count && r < DNG_MAX_ROOMS; r++) {
            rooms[r].x = dd->room_x[r];
            rooms[r].y = dd->room_y[r];
            rooms[r].w = dd->room_w[r];
            rooms[r].h = dd->room_h[r];
            rooms[r].cx = dd->room_cx[r];
            rooms[r].cy = dd->room_cy[r];
        }

        /* Map ship rooms to dungeon rooms */
        int start = current_ship.deck_room_start[deck];
        int count = current_ship.deck_room_count[deck];
        for (int r = 0; r < count && r < dd->room_count; r++) {
            dd->room_ship_idx[r] = start + r;
        }

        /* Place consoles FIRST at room centers */
        for (int r = 0; r < count && r < dd->room_count; r++) {
            ship_room *rm = &current_ship.rooms[start + r];
            if (rm->type == ROOM_CORRIDOR) continue;
            if (rm->subsystem_hp_max > 0 && rm->subsystem_hp <= 0) continue;
            if (rm->type == ROOM_CARGO && rm->cleared) continue;
            int cx = dd->room_cx[r], cy = dd->room_cy[r];
            if (cx >= 1 && cx <= dd->w && cy >= 1 && cy <= dd->h &&
                dd->map[cy][cx] == 0) {
                dd->consoles[cy][cx] = (uint8_t)rm->type;
            }
        }

        /* Then populate aliens (officers + enemies) — they avoid consoles */
        ship_populate_deck(&current_ship, dd, deck, dd->room_count, rooms);
    }
}

/* ── Handle combat end (shared by tap and keyboard) ────────────── */

static void handle_combat_end(void) {
    if (current_ship.initialized) {
        if (combat.player_won && current_combat_room >= 0) {
            current_ship.rooms[current_combat_room].cleared = true;

            /* Console sabotage deals heavy subsystem damage */
            if (console_combat) {
                int sub_dmg = 10;
                ship_damage_subsystem(&current_ship, current_combat_room, sub_dmg);

                for (int o = 0; o < current_ship.officer_count; o++) {
                    if (current_ship.officers[o].room_idx == current_combat_room &&
                        current_ship.officers[o].alive) {
                        current_ship.officers[o].alive = false;
                        current_ship.officers[o].captured = true;
                    }
                }
                /* Only check mission completion on console sabotage,
                 * not on regular enemy kills */
                ship_check_missions(&current_ship);
            }
        }
        console_combat = false;

        if (!combat.player_won) {
            /* Player died — emergency teleport back to hub */
            g_player.hp = g_player.hp_max / 4;
            if (g_player.hp < 1) g_player.hp = 1;
            current_ship.boarding_active = false;
            hub_generate(&g_hub);
            app_state = STATE_SHIP_HUB;
            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "EMERGENCY EXTRACTION!");
            g_hub.hud_msg_timer = 120;
            g_hub.mission_available = false;
            current_combat_room = -1;
            return;
        }

        if (current_ship.player_ship_destroyed) {
            g_player.hp = g_player.hp_max / 4;
            if (g_player.hp < 1) g_player.hp = 1;
            current_ship.boarding_active = false;
            hub_generate(&g_hub);
            app_state = STATE_SHIP_HUB;
            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "SHIP DAMAGED! EMERGENCY JUMP!");
            g_hub.hud_msg_timer = 120;
            g_hub.mission_available = false;
            current_combat_room = -1;
            return;
        }

        if (current_ship.enemy_ship_destroyed) {
            dng_state.current_floor = 0;
            dng_state.dungeon = &dng_state.floors[0];
            dng_player_init(&dng_state.player,
                            dng_state.dungeon->spawn_gx,
                            dng_state.dungeon->spawn_gy, 0);
            last_player_gx = dng_state.player.gx;
            last_player_gy = dng_state.player.gy;
            current_ship.boarding_active = false;
            /* Mission complete — reward scrap and return to hub */
            int reward = 20 + player_sector * 10;
            player_scrap += reward;
            hub_generate(&g_hub);
            app_state = STATE_SHIP_HUB;
            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "MISSION COMPLETE! +%d SCRAP", reward);
            g_hub.hud_msg_timer = 120;
            g_hub.mission_available = false;
            current_combat_room = -1;
            return;
        }

        /* Check if all enemies are dead across all floors */
        if (combat.player_won && current_ship.boarding_active) {
            bool any_alive = false;
            for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
                if (!dng_state.floor_generated[fl]) continue;
                sr_dungeon *dd = &dng_state.floors[fl];
                for (int gy = 1; gy <= dd->h && !any_alive; gy++)
                    for (int gx = 1; gx <= dd->w && !any_alive; gx++)
                        if (dd->aliens[gy][gx] != 0) any_alive = true;
            }
            if (!any_alive) {
                current_ship.boarding_active = false;
                int reward = 25 + player_sector * 10;
                player_scrap += reward;
                hub_generate(&g_hub);
                app_state = STATE_SHIP_HUB;
                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "ALL HOSTILES ELIMINATED! +%d SCRAP", reward);
                g_hub.hud_msg_timer = 120;
                g_hub.mission_available = false;
                current_combat_room = -1;
                return;
            }
        }

        /* Check if primary mission is done (bridge captured via console) */
        if (current_ship.mission.completed && current_ship.boarding_active) {
            current_ship.boarding_active = false;
            int reward = 30 + player_sector * 10;
            for (int b = 0; b < current_ship.bonus_count; b++)
                if (current_ship.bonus_missions[b].completed) reward += 15;
            player_scrap += reward;
            hub_generate(&g_hub);
            app_state = STATE_SHIP_HUB;
            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "MISSION COMPLETE! +%d SCRAP", reward);
            g_hub.hud_msg_timer = 120;
            g_hub.mission_available = false;
            current_combat_room = -1;
            return;
        }

        current_combat_room = -1;
    }
    app_state = STATE_RUNNING;
    game_save();
}

/* ── Track player movement for random encounters ─────────────────── */

static void check_random_encounter(void) {
    dng_player *p = &dng_state.player;
    if (p->gx != last_player_gx || p->gy != last_player_gy) {
        int prev_gx = last_player_gx, prev_gy = last_player_gy;
        last_player_gx = p->gx;
        last_player_gy = p->gy;
        /* Auto-save on each step */
        game_save();
        uint8_t alien = dng_state.dungeon->aliens[p->gy][p->gx];
        if (alien != 0) {
            dng_state.dungeon->aliens[p->gy][p->gx] = 0;

            /* Track which ship room this combat is in */
            current_combat_room = -1;
            if (current_ship.initialized) {
                int local_room = dng_room_at(dng_state.dungeon, p->gx, p->gy);
                if (local_room >= 0 && local_room < dng_state.dungeon->room_count)
                    current_combat_room = dng_state.dungeon->room_ship_idx[local_room];

                /* Tick ship turn on each combat */
                ship_tick_turn(&current_ship);
            }

            combat_init(&combat, selected_class, dng_state.current_floor, alien);
            app_state = STATE_COMBAT;
            return;
        }

        /* Check for console interaction — sentinel defense combat */
        uint8_t con_type = dng_state.dungeon->consoles[p->gy][p->gx];
        if (con_type != 0 && current_ship.initialized && current_ship.boarding_active) {
            /* Block console access if enemies remain in this room — bounce back */
            int con_room = dng_room_at(dng_state.dungeon, p->gx, p->gy);
            if (con_room >= 0) {
                sr_dungeon *dd = dng_state.dungeon;
                int rx = dd->room_x[con_room], ry = dd->room_y[con_room];
                int rw = dd->room_w[con_room], rh = dd->room_h[con_room];
                bool has_enemies = false;
                for (int cy = ry; cy < ry + rh && !has_enemies; cy++)
                    for (int cx = rx; cx < rx + rw && !has_enemies; cx++)
                        if (dd->aliens[cy][cx] != 0) has_enemies = true;
                if (has_enemies) {
                    /* Remember the console tile for the bounce midpoint */
                    float mid_x = (p->gx - 0.5f) * DNG_CELL_SIZE;
                    float mid_z = (p->gy - 0.5f) * DNG_CELL_SIZE;
                    /* Snap grid position back to previous tile */
                    p->gx = prev_gx;
                    p->gy = prev_gy;
                    last_player_gx = prev_gx;
                    last_player_gy = prev_gy;
                    p->target_x = (p->gx - 0.5f) * DNG_CELL_SIZE;
                    p->target_z = (p->gy - 0.5f) * DNG_CELL_SIZE;
                    /* Start bounce animation */
                    p->bounce_mid_x = mid_x;
                    p->bounce_mid_z = mid_z;
                    p->bounce_timer = 12;
                    snprintf(dng_hud_msg, sizeof(dng_hud_msg), "CLEAR ENEMIES FIRST");
                    dng_hud_msg_timer = 90;
                    goto skip_console;
                }
            }
            /* Remove the console from the map */
            dng_state.dungeon->consoles[p->gy][p->gx] = 0;

            /* Teleporter console — escape back to hub */
            if (con_type == ROOM_TELEPORTER) {
                current_ship.boarding_active = false;
                int reward = 5 + player_sector * 3;
                player_scrap += reward;
                hub_generate(&g_hub);
                app_state = STATE_SHIP_HUB;
                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg),
                         "TELEPORTED OUT! +%d SCRAP", reward);
                g_hub.hud_msg_timer = 120;
                g_hub.mission_available = false;
                current_combat_room = -1;
                goto skip_console;
            }

            /* Find which ship room this console belongs to */
            int local_room = dng_room_at(dng_state.dungeon, p->gx, p->gy);
            if (local_room >= 0 && local_room < dng_state.dungeon->room_count) {
                int sr_idx = dng_state.dungeon->room_ship_idx[local_room];
                if (sr_idx >= 0 && sr_idx < current_ship.room_count) {
                    current_combat_room = sr_idx;
                    console_combat = true;
                    ship_room *rm = &current_ship.rooms[sr_idx];

                    /* Sentinel defense combat based on room type */
                    combat_init(&combat, selected_class, dng_state.current_floor, 0);

                    int num_drones = 1;
                    int drone_type = ENEMY_LURKER;
                    switch (rm->type) {
                        case ROOM_BRIDGE:  num_drones = 2; drone_type = ENEMY_HIVEGUARD; break;
                        case ROOM_REACTOR: num_drones = 2; drone_type = ENEMY_SPITTER; break;
                        case ROOM_WEAPONS: num_drones = 1; drone_type = ENEMY_BRUTE; break;
                        case ROOM_ENGINES: num_drones = 1; drone_type = ENEMY_SPITTER; break;
                        case ROOM_SHIELDS: num_drones = 1; drone_type = ENEMY_BRUTE; break;
                        case ROOM_CARGO:   num_drones = 1; drone_type = ENEMY_LURKER; break;
                        default: break;
                    }

                    combat.enemy_count = num_drones;
                    for (int i = 0; i < num_drones; i++) {
                        const enemy_template *tmpl = &enemy_templates[drone_type];
                        combat.enemies[i].type = drone_type;
                        combat.enemies[i].hp = tmpl->hp_max;
                        combat.enemies[i].hp_max = tmpl->hp_max;
                        combat.enemies[i].attack_range = tmpl->attack_range;
                        combat.enemies[i].damage = tmpl->damage;
                        combat.enemies[i].ranged = tmpl->ranged;
                        combat.enemies[i].flash_timer = 0;
                        combat.enemies[i].alive = true;
                    }

                    ship_tick_turn(&current_ship);
                    app_state = STATE_COMBAT;
                    game_save();
                }
            }
        }
        skip_console: (void)0;
    }
}

/* ── Class select screen ─────────────────────────────────────────── */

#define CLASS_COUNT 4
#define CLASS_BOX_W 100
#define CLASS_BOX_H 100

static void draw_class_box(uint32_t *px, int W, int H,
                           int bx, int by, bool sel,
                           const uint32_t *sprite, const char *name,
                           const char *line1, const char *line2,
                           const char *line3, const char *line4) {
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;
    uint32_t yellow = 0xFF00DDDD;
    uint32_t border = sel ? yellow : gray;
    combat_draw_rect_outline(px, W, H, bx, by, CLASS_BOX_W, CLASS_BOX_H, border);
    if (sel) combat_draw_rect_outline(px, W, H, bx+1, by+1, CLASS_BOX_W-2, CLASS_BOX_H-2, border);
    spr_draw(px, W, H, sprite, bx + 34, by + 4, 2);
    sr_draw_text_shadow(px, W, H, bx + 8, by + 40, name, sel ? yellow : white, shadow);
    sr_draw_text_shadow(px, W, H, bx + 8, by + 52, line1, gray, shadow);
    sr_draw_text_shadow(px, W, H, bx + 8, by + 64, line2, gray, shadow);
    sr_draw_text_shadow(px, W, H, bx + 8, by + 76, line3, gray, shadow);
    sr_draw_text_shadow(px, W, H, bx + 8, by + 88, line4, gray, shadow);
}

static void draw_class_select(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0D0D11;

    sr_draw_text_shadow(px, W, H, W/2 - 45, 8, "SPACE HULKS", white, shadow);
    sr_draw_text_shadow(px, W, H, W/2 - 55, 22, "SELECT YOUR CLASS", gray, shadow);

    /* 4 classes in a row */
    int gap = (W - CLASS_COUNT * CLASS_BOX_W) / (CLASS_COUNT + 1);
    int by = 40;

    /* Scout */
    {
        int bx = gap;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 0,
                       spr_scout, "SCOUT",
                       "HP: 18", "NIMBLE",
                       "SNIPER/SHOTGUN", "3 MOVE CARDS");
    }
    /* Marine */
    {
        int bx = gap * 2 + CLASS_BOX_W;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 1,
                       spr_marine, "MARINE",
                       "HP: 30", "TOUGH",
                       "3 SHOOT", "4 SHIELD");
    }
    /* Engineer */
    {
        int bx = gap * 3 + CLASS_BOX_W * 2;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 2,
                       spr_engineer, "ENGINEER",
                       "HP: 26", "MELEE FOCUS",
                       "WELDER/CHNSAW", "UP CLOSE");
    }
    /* Scientist */
    {
        int bx = gap * 4 + CLASS_BOX_W * 3;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 3,
                       spr_scientist, "SCIENTIST",
                       "HP: 22", "PRECISION",
                       "LASER/DEFLECT", "STUN GUN");
    }

    sr_draw_text_shadow(px, W, H, W/2 - 55, H - 14,
                        "TAP TO SELECT  TAP AGAIN=GO", gray, shadow);
}

/* ── Shaders ─────────────────────────────────────────────────────── */

#if defined(SOKOL_GLCORE)

static const char *vs_src =
    "#version 330\n"
    "uniform vec2 u_scale;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos * u_scale, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, v_uv);\n"
    "}\n";

#elif defined(SOKOL_GLES3)

static const char *vs_src =
    "#version 300 es\n"
    "uniform vec2 u_scale;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos * u_scale, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *fs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, v_uv);\n"
    "}\n";

#endif

/* ── Sokol GPU resources ─────────────────────────────────────────── */

static sg_image    fb_image;
static sg_view     fb_view;
static sg_sampler  fb_sampler;
static sg_pipeline pip;
static sg_bindings sr_bind;
static sg_buffer   vbuf;

/* ── Frame limiter ───────────────────────────────────────────────── */

#if defined(_WIN32)
static void frame_limiter(void) {
    static LARGE_INTEGER freq = {0}, last = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }
    double target = 1.0 / (double)TARGET_FPS;
    for (;;) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= target) { last = now; return; }
        if (target - elapsed > 0.002) Sleep(1);
    }
}
#elif !defined(__EMSCRIPTEN__)
static void frame_limiter(void) {
    static struct timespec last = {0};
    if (last.tv_sec == 0 && last.tv_nsec == 0) clock_gettime(CLOCK_MONOTONIC, &last);
    double target = 1.0 / (double)TARGET_FPS;
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) * 1e-9;
        if (elapsed >= target) { last = now; return; }
        usleep(500);
    }
}
#else
static void frame_limiter(void) { /* WASM: requestAnimationFrame handles this */ }
#endif

/* ── PNG screenshot ──────────────────────────────────────────────── */

#include "../third_party/stb/stb_image_write.h"

static void save_screenshot(const uint32_t *pixels, int w, int h) {
#ifdef _WIN32
    _mkdir("screenshots");
#else
    mkdir("screenshots", 0755);
#endif

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d_%d.png",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec, screenshot_counter++);

    uint8_t *rgb = (uint8_t *)malloc(w * h * 3);
    if (!rgb) { fprintf(stderr, "[Screenshot] Out of memory\n"); return; }

    for (int i = 0; i < w * h; i++) {
        uint32_t c = pixels[i];
        rgb[i * 3 + 0] = (uint8_t)((c      ) & 0xFF);
        rgb[i * 3 + 1] = (uint8_t)((c >>  8) & 0xFF);
        rgb[i * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
    }

    if (!stbi_write_png(filename, w, h, 3, rgb, w * 3))
        fprintf(stderr, "[Screenshot] Failed to save %s\n", filename);
    else
        printf("[Screenshot] Saved %s\n", filename);

    free(rgb);
}

/* ── Sokol callbacks ─────────────────────────────────────────────── */

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    fb = sr_framebuffer_create(FB_WIDTH, FB_HEIGHT);
    shadow_fb = sr_framebuffer_create(SHADOW_SIZE, SHADOW_SIZE);

    fb_image = sg_make_image(&(sg_image_desc){
        .width  = FB_WIDTH,
        .height = FB_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.stream_update = true,
    });

    fb_view = sg_make_view(&(sg_view_desc){
        .texture.image = fb_image,
    });

    sg_filter upscale_filter = SG_FILTER_NEAREST;
#if defined(__EMSCRIPTEN__)
    if (emscripten_run_script_int("window.matchMedia('(hover: none) and (pointer: coarse)').matches ? 1 : 0")) {
        upscale_filter = SG_FILTER_LINEAR;
    }
#endif
    fb_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = upscale_filter,
        .mag_filter = upscale_filter,
    });

    float verts[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
         1,  1,  1, 0,
        -1, -1,  0, 1,
         1,  1,  1, 0,
        -1,  1,  0, 0,
    };
    vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(verts),
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source   = vs_src,
        .fragment_func.source = fs_src,
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(float) * 2,
            .glsl_uniforms[0] = { .glsl_name = "u_scale", .type = SG_UNIFORMTYPE_FLOAT2 },
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .sampler_type = SG_SAMPLERTYPE_FILTERING,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .glsl_name = "tex",
            .view_slot = 0,
            .sampler_slot = 0,
        },
    });

    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            },
        },
    });

    sr_bind.vertex_buffers[0] = vbuf;
    sr_bind.views[0]     = fb_view;
    sr_bind.samplers[0]  = fb_sampler;

    textures[TEX_BRICK] = sr_texture_load("assets/bricks.png");
    textures[TEX_GRASS] = sr_texture_load("assets/grass.png");
    textures[TEX_ROOF]  = sr_texture_load("assets/roof.png");
    textures[TEX_WOOD]  = sr_texture_load("assets/wood.png");
    textures[TEX_TILE]  = sr_texture_load("assets/tile.png");

    itextures[ITEX_BRICK] = sr_indexed_load("assets/indexed/bricks.idx");
    itextures[ITEX_GRASS] = sr_indexed_load("assets/indexed/grass.idx");
    itextures[ITEX_ROOF]  = sr_indexed_load("assets/indexed/roof.idx");
    itextures[ITEX_WOOD]  = sr_indexed_load("assets/indexed/wood.idx");
    itextures[ITEX_TILE]  = sr_indexed_load("assets/indexed/tile.idx");
    itextures[ITEX_STONE]  = sr_indexed_load("assets/indexed/stone.idx");
    itextures[ITEX_WALL_A] = sr_indexed_load("assets/indexed/wall_a.idx");

    stextures[STEX_LURKER]    = sr_texture_load("assets/sprites/lurker.png");
    stextures[STEX_BRUTE]     = sr_texture_load("assets/sprites/brute.png");
    stextures[STEX_SPITTER]   = sr_texture_load("assets/sprites/spitter.png");
    stextures[STEX_HIVEGUARD] = sr_texture_load("assets/sprites/hiveguard.png");
    stextures[STEX_SCOUT]     = sr_texture_load("assets/sprites/scout.png");
    stextures[STEX_MARINE]    = sr_texture_load("assets/sprites/marine.png");
    stextures[STEX_CREW_CAPTAIN]       = sr_texture_load("assets/sprites/crew_captain.png");
    stextures[STEX_CREW_SERGEANT]      = sr_texture_load("assets/sprites/crew_sergeant.png");
    stextures[STEX_CREW_QUARTERMASTER] = sr_texture_load("assets/sprites/crew_quartermaster.png");
    stextures[STEX_CREW_PRIVATE]       = sr_texture_load("assets/sprites/crew_private.png");
    stextures[STEX_CREW_DOCTOR]        = sr_texture_load("assets/sprites/crew_doctor.png");

    /* Build console textures from embedded sprite data for 3D billboard rendering */
    for (int rt = 1; rt < CONSOLE_TEX_COUNT && rt < ROOM_TYPE_COUNT; rt++) {
        const uint32_t *src = spr_console_table[rt];
        if (!src) continue;
        console_textures[rt].width = 16;
        console_textures[rt].height = 16;
        console_textures[rt].pixels = (uint32_t *)malloc(16 * 16 * sizeof(uint32_t));
        if (console_textures[rt].pixels)
            memcpy(console_textures[rt].pixels, src, 16 * 16 * sizeof(uint32_t));
    }

    sr_fog_set(FOG_COLOR, FOG_NEAR, FOG_FAR);

    dng_load_config();
    hub_load_config();

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    save_exists = game_has_save();
    printf("Space Hulks initialized (%dx%d @ %dfps)\n", FB_WIDTH, FB_HEIGHT, TARGET_FPS);
}

static void frame(void) {
    double dt = sapp_frame_duration();
    time_acc += dt;
    frame_counter++;

    fps_timer += dt;
    fps_frame_count++;
    if (fps_timer >= 1.0) {
        fps_display = fps_frame_count;
        fps_frame_count = 0;
        fps_timer -= 1.0;
    }

    /* ── CPU rasterize ───────────────────────────────────────── */
    sr_stats_reset();
    sr_framebuffer_clear(&fb, 0xFF000000, 1.0f);

    if (app_state == STATE_TITLE) {
        draw_title_screen(&fb);
    } else if (app_state == STATE_CLASS_SELECT) {
        draw_class_select(&fb);
    } else if (app_state == STATE_COMBAT) {
        combat_update(&combat);
        draw_combat_scene(&fb);
    } else if (app_state == STATE_SHIP_HUB) {
        dng_player_update(&g_hub.player);
        hub_draw_scene(&fb);
        hub_draw_hud(fb.color, fb.width, fb.height);
        hub_draw_minimap(&fb);
        if (dng_expanded_map) {
            /* Temporarily point dng_state at hub dungeon for expanded map */
            sr_dungeon *save_d = dng_state.dungeon;
            dng_player save_p = dng_state.player;
            dng_state.dungeon = &g_hub.dungeon;
            dng_state.player = g_hub.player;
            draw_expanded_map(&fb);
            dng_state.dungeon = save_d;
            dng_state.player = save_p;
        }
        if (deck_view_active)
            draw_deck_viewer(fb.color, fb.width, fb.height);
        if (g_dialog.active)
            draw_dialog(fb.color, fb.width, fb.height);
    } else if (app_state == STATE_SHOP) {
        draw_shop(fb.color, fb.width, fb.height);
    } else if (app_state == STATE_STARMAP) {
        draw_starmap(fb.color, fb.width, fb.height);
    } else {
        sr_mat4 vp;
        (void)vp;
        sr_fog_disable();

        /* Update dungeon game state */
        if (dng_play_state == DNG_STATE_CLIMBING) {
            if (dng_update_climb(&dng_state)) {
                dng_play_state = DNG_STATE_PLAYING;
            }
        } else {
            sr_dungeon *dd = dng_state.dungeon;
            dng_player *pp = &dng_state.player;
            bool on_up = (dd->has_up && pp->gx == dd->stairs_gx && pp->gy == dd->stairs_gy);
            bool on_down = (dd->has_down && pp->gx == dd->down_gx && pp->gy == dd->down_gy);
            if (dng_state.on_stairs) {
                if (!on_up && !on_down) dng_state.on_stairs = false;
            } else {
                if (on_up) {
                    dng_start_climb(&dng_state, true);
                    dng_play_state = DNG_STATE_CLIMBING;
                } else if (on_down) {
                    dng_start_climb(&dng_state, false);
                    dng_play_state = DNG_STATE_CLIMBING;
                }
            }
        }
        dng_player_update(&dng_state.player);

        /* Check for random encounter after moving */
        if (dng_play_state == DNG_STATE_PLAYING)
            check_random_encounter();

        draw_dungeon_scene(&fb, &vp);
        draw_dungeon_minimap(&fb);

        /* Ship HUD overlay */
        if (current_ship.initialized && current_ship.boarding_active) {
            draw_ship_hud(fb.color, fb.width, fb.height, &current_ship);

            /* Deck button (top-right, above minimap) */
            {
                char deckbuf[16];
                snprintf(deckbuf, sizeof(deckbuf), "DECK %d", g_player.persistent_deck_count);
                if (ui_button(fb.color, fb.width, fb.height, fb.width - 66, 14, 62, 12, deckbuf,
                              0xFF1A1A2A, 0xFF222244, 0xFF333366))
                    deck_view_active = true;
            }

            /* Recolor minimap cells by ship room type */
            {
                sr_dungeon *md = dng_state.dungeon;
                int mscale = 2;
                int mmx = fb.width - md->w * mscale - 4;
                int mmy = 28;
                for (int my = 1; my <= md->h; my++) {
                    for (int mx = 1; mx <= md->w; mx++) {
                        if (md->map[my][mx] == 1) continue;
                        int ri = dng_room_at(md, mx, my);
                        if (ri < 0 || ri >= md->room_count) continue;
                        int sr = md->room_ship_idx[ri];
                        if (sr < 0 || sr >= current_ship.room_count) continue;
                        uint32_t rc = room_type_colors[current_ship.rooms[sr].type];
                        int rr = ((rc >> 0) & 0xFF) / 3;
                        int rg = ((rc >> 8) & 0xFF) / 3;
                        int rb = ((rc >> 16) & 0xFF) / 3;
                        uint32_t cell_col = 0xFF000000 | (rb << 16) | (rg << 8) | rr;
                        int px0 = mmx + (mx - 1) * mscale;
                        int py0 = mmy + (my - 1) * mscale;
                        for (int dy = 0; dy < mscale; dy++)
                            for (int dx = 0; dx < mscale; dx++) {
                                int rx = px0 + dx, ry = py0 + dy;
                                if (rx >= 0 && rx < fb.width && ry >= 0 && ry < fb.height)
                                    fb.color[ry * fb.width + rx] = cell_col;
                            }
                    }
                }

                /* Draw console icons at room centers on minimap */
                for (int ri = 0; ri < md->room_count; ri++) {
                    int si = md->room_ship_idx[ri];
                    if (si < 0 || si >= current_ship.room_count) continue;
                    int rtype = current_ship.rooms[si].type;
                    if (rtype <= 0 || rtype >= ROOM_TYPE_COUNT) continue;
                    const uint32_t *cspr = spr_console_table[rtype];
                    if (!cspr) continue;
                    int rcx = mmx + (md->room_cx[ri] - 1) * mscale;
                    int rcy = mmy + (md->room_cy[ri] - 1) * mscale;
                    /* Draw 4x4 sample from sprite screen center (pixels 6-9, rows 6-9) */
                    for (int sy = 0; sy < 4; sy++)
                        for (int sx = 0; sx < 4; sx++) {
                            uint32_t c = cspr[(6 + sy) * 16 + (6 + sx)];
                            if ((c & 0xFF000000) == 0) continue;
                            int cpx = rcx - 2 + sx, cpy = rcy - 2 + sy;
                            if (cpx >= 0 && cpx < fb.width && cpy >= 0 && cpy < fb.height)
                                fb.color[cpy * fb.width + cpx] = c;
                        }
                }
            }

            /* Redraw player dot + FOV cone on top of recolored cells */
            draw_minimap_player(&fb);

            /* Console label — show subsystem name when facing a console */
            {
                dng_player *rp = &dng_state.player;
                int look_gx = rp->gx + dng_dir_dx[rp->dir];
                int look_gy = rp->gy + dng_dir_dz[rp->dir];
                /* Check the cell player is facing, and also current cell */
                uint8_t con = 0;
                if (look_gx >= 1 && look_gx <= dng_state.dungeon->w &&
                    look_gy >= 1 && look_gy <= dng_state.dungeon->h)
                    con = dng_state.dungeon->consoles[look_gy][look_gx];
                if (con == 0)
                    con = dng_state.dungeon->consoles[rp->gy][rp->gx];
                if (con > 0 && con < ROOM_TYPE_COUNT) {
                    uint32_t col = room_type_colors[con];
                    const char *name = room_type_names[con];
                    /* Find subsystem HP for this console type */
                    int sys_hp = -1, sys_max = -1;
                    for (int ri = 0; ri < current_ship.room_count; ri++) {
                        if (current_ship.rooms[ri].type == (int)con) {
                            sys_hp = current_ship.rooms[ri].subsystem_hp;
                            sys_max = current_ship.rooms[ri].subsystem_hp_max;
                            break;
                        }
                    }
                    char cbuf[48];
                    if (sys_max > 0)
                        snprintf(cbuf, sizeof(cbuf), "%s  SYS %d/%d", name, sys_hp, sys_max);
                    else
                        snprintf(cbuf, sizeof(cbuf), "%s", name);
                    int tw = 0; for (const char *c = cbuf; *c; c++) tw++;
                    int tx = fb.width / 2 - tw * 3;
                    sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                        tx, fb.height - 14, cbuf, col, 0xFF000000);
                }

                /* Show alien name when facing one */
                if (look_gx >= 1 && look_gx <= dng_state.dungeon->w &&
                    look_gy >= 1 && look_gy <= dng_state.dungeon->h) {
                    uint8_t alien = dng_state.dungeon->aliens[look_gy][look_gx];
                    if (alien > 0 && dng_state.dungeon->alien_names[look_gy][look_gx][0]) {
                        const char *aname = dng_state.dungeon->alien_names[look_gy][look_gx];
                        int alen = 0; for (const char *c = aname; *c; c++) alen++;
                        int ax = fb.width / 2 - alen * 3;
                        sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                            ax, fb.height - 24, aname, 0xFFFF4444, 0xFF000000);
                    }
                }
            }

            /* Deck indicator */
            {
                char deckbuf[32];
                snprintf(deckbuf, sizeof(deckbuf), "DECK %d/%d",
                         dng_state.current_floor + 1, current_ship.num_decks);
                sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                    fb.width - 60, 4, deckbuf, 0xFF888888, 0xFF000000);
            }
        }

        /* HUD message (e.g. "CLEAR ENEMIES FIRST") */
        if (dng_hud_msg_timer > 0) {
            dng_hud_msg_timer--;
            uint32_t msg_col = (dng_hud_msg_timer < 20) ? 0xFF555555 : 0xFFFF4444;
            int mlen = 0; for (const char *c = dng_hud_msg; *c; c++) mlen++;
            int mx = fb.width / 2 - mlen * 3;
            sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                mx, fb.height / 2 - 4, dng_hud_msg, msg_col, 0xFF000000);
        }

        if (dng_show_info) {
            static const char *dir_names[] = {"N","E","S","W"};
            char ibuf[64];
            dng_player *ip = &dng_state.player;
            snprintf(ibuf, sizeof(ibuf), "F%d  %s  (%d,%d)",
                     dng_state.current_floor + 1,
                     dir_names[ip->dir],
                     ip->gx, ip->gy);
            sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                3, 3, ibuf, 0xFFFFFFFF, 0xFF000000);
        }

        /* Deck viewer (dungeon) */
        if (deck_view_active)
            draw_deck_viewer(fb.color, fb.width, fb.height);

        /* Expanded map overlay (drawn on top of everything) */
        if (dng_expanded_map) {
            draw_expanded_map(&fb);
            if (current_ship.initialized && current_ship.boarding_active)
                expanded_map_ship_overlay(&fb, &current_ship);
        }
    }

    int tris = sr_stats_tri_count();
    draw_stats(&fb, tris);

    /* ── GIF capture (time-based, 24fps) ─────────────────────── */
    if (sr_gif_is_recording()) {
        gif_capture_timer += dt;
        double interval = 1.0 / GIF_TARGET_FPS;
        while (gif_capture_timer >= interval) {
            gif_capture_timer -= interval;
            sr_gif_capture_frame(fb.color);
        }
    }

    /* ── Aspect-ratio-preserving scale ───────────────────────── */
    float fb_aspect  = (float)FB_WIDTH / (float)FB_HEIGHT;
    float win_aspect = sapp_widthf() / sapp_heightf();
    float scale[2];
    if (win_aspect > fb_aspect) {
        scale[0] = fb_aspect / win_aspect;
        scale[1] = 1.0f;
    } else {
        scale[0] = 1.0f;
        scale[1] = win_aspect / fb_aspect;
    }

    /* Clear UI click state after drawing (consumed this frame) */
    ui_mouse_clicked = false;

    /* ── Upload and display ──────────────────────────────────── */
    sg_update_image(fb_image, &(sg_image_data){
        .mip_levels[0] = { .ptr = fb.color, .size = FB_WIDTH * FB_HEIGHT * 4 }
    });

    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.05f, 0.05f, 0.08f, 1.0f },
            },
        },
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(pip);
    sg_apply_bindings(&sr_bind);
    sg_apply_uniforms(0, &(sg_range){ &scale, sizeof(scale) });
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();

    frame_limiter();
}

static void cleanup(void) {
    if (sr_gif_is_recording())
        sr_gif_stop_and_save();
    for (int i = 0; i < TEX_COUNT; i++)
        sr_texture_free(&textures[i]);
    for (int i = 0; i < ITEX_COUNT; i++)
        sr_indexed_free(&itextures[i]);
    for (int i = 0; i < STEX_COUNT; i++)
        sr_texture_free(&stextures[i]);
    for (int i = 0; i < CONSOLE_TEX_COUNT; i++) {
        if (console_textures[i].pixels) { free(console_textures[i].pixels); console_textures[i].pixels = NULL; }
    }
    sr_framebuffer_destroy(&fb);
    sr_framebuffer_destroy(&shadow_fb);
    sg_shutdown();
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

/* ── Event handler ───────────────────────────────────────────────── */

/* ── Handle tap/click in screen coords ───────────────────────────── */

static void handle_screen_tap(float sx, float sy) {
    float fx, fy;
    screen_to_fb(sx, sy, &fx, &fy);

    /* Set UI click + hover state for interactive widgets (touch has no mouse-move) */
    ui_mouse_clicked = true;
    ui_click_x = fx;
    ui_click_y = fy;
    ui_mouse_x = fx;
    ui_mouse_y = fy;

    if (app_state == STATE_TITLE) {
        /* New Game button */
        int bx = FB_WIDTH/2 - 50, bw = 100, bh = 22;
        if (fx >= bx && fx <= bx + bw && fy >= 120 && fy <= 120 + bh) {
            app_state = STATE_CLASS_SELECT;
            return;
        }
        /* Continue button */
        if (fx >= bx && fx <= bx + bw && fy >= 150 && fy <= 150 + bh && save_exists) {
            if (game_load()) {
                last_player_gx = dng_state.player.gx;
                last_player_gy = dng_state.player.gy;
                app_state = STATE_RUNNING;
            }
            return;
        }
        return;
    }

    if (app_state == STATE_CLASS_SELECT) {
        int gap = (FB_WIDTH - CLASS_COUNT * CLASS_BOX_W) / (CLASS_COUNT + 1);
        int by = 40;
        for (int ci = 0; ci < CLASS_COUNT; ci++) {
            int bx = gap * (ci + 1) + CLASS_BOX_W * ci;
            if (fx >= bx && fx <= bx + CLASS_BOX_W && fy >= by && fy <= by + CLASS_BOX_H) {
                if (class_select_cursor == ci) {
                    selected_class = ci;
                    player_persist_init(selected_class);
                    player_scrap = 30;
                    player_sector = 0;
                    hub_generate(&g_hub);
                    app_state = STATE_SHIP_HUB;
                } else {
                    class_select_cursor = ci;
                }
                return;
            }
        }
        return;
    }

    if (app_state == STATE_SHIP_HUB) {
        if (deck_view_active) {
            /* Close button handled by ui_button in draw_deck_viewer via ui_mouse_clicked */
            return;
        }
        if (g_dialog.active) {
            if (g_dialog.confirm_mode) {
                /* Confirm dialog — check YES/NO button hits */
                int H = FB_HEIGHT;
                int bx = 40, bw = FB_WIDTH - 80, bh = 60;
                int by = H - 70;
                /* YES button area */
                if (fx >= bx + bw - 150 && fx < bx + bw - 90 &&
                    fy >= by + bh - 14 && fy < by + bh - 2) {
                    int action = g_dialog.pending_action;
                    g_dialog.active = false;
                    g_dialog.confirm_mode = false;
                    if (action == DIALOG_ACTION_TELEPORT_GO && g_hub.mission_available) {
                        { int sw, sh; game_ship_size(player_sector, &sw, &sh);
                        dng_game_init_sized(&dng_state, sw, sh); }
                        game_init_ship();
                        dng_initialized = true;
                        last_player_gx = dng_state.player.gx;
                        last_player_gy = dng_state.player.gy;
                        g_hub.mission_available = false;
                        app_state = STATE_RUNNING;
                    }
                }
                /* NO button area */
                else if (fx >= bx + bw - 80 && fx < bx + bw - 20 &&
                         fy >= by + bh - 14 && fy < by + bh - 2) {
                    g_dialog.active = false;
                    g_dialog.confirm_mode = false;
                }
                return;
            }
            /* Normal dialog — tap anywhere dismisses + triggers action */
            int action = g_dialog.pending_action;
            g_dialog.active = false;
            if (action != DIALOG_ACTION_NONE) {
                switch (action) {
                    case DIALOG_ACTION_STARMAP:
                        dng_rng_seed((uint32_t)(player_sector * 1337 + 42));
                        starmap_generate(&g_starmap, player_sector);
                        app_state = STATE_STARMAP;
                        break;
                    case DIALOG_ACTION_SHOP:
                        dng_rng_seed((uint32_t)(player_sector * 7777 + 123));
                        shop_generate(&g_shop);
                        app_state = STATE_SHOP;
                        break;
                    case DIALOG_ACTION_TELEPORT:
                        if (g_hub.mission_available)
                            hub_show_teleport_confirm();
                        break;
                    case DIALOG_ACTION_TELEPORT_GO:
                        if (g_hub.mission_available) {
                            dng_game_init(&dng_state);
                            game_init_ship();
                            dng_initialized = true;
                            last_player_gx = dng_state.player.gx;
                            last_player_gy = dng_state.player.gy;
                            g_hub.mission_available = false;
                            app_state = STATE_RUNNING;
                        }
                        break;
                    case DIALOG_ACTION_HEAL:
                        if (g_player.hp < g_player.hp_max && player_scrap >= 10) {
                            player_scrap -= 10;
                            g_player.hp = g_player.hp_max;
                            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "FULLY HEALED!");
                            g_hub.hud_msg_timer = 60;
                        } else if (g_player.hp >= g_player.hp_max) {
                            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "ALREADY AT FULL HP");
                            g_hub.hud_msg_timer = 60;
                        } else {
                            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "NOT ENOUGH SCRAP");
                            g_hub.hud_msg_timer = 60;
                        }
                        break;
                }
            }
            return;
        }
        if (dng_expanded_map) {
            dng_expanded_map = false;
            return;
        }
        /* Check if tap is on the hub minimap area */
        sr_dungeon *hd = &g_hub.dungeon;
        int mscale = 2;
        int mmx = FB_WIDTH - hd->w * mscale - 4;
        int mmy = 28;  /* hub minimap matches dungeon minimap position */
        int mmw = hd->w * mscale;
        int mmh = hd->h * mscale;
        if (fx >= mmx && fx <= mmx + mmw && fy >= mmy && fy <= mmy + mmh) {
            dng_expanded_map = true;
            return;
        }
        /* Tapping elsewhere does nothing — interaction is via buttons in HUD */
        return;
    }

    if (app_state == STATE_STARMAP) {
        if (g_starmap.confirm_active) return; /* clicks handled by ui_button in draw */
        /* Click on selectable nodes — select or open confirm */
        starmap_node *cur = &g_starmap.nodes[g_starmap.current_node];
        for (int c = 0; c < cur->next_count; c++) {
            int target = cur->next[c];
            if (target < 0 || target >= g_starmap.node_count) continue;
            starmap_node *nd = &g_starmap.nodes[target];
            float ddx = fx - nd->x, ddy = fy - nd->y;
            if (ddx * ddx + ddy * ddy <= 14 * 14) {
                if (g_starmap.cursor == c) {
                    /* Already selected — open confirm dialog */
                    g_starmap.confirm_active = true;
                    g_starmap.confirm_target = target;
                } else {
                    g_starmap.cursor = c;
                }
                return;
            }
        }
        return;
    }

    if (app_state == STATE_COMBAT) {
        /* Result screen — tap anywhere */
        if (combat.phase == CPHASE_RESULT) {
            handle_combat_end();
            return;
        }
        combat_touch_began(&combat, fx, fy);
        return;
    }
}

/* ── Event handler ───────────────────────────────────────────────── */

static void event(const sapp_event *ev) {
    double now_time = sapp_frame_count() * sapp_frame_duration();

    /* ── Track mouse hover in FB coords ────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        screen_to_fb(ev->mouse_x, ev->mouse_y, &ui_mouse_x, &ui_mouse_y);
    }

    /* ── Mouse click / touch began → tap handling ────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        if (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB) {
            dng_touch_began(ev->mouse_x, ev->mouse_y, now_time);
        } else {
            handle_screen_tap(ev->mouse_x, ev->mouse_y);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_BEGAN && ev->num_touches > 0) {
        float sx = ev->touches[0].pos_x, sy = ev->touches[0].pos_y;
        if (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB) {
            dng_touch_began(sx, sy, now_time);
        } else {
            handle_screen_tap(sx, sy);
        }
        return;
    }

    /* ── Mouse move / touch move ─────────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        if (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB)
            dng_touch_moved(ev->mouse_x, ev->mouse_y);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->mouse_x, ev->mouse_y, &fx, &fy);
            combat_touch_moved(&combat, fx, fy);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_MOVED && ev->num_touches > 0) {
        if (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB)
            dng_touch_moved(ev->touches[0].pos_x, ev->touches[0].pos_y);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->touches[0].pos_x, ev->touches[0].pos_y, &fx, &fy);
            combat_touch_moved(&combat, fx, fy);
        }
        return;
    }

    /* ── Mouse up / touch end ────────────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_UP && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        if (app_state == STATE_RUNNING)
            dng_touch_ended(ev->mouse_x, ev->mouse_y, now_time);
        else if (app_state == STATE_SHIP_HUB)
            hub_touch_ended(ev->mouse_x, ev->mouse_y, now_time);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->mouse_x, ev->mouse_y, &fx, &fy);
            combat_touch_ended(&combat, fx, fy);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_ENDED && ev->num_touches > 0) {
        if (app_state == STATE_RUNNING)
            dng_touch_ended(ev->touches[0].pos_x, ev->touches[0].pos_y, now_time);
        else if (app_state == STATE_SHIP_HUB)
            hub_touch_ended(ev->touches[0].pos_x, ev->touches[0].pos_y, now_time);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->touches[0].pos_x, ev->touches[0].pos_y, &fx, &fy);
            combat_touch_ended(&combat, fx, fy);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED) {
        if (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB)
            dng_touch_cancelled();
        return;
    }

    if (ev->type != SAPP_EVENTTYPE_KEY_DOWN) return;

    /* ── Global keys ─────────────────────────────────────────── */
    if (ev->key_code == SAPP_KEYCODE_8 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        if (!sr_gif_is_recording()) {
            gif_capture_timer = 0.0;
            sr_gif_start_recording(FB_WIDTH, FB_HEIGHT);
        }
        return;
    }
    if (ev->key_code == SAPP_KEYCODE_9 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        if (sr_gif_is_recording()) sr_gif_stop_and_save();
        return;
    }
    if (ev->key_code == SAPP_KEYCODE_P && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        save_screenshot(fb.color, FB_WIDTH, FB_HEIGHT);
        return;
    }

    /* ── Title screen ────────────────────────────────────────── */
    if (app_state == STATE_TITLE) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_UP:
            case SAPP_KEYCODE_W:
                title_cursor = 0;
                break;
            case SAPP_KEYCODE_DOWN:
            case SAPP_KEYCODE_S:
                title_cursor = 1;
                break;
            case SAPP_KEYCODE_ENTER:
            case SAPP_KEYCODE_KP_ENTER:
            case SAPP_KEYCODE_SPACE:
                if (title_cursor == 0) {
                    app_state = STATE_CLASS_SELECT;
                } else if (save_exists && game_load()) {
                    last_player_gx = dng_state.player.gx;
                    last_player_gy = dng_state.player.gy;
                    app_state = STATE_RUNNING;
                }
                break;
            default: break;
        }
        return;
    }

    /* ── Class selection ─────────────────────────────────────── */
    if (app_state == STATE_CLASS_SELECT) {
        switch (ev->key_code) {
            case SAPP_KEYCODE_LEFT:
            case SAPP_KEYCODE_A:
                if (class_select_cursor > 0) class_select_cursor--;
                break;
            case SAPP_KEYCODE_RIGHT:
            case SAPP_KEYCODE_D:
                if (class_select_cursor < CLASS_COUNT - 1) class_select_cursor++;
                break;
            case SAPP_KEYCODE_ENTER:
            case SAPP_KEYCODE_KP_ENTER:
            case SAPP_KEYCODE_SPACE:
                selected_class = class_select_cursor;
                player_persist_init(selected_class);
                player_scrap = 30; /* starting scrap */
                player_sector = 0;
                hub_generate(&g_hub);
                app_state = STATE_SHIP_HUB;
                break;
            default: break;
        }
        return;
    }

    /* ── Ship hub state ──────────────────────────────────────── */
    if (app_state == STATE_SHIP_HUB) {
        /* Handle expanded map */
        if (dng_expanded_map) {
            if (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_M) {
                dng_expanded_map = false;
            }
            return;
        }
        if (ev->key_code == SAPP_KEYCODE_M) {
            dng_expanded_map = true;
            return;
        }
        /* Deck viewer */
        if (deck_view_active) {
            if (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_TAB) {
                deck_view_active = false;
            }
            return;
        }
        if (ev->key_code == SAPP_KEYCODE_TAB) {
            deck_view_active = true;
            return;
        }
        if (g_dialog.active) {
            if (g_dialog.confirm_mode) {
                /* YES/NO confirm dialog */
                bool yes = (ev->key_code == SAPP_KEYCODE_Y || ev->key_code == SAPP_KEYCODE_ENTER ||
                            ev->key_code == SAPP_KEYCODE_SPACE);
                bool no  = (ev->key_code == SAPP_KEYCODE_N || ev->key_code == SAPP_KEYCODE_ESCAPE);
                if (yes) {
                    int action = g_dialog.pending_action;
                    g_dialog.active = false;
                    g_dialog.confirm_mode = false;
                    switch (action) {
                        case DIALOG_ACTION_TELEPORT_GO:
                            if (g_hub.mission_available) {
                                dng_game_init(&dng_state);
                                game_init_ship();
                                dng_initialized = true;
                                last_player_gx = dng_state.player.gx;
                                last_player_gy = dng_state.player.gy;
                                g_hub.mission_available = false;
                                app_state = STATE_RUNNING;
                            }
                            break;
                        default: break;
                    }
                } else if (no) {
                    g_dialog.active = false;
                    g_dialog.confirm_mode = false;
                }
            } else if (ev->key_code == SAPP_KEYCODE_F || ev->key_code == SAPP_KEYCODE_ENTER ||
                ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_ESCAPE) {
                int action = g_dialog.pending_action;
                g_dialog.active = false;
                /* Trigger pending action on dismiss (not on ESC) */
                if (ev->key_code != SAPP_KEYCODE_ESCAPE && action != DIALOG_ACTION_NONE) {
                    switch (action) {
                        case DIALOG_ACTION_STARMAP:
                            dng_rng_seed((uint32_t)(player_sector * 1337 + 42));
                            starmap_generate(&g_starmap, player_sector);
                            app_state = STATE_STARMAP;
                            break;
                        case DIALOG_ACTION_SHOP:
                            dng_rng_seed((uint32_t)(player_sector * 7777 + 123));
                            shop_generate(&g_shop);
                            app_state = STATE_SHOP;
                            break;
                        case DIALOG_ACTION_TELEPORT:
                            if (g_hub.mission_available)
                                hub_show_teleport_confirm();
                            break;
                        case DIALOG_ACTION_TELEPORT_GO:
                            if (g_hub.mission_available) {
                                dng_game_init(&dng_state);
                                game_init_ship();
                                dng_initialized = true;
                                last_player_gx = dng_state.player.gx;
                                last_player_gy = dng_state.player.gy;
                                g_hub.mission_available = false;
                                app_state = STATE_RUNNING;
                            }
                            break;
                        case DIALOG_ACTION_HEAL:
                            if (g_player.hp < g_player.hp_max && player_scrap >= 10) {
                                player_scrap -= 10;
                                g_player.hp = g_player.hp_max;
                                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "FULLY HEALED!");
                                g_hub.hud_msg_timer = 60;
                            } else if (g_player.hp >= g_player.hp_max) {
                                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "ALREADY AT FULL HP");
                                g_hub.hud_msg_timer = 60;
                            } else {
                                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "NOT ENOUGH SCRAP");
                                g_hub.hud_msg_timer = 60;
                            }
                            break;
                    }
                }
            }
            return;
        }
        switch (ev->key_code) {
            case SAPP_KEYCODE_W: case SAPP_KEYCODE_UP:
                dng_player_try_move(&g_hub.player, &g_hub.dungeon, g_hub.player.dir);
                break;
            case SAPP_KEYCODE_S: case SAPP_KEYCODE_DOWN:
                dng_player_try_move(&g_hub.player, &g_hub.dungeon, (g_hub.player.dir + 2) % 4);
                break;
            case SAPP_KEYCODE_A:
                dng_player_try_move(&g_hub.player, &g_hub.dungeon, (g_hub.player.dir + 3) % 4);
                break;
            case SAPP_KEYCODE_D:
                dng_player_try_move(&g_hub.player, &g_hub.dungeon, (g_hub.player.dir + 1) % 4);
                break;
            case SAPP_KEYCODE_LEFT: case SAPP_KEYCODE_Q:
                g_hub.player.dir = (g_hub.player.dir + 3) % 4;
                g_hub.player.target_angle -= 0.25f;
                break;
            case SAPP_KEYCODE_RIGHT: case SAPP_KEYCODE_E:
                g_hub.player.dir = (g_hub.player.dir + 1) % 4;
                g_hub.player.target_angle += 0.25f;
                break;
            case SAPP_KEYCODE_F: {
                /* Unified interact: F -> Dialog -> Action */
                int look_gx = g_hub.player.gx + dng_dir_dx[g_hub.player.dir];
                int look_gy = g_hub.player.gy + dng_dir_dz[g_hub.player.dir];
                int npc = hub_npc_at(look_gx, look_gy);
                if (npc >= 0) {
                    /* Facing an NPC — figure out room action for this NPC's room */
                    int npc_room = g_hub.crew[npc].room;
                    int action = DIALOG_ACTION_NONE;
                    if (npc_room >= 0 && npc_room < g_hub.dungeon.room_count)
                        action = hub_room_action_for_type(g_hub.room_types[npc_room]);
                    hub_start_dialog(npc, action);
                } else {
                    /* No NPC — check room action directly */
                    int room_idx = hub_room_at_pos(g_hub.player.gx, g_hub.player.gy);
                    if (room_idx >= 0) {
                        int action = hub_room_action_for_type(g_hub.room_types[room_idx]);
                        if (action != DIALOG_ACTION_NONE) {
                            /* Show a brief room prompt dialog then trigger action */
                            memset(&g_dialog, 0, sizeof(g_dialog));
                            int rt = g_hub.room_types[room_idx];
                            snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "%s", hub_room_names[rt]);
                            switch (action) {
                                case DIALOG_ACTION_STARMAP:
                                    snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "ACCESSING STAR MAP...");
                                    g_dialog.line_count = 1;
                                    break;
                                case DIALOG_ACTION_SHOP:
                                    snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "BROWSING INVENTORY...");
                                    g_dialog.line_count = 1;
                                    break;
                                case DIALOG_ACTION_TELEPORT:
                                    if (!g_hub.mission_available) {
                                        snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "NO MISSIONS AVAILABLE.");
                                        g_dialog.line_count = 1;
                                        action = DIALOG_ACTION_NONE;
                                    } else {
                                        snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "TELEPORTER PRIMED.");
                                        snprintf(g_dialog.lines[1], DIALOG_LINE_LEN, "READY TO DEPLOY.");
                                        g_dialog.line_count = 2;
                                    }
                                    break;
                                case DIALOG_ACTION_HEAL:
                                    snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "MEDICAL STATION ONLINE.");
                                    g_dialog.line_count = 1;
                                    break;
                            }
                            g_dialog.pending_action = action;
                            g_dialog.active = true;
                        }
                    }
                }
                break;
            }
            default: break;
        }
        return;
    }

    /* ── Shop state ──────────────────────────────────────────── */
    if (app_state == STATE_SHOP) {
        if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
            g_shop.active = false;
            app_state = STATE_SHIP_HUB;
        } else {
            shop_handle_key(ev->key_code);
        }
        return;
    }

    /* ── Star map state ──────────────────────────────────────── */
    if (app_state == STATE_STARMAP) {
        if (ev->key_code == SAPP_KEYCODE_ESCAPE && !g_starmap.confirm_active) {
            g_starmap.active = false;
            app_state = STATE_SHIP_HUB;
        } else if (ev->key_code == SAPP_KEYCODE_ENTER || ev->key_code == SAPP_KEYCODE_SPACE) {
            if (!g_starmap.confirm_active) {
                /* Open confirm dialog */
                starmap_node *cur = &g_starmap.nodes[g_starmap.current_node];
                if (cur->next_count > 0 && g_starmap.cursor >= 0 && g_starmap.cursor < cur->next_count) {
                    int next = cur->next[g_starmap.cursor];
                    if (next >= 0 && next < g_starmap.node_count) {
                        g_starmap.confirm_active = true;
                        g_starmap.confirm_target = next;
                    }
                }
            } else {
                starmap_handle_key(ev->key_code);
            }
        } else {
            starmap_handle_key(ev->key_code);
        }
        return;
    }

    /* ── Combat state ────────────────────────────────────────── */
    if (app_state == STATE_COMBAT) {
        if (combat.phase == CPHASE_RESULT &&
            (ev->key_code == SAPP_KEYCODE_ENTER || ev->key_code == SAPP_KEYCODE_SPACE)) {
            handle_combat_end();
            return;
        }
        combat_handle_key(&combat, ev->key_code);
        return;
    }

    /* ── Dungeon keys ────────────────────────────────────────── */

    /* Close expanded map on Escape or any movement key */
    if (dng_expanded_map) {
        if (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_M) {
            dng_expanded_map = false;
            return;
        }
        return; /* Consume all keys while map is open */
    }

    /* Deck viewer in dungeon */
    if (deck_view_active) {
        if (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_TAB) {
            deck_view_active = false;
        }
        return;
    }

    switch (ev->key_code) {
        case SAPP_KEYCODE_TAB:
            deck_view_active = true;
            break;
        case SAPP_KEYCODE_M:
            dng_expanded_map = true;
            break;
        case SAPP_KEYCODE_L:
            dng_light_mode = (dng_light_mode + 1) % 2;
            break;
        case SAPP_KEYCODE_I:
            dng_show_info = !dng_show_info;
            break;
        case SAPP_KEYCODE_EQUAL:
        case SAPP_KEYCODE_KP_ADD:
            dng_cfg.light_brightness += 0.1f;
            if (dng_cfg.light_brightness > 5.0f) dng_cfg.light_brightness = 5.0f;
            break;
        case SAPP_KEYCODE_MINUS:
        case SAPP_KEYCODE_KP_SUBTRACT:
            dng_cfg.light_brightness -= 0.1f;
            if (dng_cfg.light_brightness < 0.0f) dng_cfg.light_brightness = 0.0f;
            break;
        case SAPP_KEYCODE_W:
        case SAPP_KEYCODE_UP:
            if (dng_play_state == DNG_STATE_PLAYING)
                dng_player_try_move(&dng_state.player, dng_state.dungeon, dng_state.player.dir);
            break;
        case SAPP_KEYCODE_S:
        case SAPP_KEYCODE_DOWN:
            if (dng_play_state == DNG_STATE_PLAYING)
                dng_player_try_move(&dng_state.player, dng_state.dungeon, (dng_state.player.dir + 2) % 4);
            break;
        case SAPP_KEYCODE_A:
            if (dng_play_state == DNG_STATE_PLAYING)
                dng_player_try_move(&dng_state.player, dng_state.dungeon, (dng_state.player.dir + 3) % 4);
            break;
        case SAPP_KEYCODE_D:
            if (dng_play_state == DNG_STATE_PLAYING)
                dng_player_try_move(&dng_state.player, dng_state.dungeon, (dng_state.player.dir + 1) % 4);
            break;
        case SAPP_KEYCODE_LEFT:
        case SAPP_KEYCODE_Q:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 3) % 4;
                dng_state.player.target_angle -= 0.25f;
            }
            break;
        case SAPP_KEYCODE_RIGHT:
        case SAPP_KEYCODE_E:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 1) % 4;
                dng_state.player.target_angle += 0.25f;
            }
            break;
        default: break;
    }
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb    = init,
        .frame_cb   = frame,
        .cleanup_cb = cleanup,
        .event_cb   = event,
        .width      = FB_WIDTH * 2,
        .height     = FB_HEIGHT * 2,
        .window_title = "Space Hulks",
        .high_dpi     = true,
        .logger.func  = slog_func,
        .swap_interval = 0,
    };
}
