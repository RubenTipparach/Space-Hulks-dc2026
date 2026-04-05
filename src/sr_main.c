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
#include "sr_audio.h"

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

#include "sr_combat.h"

#include "sr_json.h"
#include "sr_ship.h"
#include "sr_level_loader.h"
#include "sr_dialog_data.h"
static void game_init_ship(void); /* forward decl */
static void game_pregen_enemy_ship(void); /* forward decl */

/* Enemy ship exterior position per size (loaded from game_config.yaml) */
typedef struct { float x_off, y_off, z_off, hover_amp, hover_speed; } enemy_ship_cfg;
static enemy_ship_cfg enemy_ship_small  = { 0.0f, -2.0f, -30.0f, 0.3f, 1.0f };
static enemy_ship_cfg enemy_ship_medium = { 0.0f, -12.0f, -60.0f, 0.2f, 0.4f };
static enemy_ship_cfg enemy_ship_large  = { 0.0f, -15.0f, -120.0f, 0.15f, 0.3f };

/* Hub ship as seen from enemy ship (one per enemy ship size) */
static enemy_ship_cfg hub_from_small  = { 0.0f, -2.0f, 30.0f, 0.2f, 0.8f };
static enemy_ship_cfg hub_from_medium = { 0.0f, -2.0f, 50.0f, 0.2f, 0.8f };
static enemy_ship_cfg hub_from_large  = { 0.0f, -2.0f, 80.0f, 0.15f, 0.6f };
static void game_save(void); /* forward decl */
#include "sr_scene_ship_hub.h"
#include "sr_menu.h"
static void handle_screen_tap(float sx, float sy); /* forward decl */
#include "sr_mobile_input.h"

/* Ship grid size based on sector difficulty: small(20), medium(40), large(80) */
static void game_ship_size(int sector, int *out_w, int *out_h) {
    if (sector >= 6) { *out_w = 80; *out_h = 80; }
    else if (sector >= 3) { *out_w = 40; *out_h = 40; }
    else { *out_w = 20; *out_h = 20; }
}

/* Pre-generate enemy ship layout for exterior rendering (no enemies/player setup) */
static void game_pregen_enemy_ship(void) {
    if (dng_state.floor_generated[0]) return; /* already generated */

    /* Check current starmap node's levelFile */
    const char *level_path = NULL;
    if (g_starmap.active && g_starmap.current_node >= 0 &&
        g_starmap.current_node < g_starmap.node_count) {
        const char *lf = g_starmap.nodes[g_starmap.current_node].level_file;
        if (lf[0]) {
            static char pregen_path[128];
            snprintf(pregen_path, sizeof(pregen_path), "levels/%s", lf);
            level_path = pregen_path;
            printf("[pregen] Using starmap node levelFile: %s\n", level_path);
        }
    }
    if (!level_path) level_path = "levels/sample_enemy_ship.json";
    if (lvl_file_exists(level_path)) {
        lvl_loaded lvl = lvl_load(level_path);
        if (lvl.valid && !lvl.is_hub) {
            lvl_load_ship(&current_ship, &lvl.json, lvl.root);
            dng_state.max_floors = current_ship.num_decks;
            for (int dk = 0; dk < current_ship.num_decks && dk < DNG_MAX_FLOORS; dk++)
                dng_state.deck_room_counts[dk] = current_ship.deck_room_count[dk];
            lvl_load_all_floors(&lvl, dng_state.floors,
                                dng_state.floor_generated, DNG_MAX_FLOORS);
            dng_state.grid_w = dng_state.floors[0].w;
            dng_state.grid_h = dng_state.floors[0].h;
            dng_state.current_floor = 0;
            dng_state.dungeon = &dng_state.floors[0];
            /* Count windows loaded */
            int win_count = 0;
            sr_dungeon *ed = &dng_state.floors[0];
            for (int gy = 1; gy <= ed->h; gy++)
                for (int gx = 1; gx <= ed->w; gx++)
                    if (ed->win_faces[gy][gx]) win_count++;
            printf("[pregen] Enemy ship loaded from %s (%dx%d, %d window cells)\n",
                   level_path, dng_state.grid_w, dng_state.grid_h, win_count);
            /* lvl goes out of scope — JSON data is stack/static, no free needed */
            return;
        }
    }

    /* Fallback: procedural generation */
    int gw, gh;
    game_ship_size(player_sector, &gw, &gh);
    dng_state.grid_w = gw;
    dng_state.grid_h = gh;
    dng_state.seed_base = (uint32_t)(time(NULL) ^ (player_sector * 12345));

    uint32_t ship_seed = dng_state.seed_base + 9999;
    ship_generate(&current_ship, player_sector, ship_seed);
    dng_state.max_floors = current_ship.num_decks;
    for (int dk = 0; dk < current_ship.num_decks && dk < DNG_MAX_FLOORS; dk++)
        dng_state.deck_room_counts[dk] = current_ship.deck_room_count[dk];

    bool is_last = (current_ship.num_decks <= 1);
    int deck_rooms = current_ship.deck_room_count[0];
    dng_generate_ex(&dng_state.floors[0], gw, gh, false, !is_last,
                    dng_state.seed_base, 0, deck_rooms);
    dng_state.floor_generated[0] = true;
    dng_state.current_floor = 0;
    dng_state.dungeon = &dng_state.floors[0];
    printf("[pregen] Enemy ship procedurally generated (%dx%d)\n", gw, gh);
}

/* ── Save / Load ─────────────────────────────────────────────────── */

#define SAVE_VERSION 8  /* bumped: added boss mission state */

/*  Save header — fixed-size portion written first.
 *  After the header, each generated dungeon floor (sr_dungeon) is
 *  written sequentially.  On WASM the whole blob is persisted to
 *  localStorage so it survives page reloads.                         */
typedef struct {
    uint32_t magic;
    uint32_t version;

    /* Scene / progression */
    int  app_state_saved;          /* STATE_RUNNING or STATE_COMBAT */
    int  selected_class;
    int  player_scrap_saved;
    int  player_biomass_saved;
    int  player_sector_saved;
    int  player_consumables_saved[CONSUMABLE_SLOTS];

    /* Persistent player */
    player_persist player;

    /* Ship */
    ship_state ship;

    /* Dungeon meta */
    int      current_floor;
    uint32_t seed_base;
    int      grid_w, grid_h;
    int      max_floors;
    bool     floor_generated[DNG_MAX_FLOORS];

    /* Player position */
    int player_gx, player_gy, player_dir;

    /* Combat (only meaningful when app_state_saved == STATE_COMBAT) */
    bool         in_combat;
    combat_state combat_snap;
    int          saved_combat_room;
    bool         saved_console_combat;

    /* Weakness system */
    weakness_table weakness;

    /* Starmap progress */
    int  starmap_current_node;
    int  starmap_derelicts_visited;
    bool starmap_node_visited[STARMAP_MAX_NODES];
    int  starmap_visited_path[STARMAP_MAX_NODES];
    int  starmap_visited_path_count;

    /* Boss mission state */
    bool is_boss_mission;
} save_header;

/* Variables used by save/load — declared here so they're visible to game_save/game_load */
static int current_combat_room = -1;
static bool console_combat = false;

/* ── WASM persistence helpers ────────────────────────────────────── */

#if defined(__EMSCRIPTEN__)

/* Copy virtual-FS save file → localStorage (base-64) */
static void save_persist_wasm(void) {
    EM_ASM({
        try {
            var bytes = FS.readFile('/spacehulks.sav');
            var binary = '';
            var chunk = 8192;
            for (var i = 0; i < bytes.length; i += chunk) {
                var end = Math.min(i + chunk, bytes.length);
                for (var j = i; j < end; j++)
                    binary += String.fromCharCode(bytes[j]);
            }
            localStorage.setItem('spacehulks_save', btoa(binary));
        } catch(e) { console.warn('save_persist_wasm:', e); }
    });
}

/* Copy localStorage → virtual-FS save file (called once at startup) */
static void save_restore_wasm(void) {
    EM_ASM({
        try {
            var b64 = localStorage.getItem('spacehulks_save');
            if (!b64) return;
            var binary = atob(b64);
            var bytes = new Uint8Array(binary.length);
            for (var i = 0; i < binary.length; i++)
                bytes[i] = binary.charCodeAt(i);
            FS.writeFile('/spacehulks.sav', bytes);
        } catch(e) { console.warn('save_restore_wasm:', e); }
    });
}
static void save_clear_wasm(void) {
    EM_ASM({
        try { localStorage.removeItem('spacehulks_save'); } catch(e) {}
    });
}
#endif /* __EMSCRIPTEN__ */

/* Delete save file (game over) */
static void game_delete_save(void) {
    remove(SAVE_FILE);
    save_exists = false;
#if defined(__EMSCRIPTEN__)
    save_clear_wasm();
#endif
}

/* ── Save ────────────────────────────────────────────────────────── */

static void game_save(void) {
    /* Only save while exploring, fighting, or on the hub */
    if (app_state != STATE_RUNNING && app_state != STATE_COMBAT && app_state != STATE_SHIP_HUB) return;

    save_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic   = SAVE_MAGIC;
    hdr.version = SAVE_VERSION;

    hdr.app_state_saved     = app_state;
    hdr.selected_class      = selected_class;
    hdr.player_scrap_saved  = player_scrap;
    hdr.player_biomass_saved = player_biomass;
    memcpy(hdr.player_consumables_saved, player_consumables, sizeof(player_consumables));
    hdr.player_sector_saved = player_sector;

    hdr.player = g_player;
    hdr.ship   = current_ship;

    hdr.current_floor = dng_state.current_floor;
    hdr.seed_base     = dng_state.seed_base;
    hdr.grid_w        = dng_state.grid_w;
    hdr.grid_h        = dng_state.grid_h;
    hdr.max_floors    = dng_state.max_floors;
    memcpy(hdr.floor_generated, dng_state.floor_generated,
           sizeof(hdr.floor_generated));

    hdr.player_gx  = dng_state.player.gx;
    hdr.player_gy  = dng_state.player.gy;
    hdr.player_dir = dng_state.player.dir;

    hdr.in_combat             = (app_state == STATE_COMBAT);
    hdr.combat_snap           = combat;
    hdr.saved_combat_room     = current_combat_room;
    hdr.saved_console_combat  = console_combat;
    hdr.weakness              = g_weakness;

    /* Starmap progress */
    hdr.starmap_current_node = g_starmap.current_node;
    hdr.starmap_derelicts_visited = g_starmap.derelicts_visited;
    for (int i = 0; i < STARMAP_MAX_NODES; i++)
        hdr.starmap_node_visited[i] = (i < g_starmap.node_count) ? g_starmap.nodes[i].visited : false;
    memcpy(hdr.starmap_visited_path, g_starmap.visited_path, sizeof(hdr.starmap_visited_path));
    hdr.starmap_visited_path_count = g_starmap.visited_path_count;
    hdr.is_boss_mission = current_mission_is_boss;

    FILE *f = fopen(SAVE_FILE, "wb");
    if (!f) return;
    fwrite(&hdr, sizeof(hdr), 1, f);

    /* Write each generated floor */
    for (int i = 0; i < DNG_MAX_FLOORS; i++) {
        if (dng_state.floor_generated[i])
            fwrite(&dng_state.floors[i], sizeof(sr_dungeon), 1, f);
    }
    fclose(f);

#if defined(__EMSCRIPTEN__)
    save_persist_wasm();
#endif
}

/* ── Load ────────────────────────────────────────────────────────── */

static bool game_load(void) {
#if defined(__EMSCRIPTEN__)
    save_restore_wasm();
#endif

    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return false;

    save_header hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1 ||
        hdr.magic != SAVE_MAGIC || hdr.version != SAVE_VERSION) {
        fclose(f); return false;
    }

    /* Restore dungeon floors */
    memset(&dng_state, 0, sizeof(dng_state));
    memcpy(dng_state.floor_generated, hdr.floor_generated,
           sizeof(dng_state.floor_generated));
    for (int i = 0; i < DNG_MAX_FLOORS; i++) {
        if (dng_state.floor_generated[i]) {
            if (fread(&dng_state.floors[i], sizeof(sr_dungeon), 1, f) != 1) {
                fclose(f); return false;
            }
        }
    }
    fclose(f);

    /* Restore progression */
    selected_class = hdr.selected_class;
    player_scrap   = hdr.player_scrap_saved;
    player_biomass = hdr.player_biomass_saved;
    memcpy(player_consumables, hdr.player_consumables_saved, sizeof(player_consumables));
    player_sector  = hdr.player_sector_saved;

    /* Restore player */
    g_player = hdr.player;

    /* Restore ship */
    current_ship = hdr.ship;

    /* Restore dungeon meta */
    dng_state.current_floor = hdr.current_floor;
    dng_state.seed_base     = hdr.seed_base;
    dng_state.grid_w        = hdr.grid_w;
    dng_state.grid_h        = hdr.grid_h;
    dng_state.max_floors    = hdr.max_floors;
    dng_state.dungeon       = &dng_state.floors[hdr.current_floor];

    /* Restore player position */
    dng_player_init(&dng_state.player,
                    hdr.player_gx, hdr.player_gy, hdr.player_dir);
    dng_initialized = true;
    dng_hull_computed = false;

    /* Restore weakness table */
    g_weakness = hdr.weakness;

    /* Restore starmap progress — load structure from JSON, then apply saved state */
    if (starmap_load_json(&g_starmap, "levels/starmap.json")) {
        g_starmap.current_node = hdr.starmap_current_node;
        g_starmap.derelicts_visited = hdr.starmap_derelicts_visited;
        for (int i = 0; i < g_starmap.node_count && i < STARMAP_MAX_NODES; i++)
            g_starmap.nodes[i].visited = hdr.starmap_node_visited[i];
        memcpy(g_starmap.visited_path, hdr.starmap_visited_path, sizeof(g_starmap.visited_path));
        g_starmap.visited_path_count = hdr.starmap_visited_path_count;
    }

    /* Restore combat if saved mid-fight */
    if (hdr.in_combat) {
        combat              = hdr.combat_snap;
        current_combat_room = hdr.saved_combat_room;
        console_combat      = hdr.saved_console_combat;
        app_state = STATE_COMBAT;
    } else {
        current_combat_room = -1;
        console_combat      = false;
        app_state = STATE_RUNNING;
    }

    /* Restore boss mission state */
    current_mission_is_boss = hdr.is_boss_mission;
    /* Also check starmap node in case save predates this field */
    if (!current_mission_is_boss && g_starmap.current_node >= 0 &&
        g_starmap.current_node < g_starmap.node_count &&
        g_starmap.nodes[g_starmap.current_node].is_boss)
        current_mission_is_boss = true;

    if (current_mission_is_boss)
        printf("[LOAD] Boss mission active (node %d)\n", g_starmap.current_node);

    /* Initialize enemy AI from restored dungeon grid */
    dng_enemies_init(dng_state.dungeon);
    printf("[LOAD] Enemy AI initialized, count=%d\n", dng_enemy_count);

    /* Loading a save means player is past the intro flow */
    mission_briefed = true;
    mission_medbay_done = true;
    mission_armory_done = true;

    return true;
}

static bool game_has_save(void) {
#if defined(__EMSCRIPTEN__)
    save_restore_wasm();
#endif
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return false;
    save_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    bool ok = (fread(&hdr, sizeof(hdr), 1, f) == 1 &&
               hdr.magic == SAVE_MAGIC && hdr.version == SAVE_VERSION);
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

    sr_draw_text_centered(px, W, H, 60, "DRAKE'S VOID", white, shadow);

    if (title_confirm_new) {
        /* Confirm dialog — hides buttons behind it */
        int dbw = 260, dbh = 55;
        int dbx = W/2 - dbw/2, dby = H/2 - dbh/2;
        for (int ry = dby; ry < dby + dbh && ry < H; ry++)
            for (int rx = dbx; rx < dbx + dbw && rx < W; rx++)
                if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF111133;
        for (int rx = dbx; rx < dbx + dbw && rx < W; rx++) {
            if (dby >= 0) px[dby * W + rx] = 0xFF4444AA;
            if (dby+dbh-1 < H) px[(dby+dbh-1) * W + rx] = 0xFF4444AA;
        }
        for (int ry = dby; ry < dby + dbh && ry < H; ry++) {
            if (dbx >= 0) px[ry * W + dbx] = 0xFF4444AA;
            if (dbx+dbw-1 < W) px[ry * W + dbx+dbw-1] = 0xFF4444AA;
        }
        sr_draw_text_centered(px, W, H, dby + 8, "START NEW RUN?", 0xFFCCCCCC, shadow);
        sr_draw_text_centered(px, W, H, dby + 20, "PREVIOUS SAVE WILL BE DELETED", 0xFFCC4444, shadow);
        if (ui_button(px, W, H, dbx + 20, dby + dbh - 18, 80, 14, "YES",
                      0xFF112211, 0xFF223322, 0xFF44CC44)) {
            game_delete_save();
            save_exists = false;
            title_confirm_new = false;
            app_state = STATE_CLASS_SELECT;
        }
        if (ui_button(px, W, H, dbx + dbw - 100, dby + dbh - 18, 80, 14, "NO",
                      0xFF221111, 0xFF332222, 0xFF882222)) {
            title_confirm_new = false;
        }
    } else {
        /* New Game button */
        {
            int bw = 100, bh = 22;
            int bx = (W - bw) / 2, by = 120;
            if (ui_button(px, W, H, bx, by, bw, bh, "NEW GAME",
                          0xFF111122, 0xFF222244, 0xFF333366)) {
                if (save_exists)
                    title_confirm_new = true;
                else
                    app_state = STATE_CLASS_SELECT;
            }
        }

        /* Continue button */
        {
            int bw = 100, bh = 22;
            int bx = (W - bw) / 2, by = 150;
            if (save_exists) {
                if (ui_button(px, W, H, bx, by, bw, bh, "CONTINUE",
                              0xFF111122, 0xFF222244, 0xFF333366)) {
                    title_cursor = 99;
                }
            } else {
                combat_draw_rect(px, W, H, bx, by, bw, bh, 0xFF111122);
                combat_draw_rect_outline(px, W, H, bx, by, bw, bh, 0xFF333333);
                int tx = bx + (bw - sr_text_width("CONTINUE")) / 2;
                sr_draw_text_shadow(px, W, H, tx, by + 7, "CONTINUE", 0xFF444444, shadow);
            }
        }
    }
}

/* ── Intro teletype screen ──────────────────────────────────────── */

static void draw_intro_screen(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;
    uint32_t green = 0xFF44CC44;
    uint32_t dim_green = 0xFF227722;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0A0A0A;

    if (!g_dlgd.loaded) return;

    /* Advance teletype: 2 chars per frame */
    intro_timer++;
    if (!intro_done) {
        intro_char_idx = intro_timer * 2;
        /* Count total chars across all lines */
        int total = 0;
        for (int i = 0; i < g_dlgd.intro_count; i++)
            total += (int)strlen(g_dlgd.intro_lines[i]) + 1; /* +1 for newline */
        if (intro_char_idx >= total) {
            intro_char_idx = total;
            intro_done = true;
        }
    }

    /* Draw revealed text */
    int chars_left = intro_char_idx;
    int lh = 9; /* line height */
    int y = 10;
    for (int i = 0; i < g_dlgd.intro_count && chars_left > 0; i++) {
        const char *line = g_dlgd.intro_lines[i];
        int llen = (int)strlen(line);

        /* Blank line marker */
        if (line[0] == '_' && llen == 1) {
            y += lh;
            chars_left -= 2; /* consume the line + newline */
            continue;
        }

        /* Build partial string if typing through this line */
        char partial[DLGD_LINE_LEN];
        int show = (chars_left >= llen) ? llen : chars_left;
        memcpy(partial, line, show);
        partial[show] = '\0';

        sr_draw_text_shadow(px, W, H, 20, y, partial, green, shadow);
        chars_left -= llen + 1;
        y += lh;
    }

    /* Blinking cursor */
    if (!intro_done && (intro_timer / 15) % 2 == 0) {
        int cx = 20, cy = 10;
        int remain = intro_char_idx;
        for (int i = 0; i < g_dlgd.intro_count && remain > 0; i++) {
            const char *line = g_dlgd.intro_lines[i];
            int llen = (int)strlen(line);
            if (line[0] == '_' && llen == 1) { cy += lh; remain -= 2; continue; }
            int show = (remain >= llen) ? llen : remain;
            cx = 20 + show * 6;
            remain -= llen + 1;
            if (remain >= 0) { cy += lh; cx = 20; }
        }
        sr_draw_text_shadow(px, W, H, cx, cy, "_", green, shadow);
    }

    /* Skip / continue prompt */
    if (intro_done) {
        uint32_t blink = ((intro_timer / 30) % 2 == 0) ? green : dim_green;
        sr_draw_text_shadow(px, W, H, W/2 - 60, H - 20,
                            "PRESS SPACE TO CONTINUE", blink, shadow);
    } else {
        sr_draw_text_shadow(px, W, H, W - 90, H - 12,
                            "SPACE TO SKIP", dim_green, shadow);
    }
}

/* ── Run stats screen ──────────────────────────────────────────── */

static void draw_run_stats(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0A0A0A;

    const char *title = epilogue_is_win ? "MISSION COMPLETE" : "MISSION FAILED";
    uint32_t title_col = epilogue_is_win ? 0xFF44CC44 : 0xFF4444CC;
    sr_draw_text_centered(px, W, H, 16, title, title_col, shadow);
    sr_draw_text_centered(px, W, H, 30, "RUN STATISTICS", 0xFFCCCCCC, shadow);

    int y = 50;
    int x = W / 2 - 80;
    uint32_t label = 0xFF888888;
    uint32_t val_col = 0xFFEEEEEE;
    char buf[64];

    snprintf(buf, sizeof(buf), "SECTORS VISITED     %d", g_run_stats.sectors_visited);
    sr_draw_text_shadow(px, W, H, x, y, buf, val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "ENEMIES KILLED      %d", g_run_stats.enemies_killed);
    sr_draw_text_shadow(px, W, H, x, y, buf, val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "BOSSES KILLED       %d", g_run_stats.bosses_killed);
    sr_draw_text_shadow(px, W, H, x, y, buf, g_run_stats.bosses_killed > 0 ? 0xFF44CC44 : val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "DAMAGE DEALT        %d", g_run_stats.damage_dealt);
    sr_draw_text_shadow(px, W, H, x, y, buf, val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "DAMAGE TAKEN        %d", g_run_stats.damage_taken);
    sr_draw_text_shadow(px, W, H, x, y, buf, val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "CARDS GATHERED      %d", g_run_stats.cards_gathered);
    sr_draw_text_shadow(px, W, H, x, y, buf, val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "LOOT CHESTS FOUND   %d", g_run_stats.chests_found);
    sr_draw_text_shadow(px, W, H, x, y, buf, val_col, shadow); y += 12;

    snprintf(buf, sizeof(buf), "SCRAP EARNED        %d", g_run_stats.scrap_earned);
    sr_draw_text_shadow(px, W, H, x, y, buf, 0xFFEECC44, shadow); y += 12;

    snprintf(buf, sizeof(buf), "BIOMASS EARNED      %d", g_run_stats.biomass_earned);
    sr_draw_text_shadow(px, W, H, x, y, buf, 0xFF44CCAA, shadow); y += 16;

    /* Final deck size */
    snprintf(buf, sizeof(buf), "FINAL DECK SIZE     %d", g_player.persistent_deck_count);
    sr_draw_text_shadow(px, W, H, x, y, buf, 0xFF44AACC, shadow);

    uint32_t blink = ((beam_timer++ / 30) % 2 == 0) ? 0xFFCCCCCC : 0xFF666666;
    sr_draw_text_centered(px, W, H, H - 16, "PRESS SPACE TO CONTINUE", blink, shadow);
}

/* ── Epilogue teletype screen ───────────────────────────────────── */

static void draw_epilogue_screen(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;
    uint32_t text_col = epilogue_is_win ? 0xFF44CC44 : 0xFF4444CC;
    uint32_t dim_col  = epilogue_is_win ? 0xFF227722 : 0xFF222266;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0A0A0A;

    if (!g_dlgd.loaded) return;

    const char (*lines)[DLGD_LINE_LEN] = epilogue_is_win
        ? g_dlgd.epilogue_win : g_dlgd.epilogue_loss;
    int line_count = epilogue_is_win
        ? g_dlgd.epilogue_win_count : g_dlgd.epilogue_loss_count;

    /* Advance teletype */
    intro_timer++;
    if (!intro_done) {
        intro_char_idx = intro_timer * 2;
        int total = 0;
        for (int i = 0; i < line_count; i++)
            total += (int)strlen(lines[i]) + 1;
        if (intro_char_idx >= total) {
            intro_char_idx = total;
            intro_done = true;
        }
    }

    /* Draw text */
    int chars_left = intro_char_idx;
    int y = 20;
    for (int i = 0; i < line_count && chars_left > 0; i++) {
        const char *line = lines[i];
        int llen = (int)strlen(line);

        if (line[0] == '_' && llen == 1) {
            y += 10;
            chars_left -= 2;
            continue;
        }

        char partial[DLGD_LINE_LEN];
        int show = (chars_left >= llen) ? llen : chars_left;
        memcpy(partial, line, show);
        partial[show] = '\0';

        sr_draw_text_shadow(px, W, H, 16, y, partial, text_col, shadow);
        chars_left -= llen + 1;
        y += 10;
    }

    /* Blinking cursor */
    if (!intro_done && (intro_timer / 15) % 2 == 0) {
        int cx = 16, cy = 20;
        int remain = intro_char_idx;
        for (int i = 0; i < line_count && remain > 0; i++) {
            const char *line = lines[i];
            int llen = (int)strlen(line);
            if (line[0] == '_' && llen == 1) { cy += 10; remain -= 2; continue; }
            int show = (remain >= llen) ? llen : remain;
            cx = 16 + show * 6;
            remain -= llen + 1;
            if (remain >= 0) { cy += 10; cx = 16; }
        }
        sr_draw_text_shadow(px, W, H, cx, cy, "_", text_col, shadow);
    }

    if (intro_done) {
        uint32_t blink = ((intro_timer / 30) % 2 == 0) ? text_col : dim_col;
        sr_draw_text_shadow(px, W, H, W/2 - 60, H - 20,
                            "PRESS SPACE TO CONTINUE", blink, shadow);
    } else {
        sr_draw_text_shadow(px, W, H, W - 90, H - 12,
                            "SPACE TO SKIP", dim_col, shadow);
    }
}

/* ── Beam teleport effect ──────────────────────────────────────── */

static void draw_beam_effect(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;

    beam_timer++;
    if (beam_timer == 1) sr_audio_play_sfx(&audio_sfx_teleporter);
    float t = (float)beam_timer / BEAM_DURATION; /* 0.0 to 1.0 */
    if (t > 1.0f) t = 1.0f;

    /* Dark background */
    for (int i = 0; i < W * H; i++)
        px[i] = 0xFF080810;

    /* Beam column parameters */
    int beam_cx = W / 2;       /* center X of beam */
    int beam_w = 24;           /* beam width */
    float beam_intensity;
    if (t < 0.15f) beam_intensity = t / 0.15f;
    else if (t > 0.85f) beam_intensity = (1.0f - t) / 0.15f;
    else beam_intensity = 1.0f;

    /* Draw main beam column */
    int bx0 = beam_cx - beam_w / 2;
    int bx1 = beam_cx + beam_w / 2;
    for (int ry = 0; ry < H; ry++) {
        for (int rx = bx0; rx < bx1 && rx < W; rx++) {
            if (rx < 0) continue;
            /* Distance from beam center for gradient */
            float dx = (float)(rx - beam_cx) / (beam_w / 2.0f);
            if (dx < 0) dx = -dx;
            float fade = (1.0f - dx) * beam_intensity;
            if (fade < 0) fade = 0;

            /* Cyan-white beam color */
            int br = (int)(100 * fade + 155 * fade * fade);
            int bg = (int)(200 * fade + 55 * fade * fade);
            int bb = (int)(255 * fade);
            if (br > 255) br = 255;
            if (bg > 255) bg = 255;
            if (bb > 255) bb = 255;

            /* Blend additively */
            uint32_t existing = px[ry * W + rx];
            int er = (existing >> 0) & 0xFF;
            int eg = (existing >> 8) & 0xFF;
            int eb = (existing >> 16) & 0xFF;
            er += br; if (er > 255) er = 255;
            eg += bg; if (eg > 255) eg = 255;
            eb += bb; if (eb > 255) eb = 255;
            px[ry * W + rx] = 0xFF000000 | (eb << 16) | (eg << 8) | er;
        }
    }

    /* Scanning line effect — horizontal bright band sweeping down */
    if (beam_intensity > 0.3f) {
        int scan_y = (int)(((beam_timer * 3) % H));
        for (int ry = scan_y; ry < scan_y + 3 && ry < H; ry++) {
            for (int rx = bx0 - 4; rx < bx1 + 4 && rx < W; rx++) {
                if (rx < 0 || ry < 0) continue;
                uint32_t existing = px[ry * W + rx];
                int er = (existing >> 0) & 0xFF;
                int eg = (existing >> 8) & 0xFF;
                int eb = (existing >> 16) & 0xFF;
                int add = (int)(120 * beam_intensity);
                er += add; if (er > 255) er = 255;
                eg += add; if (eg > 255) eg = 255;
                eb += add; if (eb > 255) eb = 255;
                px[ry * W + rx] = 0xFF000000 | (eb << 16) | (eg << 8) | er;
            }
        }
    }

    /* Sparkle particles */
    beam_rng_state = beam_timer * 7919 + 1;
    int num_sparkles = (int)(40 * beam_intensity);
    for (int s = 0; s < num_sparkles; s++) {
        int sx = beam_cx - beam_w + beam_rng() % (beam_w * 2);
        int sy = beam_rng() % H;
        if (sx < 0 || sx >= W || sy < 0 || sy >= H) continue;

        /* Sparkle brightness varies */
        float sparkle_bright = (float)(beam_rng() % 100) / 100.0f;
        sparkle_bright *= beam_intensity;

        /* Small sparkle (1-2 pixels) */
        int size = 1 + (beam_rng() % 2);
        for (int dy = 0; dy < size; dy++) {
            for (int dx = 0; dx < size; dx++) {
                int px_x = sx + dx, px_y = sy + dy;
                if (px_x >= W || px_y >= H) continue;
                uint32_t existing = px[px_y * W + px_x];
                int er = (existing >> 0) & 0xFF;
                int eg = (existing >> 8) & 0xFF;
                int eb = (existing >> 16) & 0xFF;
                /* White-cyan sparkles */
                int add_r = (int)(200 * sparkle_bright);
                int add_g = (int)(240 * sparkle_bright);
                int add_b = (int)(255 * sparkle_bright);
                er += add_r; if (er > 255) er = 255;
                eg += add_g; if (eg > 255) eg = 255;
                eb += add_b; if (eb > 255) eb = 255;
                px[px_y * W + px_x] = 0xFF000000 | (eb << 16) | (eg << 8) | er;
            }
        }
    }

    /* Rising particle trails along beam edges */
    for (int p = 0; p < 12; p++) {
        int edge = (p % 2 == 0) ? bx0 - 2 : bx1 + 1;
        int py = H - ((beam_timer * 4 + p * 37) % H);
        if (py < 0 || py >= H) continue;
        float trail_fade = beam_intensity * ((float)(beam_rng() % 60 + 40) / 100.0f);
        for (int dy = 0; dy < 4 && py + dy < H; dy++) {
            float df = trail_fade * (1.0f - dy / 4.0f);
            for (int dx = 0; dx < 2; dx++) {
                int px_x = edge + dx, px_y = py + dy;
                if (px_x < 0 || px_x >= W) continue;
                uint32_t existing = px[px_y * W + px_x];
                int er = (existing >> 0) & 0xFF;
                int eg = (existing >> 8) & 0xFF;
                int eb = (existing >> 16) & 0xFF;
                er += (int)(100 * df); if (er > 255) er = 255;
                eg += (int)(220 * df); if (eg > 255) eg = 255;
                eb += (int)(255 * df); if (eb > 255) eb = 255;
                px[px_y * W + px_x] = 0xFF000000 | (eb << 16) | (eg << 8) | er;
            }
        }
    }

    /* Text at top */
    uint32_t shadow = 0xFF000000;
    if (beam_timer > 10) {
        uint32_t text_col = ((beam_timer / 10) % 2 == 0) ? 0xFF44DDDD : 0xFF22AAAA;
        sr_draw_text_centered(px, W, H, H - 14, "ENERGIZING...", text_col, shadow);
    }

    /* Auto-transition when done */
    if (beam_timer >= BEAM_DURATION) {
        app_state = STATE_RUNNING;
    }
}

/* ── Mission summary screen ────────────────────────────────────── */

static void draw_mission_summary(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0D0D11;

    /* Title */
    sr_draw_text_centered(px, W, H, 16, "MISSION COMPLETE", 0xFF44CC44, shadow);

    /* Completion method */
    uint32_t method_col = g_summary.all_killed ? 0xFF44AACC : 0xFFCC8844;
    sr_draw_text_centered(px, W, H, 32, g_summary.completion_method, method_col, shadow);

    /* Divider */
    for (int rx = 60; rx < W - 60; rx++)
        if (44 < H) px[44 * W + rx] = 0xFF333355;

    /* Stats */
    int y = 54;
    char buf[64];

    if (g_summary.all_killed) {
        sr_draw_text_shadow(px, W, H, 40, y, "ALL CREATURES ELIMINATED", 0xFF44AACC, shadow);
        y += 12;
        sr_draw_text_shadow(px, W, H, 40, y, "MAXIMUM BIOMASS RECOVERED", 0xFF448844, shadow);
    } else {
        snprintf(buf, sizeof(buf), "TERMINALS DESTROYED: %d / %d",
                 g_summary.terminals_destroyed, g_summary.terminals_total);
        sr_draw_text_shadow(px, W, H, 40, y, buf, 0xFFCC8844, shadow);
        y += 12;
        sr_draw_text_shadow(px, W, H, 40, y, "SHIP SCAVENGED FOR PARTS", 0xFF888888, shadow);
    }

    /* Divider */
    y += 16;
    for (int rx = 60; rx < W - 60; rx++)
        if (y < H) px[y * W + rx] = 0xFF333355;
    y += 10;

    /* Rewards */
    sr_draw_text_shadow(px, W, H, 40, y, "REWARDS:", 0xFFCCCCCC, shadow);
    y += 14;

    /* Scrap */
    snprintf(buf, sizeof(buf), "SCRAP: +%d", g_summary.scrap_earned);
    uint32_t scrap_col = g_summary.all_killed ? 0xFF888888 : 0xFFEECC44;
    sr_draw_text_shadow(px, W, H, 60, y, buf, scrap_col, shadow);
    if (!g_summary.all_killed) {
        sr_draw_text_shadow(px, W, H, W - 120, y, "(BONUS)", 0xFF666644, shadow);
    }
    y += 12;

    /* Biomass */
    snprintf(buf, sizeof(buf), "BIOMASS: +%d", g_summary.biomass_earned);
    uint32_t bio_col = g_summary.all_killed ? 0xFF44CC88 : 0xFF888888;
    sr_draw_text_shadow(px, W, H, 60, y, buf, bio_col, shadow);
    if (g_summary.all_killed) {
        sr_draw_text_shadow(px, W, H, W - 120, y, "(BONUS)", 0xFF226644, shadow);
    }
    y += 20;

    /* Totals */
    snprintf(buf, sizeof(buf), "TOTAL SCRAP: %d    TOTAL BIOMASS: %d",
             player_scrap, player_biomass);
    sr_draw_text_shadow(px, W, H, 40, y, buf, 0xFF888888, shadow);

    /* Boss sample info */
    if (g_summary.is_boss) {
        y += 16;
        snprintf(buf, sizeof(buf), "BIOMASS SAMPLE %d / %d SECURED!", player_samples, SAMPLES_REQUIRED);
        sr_draw_text_shadow(px, W, H, 40, y, buf, 0xFF44CC44, shadow);
    }

    /* Continue prompt */
    uint32_t blink = ((beam_timer++ / 30) % 2 == 0) ? 0xFFCCCCCC : 0xFF666666;
    sr_draw_text_centered(px, W, H, H - 20, "PRESS SPACE TO CONTINUE", blink, shadow);
}

/* ── Ship-mode game initialization ──────────────────────────────── */

static int last_player_gx = -1, last_player_gy = -1;
/* current_combat_room and console_combat declared above save/load section */
static char dng_hud_msg[64];         /* temporary HUD message for dungeon scene */
static int  dng_hud_msg_timer = 0;   /* frames remaining to show message */
static int  console_confirm_gx = -1, console_confirm_gy = -1; /* pending console confirm */

/* Chest overlay state */
static bool chest_overlay_active = false;
static int  chest_choices[3];        /* 3 elemental card choices */
static int  chest_gx, chest_gy;     /* position of opened chest */

static void game_init_ship(void) {
    printf("[game_init_ship] Starting ship initialization\n");
    printf("[game_init_ship] current_floor=%d, seed_base=%u, grid=%dx%d\n",
           dng_state.current_floor, dng_state.seed_base, dng_state.grid_w, dng_state.grid_h);

    /* Try to load a hand-designed level file first */
    /* Check starmap node for a specific level file */
    const char *level_path = NULL;
    if (g_starmap.active && g_starmap.current_node >= 0 &&
        g_starmap.current_node < g_starmap.node_count) {
        const char *lf = g_starmap.nodes[g_starmap.current_node].level_file;
        if (lf[0]) {
            static char level_path_buf[128];
            snprintf(level_path_buf, sizeof(level_path_buf), "levels/%s", lf);
            level_path = level_path_buf;
            printf("[game_init_ship] Sector '%s' (node %d) -> level: %s\n",
                   g_starmap.nodes[g_starmap.current_node].name,
                   g_starmap.current_node, level_path);
        }
    }
    if (!level_path) level_path = "levels/sample_enemy_ship.json";
    if (lvl_file_exists(level_path)) {
        printf("[game_init_ship] Found %s, attempting load...\n", level_path);
        lvl_loaded lvl = lvl_load(level_path);
        printf("[game_init_ship] lvl_load result: valid=%d, is_hub=%d, num_floors=%d\n",
               lvl.valid, lvl.is_hub, lvl.num_floors);
        if (lvl.valid && !lvl.is_hub) {
            printf("[game_init_ship] Loading level from %s (%d floors)\n", level_path, lvl.num_floors);

            /* Populate ship state from JSON */
            lvl_load_ship(&current_ship, &lvl.json, lvl.root);
            printf("[game_init_ship] Ship: '%s' decks=%d rooms=%d officers=%d\n",
                   current_ship.name, current_ship.num_decks,
                   current_ship.room_count, current_ship.officer_count);
            dng_state.max_floors = current_ship.num_decks;
            for (int dk = 0; dk < current_ship.num_decks && dk < DNG_MAX_FLOORS; dk++) {
                dng_state.deck_room_counts[dk] = current_ship.deck_room_count[dk];
                printf("[game_init_ship]   deck %d: %d rooms (start=%d)\n",
                       dk, current_ship.deck_room_count[dk], current_ship.deck_room_start[dk]);
            }

            /* Load all floors directly from JSON */
            printf("[game_init_ship] Loading floors into dng_state...\n");
            lvl_load_all_floors(&lvl, dng_state.floors,
                                dng_state.floor_generated, DNG_MAX_FLOORS);
            printf("[game_init_ship] current_floor=%d, floor_generated[0]=%d\n",
                   dng_state.current_floor, dng_state.floor_generated[0]);
            if (dng_state.current_floor >= current_ship.num_decks) {
                printf("[game_init_ship] WARNING: current_floor %d >= num_decks %d, clamping to 0\n",
                       dng_state.current_floor, current_ship.num_decks);
                dng_state.current_floor = 0;
            }
            dng_state.dungeon = &dng_state.floors[dng_state.current_floor];

            /* Set grid size from first floor */
            dng_state.grid_w = dng_state.floors[0].w;
            dng_state.grid_h = dng_state.floors[0].h;

            /* JSON already contains enemies/consoles, but we still need to
               link room_ship_idx and place consoles for rooms that have subsystems */
            for (int deck = 0; deck < current_ship.num_decks && deck < DNG_MAX_FLOORS; deck++) {
                if (!dng_state.floor_generated[deck]) continue;
                sr_dungeon *dd = &dng_state.floors[deck];

                int start = current_ship.deck_room_start[deck];
                int count = current_ship.deck_room_count[deck];
                for (int r = 0; r < count && r < dd->room_count; r++) {
                    dd->room_ship_idx[r] = start + r;
                }

                /* Place consoles at room centers if not already placed by JSON */
                for (int r = 0; r < count && r < dd->room_count; r++) {
                    ship_room *rm = &current_ship.rooms[start + r];
                    if (rm->type == ROOM_CORRIDOR) continue;
                    int cx = dd->room_cx[r], cy = dd->room_cy[r];
                    if (cx >= 1 && cx <= dd->w && cy >= 1 && cy <= dd->h &&
                        dd->map[cy][cx] == 0 && dd->consoles[cy][cx] == 0) {
                        dd->consoles[cy][cx] = (uint8_t)rm->type;
                    }
                }
            }

            /* Remove invalid stairs — stairs only exist between adjacent floors */
            for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
                if (!dng_state.floor_generated[fl]) continue;
                sr_dungeon *fld = &dng_state.floors[fl];
                bool has_floor_below = (fl > 0 && dng_state.floor_generated[fl - 1]);
                bool has_floor_above = (fl + 1 < current_ship.num_decks &&
                                        fl + 1 < DNG_MAX_FLOORS &&
                                        dng_state.floor_generated[fl + 1]);
                if (!has_floor_below) {
                    if (fld->has_down)
                        printf("[stairs] Floor %d: removing down-stairs (no floor below)\n", fl);
                    fld->has_down = false;
                    fld->down_gx = -1;
                    fld->down_gy = -1;
                }
                if (!has_floor_above) {
                    if (fld->has_up)
                        printf("[stairs] Floor %d: removing up-stairs (no floor above)\n", fl);
                    fld->has_up = false;
                    fld->stairs_gx = -1;
                    fld->stairs_gy = -1;
                }
                printf("[stairs] Floor %d: has_up=%d up=(%d,%d) has_down=%d down=(%d,%d)\n",
                       fl, fld->has_up, fld->stairs_gx, fld->stairs_gy,
                       fld->has_down, fld->down_gx, fld->down_gy);
            }

            printf("[game_init_ship] Level file: %s\n", level_path);
            printf("[game_init_ship] Level loaded: %s (%d decks, %d rooms, %d officers)\n",
                   current_ship.name, current_ship.num_decks,
                   current_ship.room_count, current_ship.officer_count);
            printf("[game_init_ship] Dungeon ptr=%p, w=%d, h=%d, spawn=(%d,%d)\n",
                   (void*)dng_state.dungeon, dng_state.dungeon->w, dng_state.dungeon->h,
                   dng_state.dungeon->spawn_gx, dng_state.dungeon->spawn_gy);
            /* Initialize enemy AI entities for the starting floor */
            /* Upgrade tier 1 enemies to tier 2 on difficulty >= 2 */
            if (player_sector >= 2) {
                /* Mapping: Lurker→Stalker, Brute→Mauler, Spitter→AcidThrower, Hiveguard→Warden */
                static const uint8_t tier2_map[] = {
                    [ENEMY_LURKER+1]    = ENEMY_STALKER+1,
                    [ENEMY_BRUTE+1]     = ENEMY_MAULER+1,
                    [ENEMY_SPITTER+1]   = ENEMY_ACID_THROWER+1,
                    [ENEMY_HIVEGUARD+1] = ENEMY_WARDEN+1,
                };
                sr_dungeon *dd = dng_state.dungeon;
                for (int gy2 = 1; gy2 <= dd->h; gy2++)
                    for (int gx2 = 1; gx2 <= dd->w; gx2++) {
                        uint8_t a = dd->aliens[gy2][gx2];
                        if (a >= 1 && a <= ENEMY_HIVEGUARD+1)
                            dd->aliens[gy2][gx2] = tier2_map[a];
                    }
                printf("[game_init_ship] Upgraded enemies to tier 2 (sector %d)\n", player_sector);
                fflush(stdout);
            }

            /* Remove any enemies sitting on consoles */
            {
                sr_dungeon *dd = dng_state.dungeon;
                for (int gy2 = 1; gy2 <= dd->h; gy2++)
                    for (int gx2 = 1; gx2 <= dd->w; gx2++)
                        if (dd->aliens[gy2][gx2] != 0 && dd->consoles[gy2][gx2] != 0)
                            dd->aliens[gy2][gx2] = 0;
            }

            printf("[game_init_ship] Calling dng_enemies_init...\n");
            fflush(stdout);
            dng_enemies_init(dng_state.dungeon);
            printf("[game_init_ship] dng_enemies_init done, enemy_count=%d\n", dng_enemy_count);
            fflush(stdout);
            printf("[game_init_ship] Calling dng_spawn_hallway_enemies...\n");
            fflush(stdout);
            dng_spawn_hallway_enemies(dng_state.dungeon, dng_state.current_floor);
            printf("[game_init_ship] dng_spawn_hallway_enemies done, enemy_count=%d\n", dng_enemy_count);
            fflush(stdout);

            /* Place boss in reactor room if this is a boss mission */
            if (current_mission_is_boss) {
                int boss_type = ENEMY_BOSS_1;
                bool placed = false;
                /* Search all floors for reactor room */
                for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS && !placed; fl++) {
                    sr_dungeon *dd = &dng_state.floors[fl];
                    for (int r = 0; r < dd->room_count && !placed; r++) {
                        if (dd->consoles[dd->room_cy[r]][dd->room_cx[r]] != ROOM_REACTOR) continue;
                        int bx = dd->room_cx[r] + 1;
                        int by = dd->room_cy[r] + 1;
                        if (bx < 1 || bx > dd->w) bx = dd->room_cx[r];
                        if (by < 1 || by > dd->h) by = dd->room_cy[r];
                        dd->aliens[by][bx] = (uint8_t)(STEX_BOSS_FRAME_0 + 1);
                        snprintf(dd->alien_names[by][bx], 16, "%s",
                                 enemy_templates[boss_type].name);
                        printf("[BOSS] %s placed on floor %d, reactor room %d at gx=%d gy=%d\n",
                               enemy_templates[boss_type].name, fl, r, bx, by);
                        fflush(stdout);
                        placed = true;
                    }
                }
                if (!placed) {
                    printf("[BOSS] WARNING: No reactor room found! Placing in last room of last floor.\n");
                    fflush(stdout);
                    int fl = current_ship.num_decks - 1;
                    if (fl >= 0 && fl < DNG_MAX_FLOORS) {
                        sr_dungeon *dd = &dng_state.floors[fl];
                        int r = dd->room_count - 1;
                        if (r >= 0) {
                            int bx = dd->room_cx[r] + 1, by = dd->room_cy[r] + 1;
                            if (bx < 1 || bx > dd->w) bx = dd->room_cx[r];
                            if (by < 1 || by > dd->h) by = dd->room_cy[r];
                            dd->aliens[by][bx] = (uint8_t)(STEX_BOSS_FRAME_0 + 1);
                            snprintf(dd->alien_names[by][bx], 16, "%s",
                                     enemy_templates[boss_type].name);
                        }
                    }
                }
            }

            /* Re-init player at the JSON floor's spawn point */
            dng_player_init(&dng_state.player,
                            dng_state.dungeon->spawn_gx,
                            dng_state.dungeon->spawn_gy,
                            dng_state.dungeon->spawn_dir);
            printf("[game_init_ship] Player repositioned to (%d,%d) dir=%d\n",
                   dng_state.player.gx, dng_state.player.gy, dng_state.player.dir);
            fflush(stdout);
            printf("[game_init_ship] Ship init complete, returning to caller\n");
            fflush(stdout);
            return;
        }
    }

    /* Fallback: procedural generation */
    int difficulty = player_sector;
    uint32_t ship_seed = dng_state.seed_base + 9999;
    ship_generate(&current_ship, difficulty, ship_seed);
    dng_state.max_floors = current_ship.num_decks;
    for (int dk = 0; dk < current_ship.num_decks && dk < DNG_MAX_FLOORS; dk++)
        dng_state.deck_room_counts[dk] = current_ship.deck_room_count[dk];

    /* Regenerate all existing floors with correct stair flags and room count */
    for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
        if (!dng_state.floor_generated[fl]) continue;
        bool is_last = (fl >= current_ship.num_decks - 1);
        int deck_rooms = current_ship.deck_room_count[fl];
        dng_generate_ex(&dng_state.floors[fl], dng_state.grid_w, dng_state.grid_h,
                        fl > 0, !is_last,
                        dng_state.seed_base + (uint32_t)fl * 777, fl, deck_rooms);
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

    /* Initialize enemy AI entities from the current floor's alien grid */
    dng_enemies_init(dng_state.dungeon);
    dng_spawn_hallway_enemies(dng_state.dungeon, dng_state.current_floor);
}

/* ── Mission completion with boss/sample tracking ──────────────── */

/* Count remaining aliens across all floors */
static int count_remaining_aliens(void) {
    int count = 0;
    for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
        if (!dng_state.floor_generated[fl]) continue;
        sr_dungeon *dd = &dng_state.floors[fl];
        for (int gy = 1; gy <= dd->h; gy++)
            for (int gx = 1; gx <= dd->w; gx++)
                if (dd->aliens[gy][gx] != 0) count++;
    }
    return count;
}

static void mission_complete_return_to_hub(int base_reward, const char *msg, bool all_killed) {
    sr_audio_stop_enemyship_music();
    sr_audio_play_sfx(&audio_sfx_victory);
    /* Calculate dual rewards based on completion method */
    int scrap_reward, biomass_reward;
    if (all_killed) {
        /* Killed everything: more biomass, less scrap */
        biomass_reward = base_reward;
        scrap_reward = base_reward / 3;
    } else {
        /* Terminals destroyed: more scrap, less biomass */
        scrap_reward = base_reward;
        biomass_reward = base_reward / 3;
    }

    player_scrap += scrap_reward;
    player_biomass += biomass_reward;
    g_run_stats.scrap_earned += scrap_reward;
    g_run_stats.biomass_earned += biomass_reward;
    g_run_stats.sectors_visited++;

    /* Mark first mission complete (enables starmap, changes captain dialog) */
    /* No auto-heal — player must visit medbay */
    medbay_used = false; /* allow one medbay heal between missions */

    /* Populate mission summary */
    memset(&g_summary, 0, sizeof(g_summary));
    g_summary.terminals_destroyed = current_ship.terminals_destroyed;
    g_summary.terminals_total = current_ship.terminals_required;
    g_summary.scrap_earned = scrap_reward;
    g_summary.biomass_earned = biomass_reward;
    g_summary.all_killed = all_killed;
    g_summary.is_boss = current_mission_is_boss;
    snprintf(g_summary.completion_method, sizeof(g_summary.completion_method), "%s", msg);

    hub_generate(&g_hub);
    g_hub.mission_available = false;
    current_combat_room = -1;
    printf("[MISSION_COMPLETE] mission_available=%d\n", g_hub.mission_available);

    /* Boss sample collection */
    if (current_mission_is_boss) {
        player_samples++;
        current_mission_is_boss = false;
        current_map_boss_done = true;

        /* Demo ends after first boss — show epilogue */
        epilogue_is_win = true;
        intro_char_idx = 0;
        intro_timer = 0;
        intro_done = false;
        app_state = STATE_EPILOGUE;
        return;
    }

    app_state = STATE_MISSION_SUMMARY;
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
                current_ship.terminals_destroyed++;

                for (int o = 0; o < current_ship.officer_count; o++) {
                    if (current_ship.officers[o].room_idx == current_combat_room &&
                        current_ship.officers[o].alive) {
                        current_ship.officers[o].alive = false;
                        current_ship.officers[o].captured = true;
                    }
                }
                /* Check mission completion on console sabotage */
                ship_check_missions(&current_ship);
            }
        }
        console_combat = false;

        if (!combat.player_won) {
            /* Player died — game over, delete save, show loss epilogue */
            current_ship.boarding_active = false;
            game_delete_save();
            epilogue_is_win = false;
            intro_char_idx = 0;
            intro_timer = 0;
            intro_done = false;
            app_state = STATE_EPILOGUE;
            return;
        }

        /* Player ship never takes damage — destroyed check removed */

        /* Check if we just killed a boss — instant mission complete */
        if (combat.player_won && current_mission_is_boss) {
            bool boss_killed = false;
            for (int i = 0; i < combat.enemy_count; i++) {
                if (combat.enemies[i].type >= ENEMY_BOSS_1 &&
                    combat.enemies[i].type <= ENEMY_BOSS_3 &&
                    !combat.enemies[i].alive)
                    boss_killed = true;
            }
            if (boss_killed) {
                current_ship.boarding_active = false;
                int reward = 50 + player_sector * 15;
                mission_complete_return_to_hub(reward, "BOSS DEFEATED!", true);
                return;
            }
        }

        if (current_ship.enemy_ship_destroyed) {
            current_ship.boarding_active = false;
            int reward = 20 + player_sector * 10;
            mission_complete_return_to_hub(reward, "SHIP DESTROYED!", false);
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
                mission_complete_return_to_hub(reward, "ALL HOSTILES KILLED!", true);
                return;
            }
        }

        /* Check if primary mission is done (terminals destroyed) */
        if (current_ship.mission.completed && current_ship.boarding_active) {
            current_ship.boarding_active = false;
            int reward = 30 + player_sector * 10;
            for (int b = 0; b < current_ship.bonus_count; b++)
                if (current_ship.bonus_missions[b].completed) reward += 15;
            mission_complete_return_to_hub(reward, "TERMINALS DESTROYED!", false);
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
        /* Clear console confirm if player moved elsewhere */
        if (p->gx != console_confirm_gx || p->gy != console_confirm_gy) {
            console_confirm_gx = -1;
            console_confirm_gy = -1;
        }
        /* Auto-save on each step */
        game_save();

        /* Tick enemy AI — enemies move every 2 player steps */
        static int player_step_count = 0;
        player_step_count++;
        if (player_step_count >= 2) {
            player_step_count = 0;
            dng_enemies_tick(dng_state.dungeon, p->gx, p->gy);
        }

        uint8_t alien = dng_state.dungeon->aliens[p->gy][p->gx];
        if (alien != 0) {
            dng_state.dungeon->aliens[p->gy][p->gx] = 0;

            /* Track which ship room this combat is in */
            current_combat_room = -1;
            if (current_ship.initialized) {
                int local_room = dng_room_at(dng_state.dungeon, p->gx, p->gy);
                if (local_room >= 0 && local_room < dng_state.dungeon->room_count)
                    current_combat_room = dng_state.dungeon->room_ship_idx[local_room];

                /* ship_tick_turn(&current_ship); — ship simulation disabled */
            }

            combat_init(&combat, selected_class, dng_state.current_floor, alien);
            app_state = STATE_COMBAT;
            return;
        }

        /* Check for chest pickup */
        if (dng_state.dungeon->chests[p->gy][p->gx] != 0 && !chest_overlay_active) {
            chest_overlay_active = true;
            g_run_stats.chests_found++;
            chest_gx = p->gx;
            chest_gy = p->gy;
            /* Generate 3 unique elemental card choices */
            int elems[] = { CARD_ICE, CARD_ACID, CARD_FIRE, CARD_LIGHTNING };
            for (int i = 3; i > 0; i--) {
                int j = dng_rng_int(i + 1);
                int tmp = elems[i]; elems[i] = elems[j]; elems[j] = tmp;
            }
            for (int i = 0; i < 3; i++) chest_choices[i] = elems[i];
            goto skip_console; /* block further interaction */
        }

        /* Check for console interaction — sentinel defense combat */
        uint8_t con_type = dng_state.dungeon->consoles[p->gy][p->gx];
        if (con_type != 0 && current_ship.initialized && current_ship.boarding_active) {
            /* First time stepping on this console: bounce back and ask for confirm */
            if (console_confirm_gx != p->gx || console_confirm_gy != p->gy) {
                console_confirm_gx = p->gx;
                console_confirm_gy = p->gy;
                float mid_x = (p->gx - 0.5f) * DNG_CELL_SIZE;
                float mid_z = (p->gy - 0.5f) * DNG_CELL_SIZE;
                p->gx = prev_gx;
                p->gy = prev_gy;
                last_player_gx = prev_gx;
                last_player_gy = prev_gy;
                p->target_x = (p->gx - 0.5f) * DNG_CELL_SIZE;
                p->target_z = (p->gy - 0.5f) * DNG_CELL_SIZE;
                p->bounce_mid_x = mid_x;
                p->bounce_mid_z = mid_z;
                p->bounce_timer = 12;
                snprintf(dng_hud_msg, sizeof(dng_hud_msg), "ATTACK TERMINAL? STEP AGAIN");
                dng_hud_msg_timer = 120;
                goto skip_console;
            }
            console_confirm_gx = -1;
            console_confirm_gy = -1;
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

                    /* Terminal sentinels — hiveguards serve as robotic defenders */
                    int num_drones = 1;
                    int drone_type = ENEMY_HIVEGUARD;
                    switch (rm->type) {
                        case ROOM_BRIDGE:  num_drones = 2; break;
                        case ROOM_REACTOR: num_drones = 2; break;
                        case ROOM_WEAPONS: num_drones = 2; break;
                        case ROOM_ENGINES: num_drones = 1; break;
                        case ROOM_SHIELDS: num_drones = 1; break;
                        case ROOM_CARGO:   num_drones = 1; break;
                        default: break;
                    }

                    combat.enemy_count = num_drones;
                    for (int i = 0; i < num_drones; i++) {
                        const enemy_template *tmpl = &enemy_templates[drone_type];
                        combat.enemies[i].type = drone_type;
                        combat.enemies[i].hp = tmpl->hp_max;
                        combat.enemies[i].hp_max = tmpl->hp_max;
                        combat.enemies[i].attack_range = tmpl->attack_range;
                        combat.enemies[i].flash_timer = 0;
                        combat.enemies[i].alive = true;
                    }

                    /* ship_tick_turn(&current_ship); — ship simulation disabled */
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
                           int stex_idx, const char *name,
                           const char *line1, const char *line2,
                           const char *line3, const char *line4) {
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;
    uint32_t yellow = 0xFF00DDDD;
    uint32_t border = sel ? yellow : gray;
    combat_draw_rect_outline(px, W, H, bx, by, CLASS_BOX_W, CLASS_BOX_H, border);
    if (sel) combat_draw_rect_outline(px, W, H, bx+1, by+1, CLASS_BOX_W-2, CLASS_BOX_H-2, border);
    if (stextures[stex_idx].pixels)
        spr_draw_tex(px, W, H, &stextures[stex_idx], bx + 34, by + 4, 2);
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

    sr_draw_text_shadow(px, W, H, W/2 - 45, 8, "DRAKE'S VOID", white, shadow);
    sr_draw_text_shadow(px, W, H, W/2 - 55, 22, "SELECT YOUR CLASS", gray, shadow);

    /* 4 classes in a row */
    int gap = (W - CLASS_COUNT * CLASS_BOX_W) / (CLASS_COUNT + 1);
    int by = 40;

    /* Scout */
    {
        int bx = gap;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 0,
                       STEX_SCOUT, "SCOUT",
                       "HP: 18", "NIMBLE",
                       "SNIPER/SHOTGUN", "3 MOVE CARDS");
    }
    /* Marine */
    {
        int bx = gap * 2 + CLASS_BOX_W;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 1,
                       STEX_MARINE, "MARINE",
                       "HP: 30", "TOUGH",
                       "3 SHOOT", "4 SHIELD");
    }
    /* Engineer */
    {
        int bx = gap * 3 + CLASS_BOX_W * 2;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 2,
                       STEX_ENGINEER, "ENGINEER",
                       "HP: 26", "MELEE FOCUS",
                       "WELDER/CHNSAW", "UP CLOSE");
    }
    /* Scientist */
    {
        int bx = gap * 4 + CLASS_BOX_W * 3;
        draw_class_box(px, W, H, bx, by, class_select_cursor == 3,
                       STEX_SCIENTIST, "SCIENTIST",
                       "HP: 22", "PRECISION",
                       "LASER/DEFLECT", "STUN GUN");
    }

    /* Bottom bar: skip intro checkbox + START button */
    {
        int sy = H - 28;

        /* Skip intro checkbox */
        int sx = W/2 - 70;
        uint32_t box_col = skip_intro ? 0xFF44FF44 : 0xFF333333;
        combat_draw_rect(px, W, H, sx, sy, 8, 8, box_col);
        combat_draw_rect_outline(px, W, H, sx, sy, 8, 8, 0xFF888888);
        sr_draw_text_shadow(px, W, H, sx + 12, sy, "SKIP INTRO",
                            skip_intro ? 0xFF44FF44 : gray, shadow);

        /* START button */
        if (ui_button(px, W, H, W/2 + 20, sy - 2, 50, 12, "START",
                      0xFF223322, 0xFF334433, 0xFF446644)) {
            /* Trigger game start with currently selected class */
            selected_class = class_select_cursor;
            player_persist_init(selected_class);
            weakness_init((uint32_t)(time(NULL) ^ (selected_class * 31337)));
            player_scrap = 30;
            player_biomass = 0;
            memset(player_consumables, 0, sizeof(player_consumables));
            player_sector = 0;
            captain_briefing_page = 0;
            player_samples = 0;
            player_starmap = 0;
            current_map_boss_done = false;
            current_mission_is_boss = false;
            /* Reset starmap and run stats for new game */
            memset(&g_starmap, 0, sizeof(g_starmap));
            memset(&g_run_stats, 0, sizeof(g_run_stats));
            settings_save();
            if (skip_intro) {
                mission_briefed = true;
                mission_medbay_done = true;
                mission_armory_done = true;
                hub_generate(&g_hub);
                memset(&g_dialog, 0, sizeof(g_dialog));
                snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "CPT HARDEN");
                snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "GET TO THE TELEPORTER, SOLDIER.");
                snprintf(g_dialog.lines[1], DIALOG_LINE_LEN, "WE HAVE A DERELICT TO BOARD.");
                g_dialog.line_count = 2;
                g_dialog.pending_action = DIALOG_ACTION_NONE;
                g_dialog.active = true;
                app_state = STATE_SHIP_HUB;
            } else {
                mission_briefed = false;
                mission_medbay_done = false;
                mission_armory_done = false;
                medbay_used = false;
                intro_char_idx = 0;
                intro_timer = 0;
                intro_done = false;
                app_state = STATE_INTRO;
            }
        }
    }

}

/* ── Pause menu overlay ─────────────────────────────────────────── */

/* ── Starfield background (visible through windows) ─────────────── */

/* ── Starfield: fixed 3D points on a sphere, projected to screen ── */

#define STAR_COUNT 300
static float star_positions[STAR_COUNT * 3]; /* x, y, z on unit sphere */
static uint32_t star_colors[STAR_COUNT];
static bool stars_initialized = false;

static void stars_init(void) {
    /* Deterministic seed for reproducible star positions */
    uint32_t seed = 42;
    for (int i = 0; i < STAR_COUNT; i++) {
        /* Generate uniformly distributed points on unit sphere */
        seed = seed * 1103515245u + 12345u;
        float u = (float)(seed >> 8 & 0xFFFF) / 65535.0f;
        seed = seed * 1103515245u + 12345u;
        float v = (float)(seed >> 8 & 0xFFFF) / 65535.0f;

        float theta = u * 2.0f * 3.14159265f;
        float phi = acosf(2.0f * v - 1.0f);

        star_positions[i * 3 + 0] = sinf(phi) * cosf(theta);
        star_positions[i * 3 + 1] = sinf(phi) * sinf(theta);
        star_positions[i * 3 + 2] = cosf(phi);

        /* Random brightness */
        seed = seed * 1103515245u + 12345u;
        int brightness = 100 + (int)(seed >> 16 & 0x9F);
        if (brightness > 255) brightness = 255;
        star_colors[i] = 0xFF000000 | (uint32_t)(brightness << 16 | brightness << 8 | brightness);
    }
    stars_initialized = true;
}

static void draw_starfield(sr_framebuffer *fb_ptr, const dng_player *p) {
    if (!stars_initialized) stars_init();

    uint32_t *px = fb_ptr->color;
    int W = fb_ptr->width, H = fb_ptr->height;
    float angle = p->angle * 6.28318f;
    float ca = cosf(angle), sa = sinf(angle);

    float fov = 70.0f * 3.14159265f / 180.0f;
    float aspect = (float)W / (float)H;
    float f = 1.0f / tanf(fov * 0.5f);

    float star_radius = 30.0f; /* sphere radius (inside near/far clip) */

    for (int i = 0; i < STAR_COUNT; i++) {
        /* World position on sphere centered at camera */
        float wx = star_positions[i * 3 + 0] * star_radius;
        float wy = star_positions[i * 3 + 1] * star_radius;
        float wz = star_positions[i * 3 + 2] * star_radius;

        /* Transform to camera space (Y-axis rotation only, camera at origin) */
        float cx = ca * wx + sa * wz;    /* right component */
        float cy = wy;                    /* up component */
        float cz = -sa * wx + ca * wz;   /* forward component (negative = in front) */

        /* Behind camera? */
        if (cz >= -0.1f) continue;

        /* Project to screen */
        float inv_z = -1.0f / cz;
        float sx = (cx * f * inv_z / aspect + 1.0f) * 0.5f * W;
        float sy = (-cy * f * inv_z + 1.0f) * 0.5f * H;

        int ix = (int)sx, iy = (int)sy;
        if (ix < 0 || ix >= W || iy < 0 || iy >= H) continue;

        /* Shimmer: vary brightness per star using sin wave with unique phase */
        uint32_t base = star_colors[i] & 0xFF;
        float phase = (float)i * 1.7f + (float)dng_time * (1.5f + (i & 3) * 0.5f);
        float flicker = 0.75f + 0.25f * sinf(phase);
        int b = (int)(base * flicker);
        if (b > 255) b = 255;
        uint32_t col = 0xFF000000 | (uint32_t)(b << 16 | b << 8 | b);

        px[iy * W + ix] = col;
        if (ix + 1 < W) px[iy * W + ix + 1] = col;
        if (iy + 1 < H) px[(iy + 1) * W + ix] = col;
        if (ix + 1 < W && iy + 1 < H) px[(iy + 1) * W + ix + 1] = col;
    }
}

static void draw_pause_menu(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;

    /* Dim background */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        int r = ((c >> 0) & 0xFF) / 3;
        int g = ((c >> 8) & 0xFF) / 3;
        int b = ((c >> 16) & 0xFF) / 3;
        px[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }

    /* Panel */
    int pw = 140, ph = 80;
    int px0 = (W - pw) / 2, py0 = (H - ph) / 2;
    combat_draw_rect(px, W, H, px0, py0, pw, ph, 0xFF111122);
    combat_draw_rect_outline(px, W, H, px0, py0, pw, ph, 0xFF444466);

    sr_draw_text_shadow(px, W, H, px0 + pw/2 - 18, py0 + 4, "PAUSED", 0xFFCCCCFF, shadow);

    /* Volume slider */
    sr_draw_text_shadow(px, W, H, px0 + 8, py0 + 20, "VOLUME", 0xFF888888, shadow);
    int bar_x = px0 + 50, bar_y = py0 + 20, bar_w = 80, bar_h = 8;
    combat_draw_rect(px, W, H, bar_x, bar_y, bar_w, bar_h, 0xFF222233);
    int fill_w = (int)(settings_master_vol * bar_w);
    if (fill_w > 0)
        combat_draw_rect(px, W, H, bar_x, bar_y, fill_w, bar_h, 0xFF4466AA);
    combat_draw_rect_outline(px, W, H, bar_x, bar_y, bar_w, bar_h, 0xFF555577);

    /* Volume click detection */
    if (ui_mouse_clicked &&
        ui_click_x >= bar_x && ui_click_x <= bar_x + bar_w &&
        ui_click_y >= bar_y - 4 && ui_click_y <= bar_y + bar_h + 4) {
        float old_vol = settings_master_vol;
        settings_master_vol = (float)(ui_click_x - bar_x) / bar_w;
        if (settings_master_vol < 0) settings_master_vol = 0;
        if (settings_master_vol > 1) settings_master_vol = 1;
        audio_vol.ambient = settings_master_vol * 0.7f;
        audio_vol.footstep = settings_master_vol * 0.4f;
        /* Update all currently playing voices to reflect new master volume */
        if (old_vol > 0.001f) {
            float scale = settings_master_vol / old_vol;
            for (int i = 0; i < SR_AUDIO_MAX_VOICES; i++)
                if (audio_voices[i].active)
                    audio_voices[i].volume *= scale;
        } else {
            /* Was muted — restart at new volume */
            for (int i = 0; i < SR_AUDIO_MAX_VOICES; i++)
                if (audio_voices[i].active)
                    audio_voices[i].volume = settings_master_vol * 0.4f;
        }
    }

    /* Resume button */
    if (ui_button(px, W, H, px0 + 20, py0 + 38, pw - 40, 14, "RESUME",
                  0xFF222233, 0xFF333344, 0xFF444466)) {
        game_paused = false;
        settings_save();
    }

    /* Main menu button */
    if (ui_button(px, W, H, px0 + 20, py0 + 58, pw - 40, 14, "MAIN MENU",
                  0xFF332222, 0xFF443333, 0xFF664444)) {
        game_paused = false;
        settings_save();
        app_state = STATE_TITLE;
    }
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
    itextures[ITEX_HUB_FLOOR]    = sr_indexed_load("assets/indexed/hub_floor.idx");
    itextures[ITEX_HUB_CEILING]  = sr_indexed_load("assets/indexed/hub_ceiling.idx");
    itextures[ITEX_HUB_CORRIDOR] = sr_indexed_load("assets/indexed/hub_corridor_wall.idx");
    itextures[ITEX_WALL_A_WIN]   = sr_indexed_load("assets/indexed/wall_A_window.idx");
    itextures[ITEX_EXT_WALL]     = sr_indexed_load("assets/indexed/exterior_ship_wall.idx");
    itextures[ITEX_EXT_WINDOW]   = sr_indexed_load("assets/indexed/exerior_window.idx");
    itextures[ITEX_ALIEN_EXT]    = sr_indexed_load("assets/indexed/alien_exterior.idx");
    itextures[ITEX_ALIEN_EXT_WIN]= sr_indexed_load("assets/indexed/alien_exterior_window.idx");
    itextures[ITEX_ALIEN_WALL]   = sr_indexed_load("assets/indexed/alien_interior_wall.idx");
    itextures[ITEX_ALIEN_CORRIDOR]= sr_indexed_load("assets/indexed/alien_corridor.idx");
    itextures[ITEX_ALIEN_WIN]    = sr_indexed_load("assets/indexed/alien_interior_window.idx");

    stextures[STEX_LURKER]    = sr_texture_load("assets/sprites/lurker.png");
    stextures[STEX_BRUTE]     = sr_texture_load("assets/sprites/brute.png");
    stextures[STEX_SPITTER]   = sr_texture_load("assets/sprites/spitter.png");
    stextures[STEX_HIVEGUARD] = sr_texture_load("assets/sprites/hiveguard.png");
    /* Evolved tier 2 — load specific sprites, fall back to tier 1 */
    stextures[STEX_STALKER]      = sr_texture_load("assets/sprites/jaycook/By-Tor.png");
    if (!stextures[STEX_STALKER].pixels) stextures[STEX_STALKER] = sr_texture_load("assets/sprites/lurker.png");
    stextures[STEX_MAULER]       = sr_texture_load("assets/sprites/jaycook/Xenodragon.png");
    if (!stextures[STEX_MAULER].pixels) stextures[STEX_MAULER] = sr_texture_load("assets/sprites/brute.png");
    stextures[STEX_ACID_THROWER] = sr_texture_load("assets/sprites/jaycook/Owlien.png");
    if (!stextures[STEX_ACID_THROWER].pixels) stextures[STEX_ACID_THROWER] = sr_texture_load("assets/sprites/spitter.png");
    stextures[STEX_WARDEN]       = sr_texture_load("assets/sprites/jaycook/Dragon.png");
    if (!stextures[STEX_WARDEN].pixels) stextures[STEX_WARDEN] = sr_texture_load("assets/sprites/hiveguard.png");
    stextures[STEX_SCOUT]     = sr_texture_load("assets/sprites/scout.png");
    stextures[STEX_MARINE]    = sr_texture_load("assets/sprites/marine.png");
    stextures[STEX_ENGINEER]  = sr_texture_load("assets/sprites/engineer.png");
    stextures[STEX_SCIENTIST] = sr_texture_load("assets/sprites/scientist.png");
    stextures[STEX_CREW_CAPTAIN]       = sr_texture_load("assets/sprites/crew_captain.png");
    stextures[STEX_CREW_SERGEANT]      = sr_texture_load("assets/sprites/crew_sergeant.png");
    stextures[STEX_CREW_QUARTERMASTER] = sr_texture_load("assets/sprites/crew_quartermaster.png");
    stextures[STEX_CREW_PRIVATE]       = sr_texture_load("assets/sprites/crew_private.png");
    stextures[STEX_CREW_DOCTOR]        = sr_texture_load("assets/sprites/crew_doctor.png");
    stextures[STEX_CREW_BYTOR]        = sr_texture_load("assets/sprites/jaycook/Fireman.png");
    if (!stextures[STEX_CREW_BYTOR].pixels) /* fallback until sprite exists */
        stextures[STEX_CREW_BYTOR] = sr_texture_load("assets/sprites/crew_private.png");
    stextures[STEX_ICON_ICE]           = sr_texture_load("assets/sprites/icon_ice.png");
    stextures[STEX_ICON_ACID]          = sr_texture_load("assets/sprites/icon_acid.png");
    stextures[STEX_ICON_FIRE]          = sr_texture_load("assets/sprites/icon_fire.png");
    stextures[STEX_ICON_LIGHTNING]     = sr_texture_load("assets/sprites/icon_lightning.png");
    stextures[STEX_LOOT_CHEST]         = sr_texture_load("assets/sprites/loot_chest.png");
    /* Animated boss frames */
    stextures[STEX_BOSS_FRAME_0]       = sr_texture_load("assets/sprites/final_boss/astrozom_side00.png");
    stextures[STEX_BOSS_FRAME_1]       = sr_texture_load("assets/sprites/final_boss/astrozom_side01.png");
    stextures[STEX_BOSS_FRAME_2]       = sr_texture_load("assets/sprites/final_boss/astrozom_side02.png");

    /* Map enemy types to sprite texture indices (-1 = use raw index) */
    memset(enemy_to_stex, -1, sizeof(enemy_to_stex));
    enemy_to_stex[ENEMY_LURKER]       = STEX_LURKER;
    enemy_to_stex[ENEMY_BRUTE]        = STEX_BRUTE;
    enemy_to_stex[ENEMY_SPITTER]      = STEX_SPITTER;
    enemy_to_stex[ENEMY_HIVEGUARD]    = STEX_HIVEGUARD;
    enemy_to_stex[ENEMY_STALKER]      = STEX_STALKER;
    enemy_to_stex[ENEMY_MAULER]       = STEX_MAULER;
    enemy_to_stex[ENEMY_ACID_THROWER] = STEX_ACID_THROWER;
    enemy_to_stex[ENEMY_WARDEN]       = STEX_WARDEN;
    enemy_to_stex[ENEMY_BOSS_1]       = STEX_BOSS_FRAME_0;
    enemy_to_stex[ENEMY_BOSS_2]       = STEX_BOSS_FRAME_0;
    enemy_to_stex[ENEMY_BOSS_3]       = STEX_BOSS_FRAME_0;
    enemy_to_stex_init = true;

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

    /* Set window icon from Fireman sprite */
    if (stextures[STEX_CREW_BYTOR].pixels) {
        sr_texture *icon_tex = &stextures[STEX_CREW_BYTOR];
        sapp_set_icon(&(sapp_icon_desc){
            .images[0] = {
                .width = icon_tex->width,
                .height = icon_tex->height,
                .pixels = { .ptr = icon_tex->pixels,
                             .size = (size_t)(icon_tex->width * icon_tex->height * 4) },
            },
        });
    }

    dng_load_config();
    hub_load_config();
    enemy_load_config();
    dlgd_load();

    /* Load game config (debug mode etc.) */
    {
        sr_config gcfg = sr_config_load("config/game_config.yaml");
        if (gcfg.count > 0) {
            debug_mode = (int)sr_config_float(&gcfg, "debug.enabled", 0) != 0;
            if (debug_mode) printf("[game] DEBUG MODE ENABLED\n");

            /* Enemy ship position per size */
            enemy_ship_small.x_off      = sr_config_float(&gcfg, "enemy_ship_small.x_offset", 0.0f);
            enemy_ship_small.y_off      = sr_config_float(&gcfg, "enemy_ship_small.y_offset", -2.0f);
            enemy_ship_small.z_off      = sr_config_float(&gcfg, "enemy_ship_small.z_offset", -30.0f);
            enemy_ship_small.hover_amp  = sr_config_float(&gcfg, "enemy_ship_small.hover_amplitude", 0.3f);
            enemy_ship_small.hover_speed= sr_config_float(&gcfg, "enemy_ship_small.hover_speed", 1.0f);

            enemy_ship_medium.x_off      = sr_config_float(&gcfg, "enemy_ship_medium.x_offset", 0.0f);
            enemy_ship_medium.y_off      = sr_config_float(&gcfg, "enemy_ship_medium.y_offset", -12.0f);
            enemy_ship_medium.z_off      = sr_config_float(&gcfg, "enemy_ship_medium.z_offset", -60.0f);
            enemy_ship_medium.hover_amp  = sr_config_float(&gcfg, "enemy_ship_medium.hover_amplitude", 0.2f);
            enemy_ship_medium.hover_speed= sr_config_float(&gcfg, "enemy_ship_medium.hover_speed", 0.4f);

            enemy_ship_large.x_off      = sr_config_float(&gcfg, "enemy_ship_large.x_offset", 0.0f);
            enemy_ship_large.y_off      = sr_config_float(&gcfg, "enemy_ship_large.y_offset", -15.0f);
            enemy_ship_large.z_off      = sr_config_float(&gcfg, "enemy_ship_large.z_offset", -120.0f);
            enemy_ship_large.hover_amp  = sr_config_float(&gcfg, "enemy_ship_large.hover_amplitude", 0.15f);
            enemy_ship_large.hover_speed= sr_config_float(&gcfg, "enemy_ship_large.hover_speed", 0.3f);

            /* Hub ship remote configs (per enemy ship size) */
            hub_from_small.x_off       = sr_config_float(&gcfg, "hub_from_small.x_offset", 0.0f);
            hub_from_small.y_off       = sr_config_float(&gcfg, "hub_from_small.y_offset", -2.0f);
            hub_from_small.z_off       = sr_config_float(&gcfg, "hub_from_small.z_offset", 30.0f);
            hub_from_small.hover_amp   = sr_config_float(&gcfg, "hub_from_small.hover_amplitude", 0.2f);
            hub_from_small.hover_speed = sr_config_float(&gcfg, "hub_from_small.hover_speed", 0.8f);

            hub_from_medium.x_off       = sr_config_float(&gcfg, "hub_from_medium.x_offset", 0.0f);
            hub_from_medium.y_off       = sr_config_float(&gcfg, "hub_from_medium.y_offset", -2.0f);
            hub_from_medium.z_off       = sr_config_float(&gcfg, "hub_from_medium.z_offset", 50.0f);
            hub_from_medium.hover_amp   = sr_config_float(&gcfg, "hub_from_medium.hover_amplitude", 0.2f);
            hub_from_medium.hover_speed = sr_config_float(&gcfg, "hub_from_medium.hover_speed", 0.8f);

            hub_from_large.x_off       = sr_config_float(&gcfg, "hub_from_large.x_offset", 0.0f);
            hub_from_large.y_off       = sr_config_float(&gcfg, "hub_from_large.y_offset", -2.0f);
            hub_from_large.z_off       = sr_config_float(&gcfg, "hub_from_large.z_offset", 80.0f);
            hub_from_large.hover_amp   = sr_config_float(&gcfg, "hub_from_large.hover_amplitude", 0.15f);
            hub_from_large.hover_speed = sr_config_float(&gcfg, "hub_from_large.hover_speed", 0.6f);

            /* Movement mode */
            dng_instant_step = (int)sr_config_float(&gcfg, "movement.instant_step", 0) != 0;
            if (dng_instant_step) printf("[game] INSTANT STEP MODE ENABLED\n");

            sr_config_free(&gcfg);
        }
    }
    sr_audio_init();
    settings_load();
    audio_vol.ambient = settings_master_vol * 0.7f;
    audio_vol.footstep = settings_master_vol * 0.4f;

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    save_exists = game_has_save();
    printf("Space Hulks initialized (%dx%d @ %dfps)\n", FB_WIDTH, FB_HEIGHT, TARGET_FPS);
}

static void frame(void) {
    double dt = sapp_frame_duration();
    time_acc += dt;
    dng_time += dt;
    frame_counter++;

    fps_timer += dt;
    fps_frame_count++;
    if (fps_timer >= 1.0) {
        fps_display = fps_frame_count;
        fps_frame_count = 0;
        fps_timer -= 1.0;
    }

    /* ── Audio state transitions ────────────────────────────── */
    {
        static int prev_app_state = -1;
        if (app_state != prev_app_state) {
            /* Stop enemy ship music when leaving dungeon */
            if (prev_app_state == STATE_RUNNING || prev_app_state == STATE_COMBAT)
                sr_audio_stop_enemyship_music();
            /* Hub ambient */
            if (app_state == STATE_SHIP_HUB)
                sr_audio_start_hub_ambient();
            else if (prev_app_state == STATE_SHIP_HUB && app_state != STATE_SHOP && app_state != STATE_DIALOG && app_state != STATE_STARMAP)
                sr_audio_stop_hub_ambient();
            prev_app_state = app_state;
        }
    }
    sr_audio_update((float)dt);

    /* ── CPU rasterize ───────────────────────────────────────── */
    sr_stats_reset();
    sr_framebuffer_clear(&fb, 0xFF000000, 1.0f);

    if (app_state == STATE_TITLE) {
        draw_title_screen(&fb);
        /* Handle continue button click (signaled from ui_button in draw) */
        if (title_cursor == 99) {
            title_cursor = 1;
            printf("[title] Continue clicked, save_exists=%d\n", save_exists);
            if (save_exists && game_load()) {
                last_player_gx = dng_state.player.gx;
                last_player_gy = dng_state.player.gy;
                printf("[title] Game loaded, state=%d\n", app_state);
            } else {
                printf("[title] Load failed or no save\n");
            }
        }
    } else if (app_state == STATE_INTRO) {
        draw_intro_screen(&fb);
    } else if (app_state == STATE_RUN_STATS) {
        draw_run_stats(&fb);
    } else if (app_state == STATE_EPILOGUE) {
        draw_epilogue_screen(&fb);
    } else if (app_state == STATE_CLASS_SELECT) {
        draw_class_select(&fb);
    } else if (app_state == STATE_COMBAT) {
        combat_update(&combat);
        /* Auto-save at the start of each player turn */
        {
            static int last_saved_turn = -1;
            if (combat.phase == CPHASE_PLAYER_TURN &&
                combat.turn != last_saved_turn) {
                last_saved_turn = combat.turn;
                game_save();
            }
            if (combat.combat_over) last_saved_turn = -1;
        }
        draw_combat_scene(&fb);
    } else if (app_state == STATE_SHIP_HUB) {
        dng_player_update(&g_hub.player);
        draw_starfield(&fb, &g_hub.player);
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
        if (g_kit.active)
            draw_kit_display(fb.color, fb.width, fb.height);
    } else if (app_state == STATE_BEAM) {
        draw_beam_effect(&fb);
    } else if (app_state == STATE_MISSION_SUMMARY) {
        draw_mission_summary(&fb);
    } else if (app_state == STATE_SHOP) {
        draw_shop(fb.color, fb.width, fb.height);
    } else if (app_state == STATE_STARMAP) {
        draw_starmap(fb.color, fb.width, fb.height);
    } else {
        sr_mat4 vp;
        (void)vp;
        sr_fog_disable();
        dng_alien_exterior = true; /* enemy ship uses alien exterior textures */

        {
            static bool _dng_first_frame = true;
            if (_dng_first_frame) {
                printf("[dungeon_render] First frame: dungeon=%p w=%d h=%d player=(%d,%d)\n",
                       (void*)dng_state.dungeon, dng_state.dungeon ? dng_state.dungeon->w : -1,
                       dng_state.dungeon ? dng_state.dungeon->h : -1,
                       dng_state.player.gx, dng_state.player.gy);
                fflush(stdout);
                _dng_first_frame = false;
            }
        }

        /* Update dungeon game state */
        if (dng_play_state == DNG_STATE_CLIMBING) {
            if (dng_update_climb(&dng_state)) {
                dng_play_state = DNG_STATE_PLAYING;
                /* Re-init enemy entities for the new floor */
                dng_enemies_init(dng_state.dungeon);
                dng_spawn_hallway_enemies(dng_state.dungeon, dng_state.current_floor);
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

        dng_enemies_lerp_update((float)dt);
        sr_audio_start_enemyship_music();
        draw_starfield(&fb, &dng_state.player);

        /* Render hub ship exterior (visible south through windows) */
        if (0 && g_hub.initialized && g_hub.dungeon.w > 0) { /* DISABLED for crash debugging */
            dng_player *ep = &dng_state.player;
            float cam_angle = ep->angle * 6.28318f;
            float ca_cos = cosf(cam_angle), ca_sin = sinf(cam_angle);
            sr_vec3 eye = { ep->x, ep->y, ep->z };
            sr_vec3 fwd = { ca_sin, 0, -ca_cos };
            sr_vec3 target2 = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
            sr_vec3 up = { 0, 1, 0 };
            sr_mat4 view2 = sr_mat4_lookat(eye, target2, up);
            sr_mat4 proj2 = sr_mat4_perspective(
                70.0f * 3.14159f / 180.0f,
                (float)fb.width / (float)fb.height,
                0.05f, 500.0f);
            sr_mat4 remote_mvp2 = sr_mat4_mul(proj2, view2);

            sr_dungeon *hub_d = &g_hub.dungeon;
            sr_dungeon *cur_d = dng_state.dungeon;
            /* Select config based on current enemy ship size */
            enemy_ship_cfg *hcfg = (cur_d->w >= 80) ? &hub_from_large
                                 : (cur_d->w >= 40) ? &hub_from_medium
                                 : &hub_from_small;
            float center_x = -(hub_d->w * DNG_CELL_SIZE) * 0.5f + (cur_d->w * DNG_CELL_SIZE) * 0.5f;
            float hover = sinf((float)dng_time * hcfg->hover_speed) * hcfg->hover_amp;
            float hox = center_x + hcfg->x_off;
            float hoy = hcfg->y_off + hover;
            float hoz = hcfg->z_off;
            sr_set_pixel_light_fn(NULL);
            {
                static bool _hub_remote_logged = false;
                if (!_hub_remote_logged) {
                    printf("[dungeon_render] Hub remote: hub_d=%p w=%d h=%d offset=(%.1f,%.1f,%.1f)\n",
                           (void*)hub_d, hub_d->w, hub_d->h, hox, hoy, hoz);
                    fflush(stdout);
                    _hub_remote_logged = true;
                }
            }
            printf("[dungeon_render] draw_remote_ship_interior...\n"); fflush(stdout);
            draw_remote_ship_interior(&fb, &remote_mvp2, hub_d, hox, hoy, hoz, false);
            printf("[dungeon_render] draw_remote_ship_exterior...\n"); fflush(stdout);
            draw_remote_ship_exterior(&fb, &remote_mvp2, hub_d, hox, hoy, hoz, false);
            printf("[dungeon_render] hub remote done\n"); fflush(stdout);
        }

        /* Set alien ship textures: no pillars, alien walls, hub floor/ceiling reused */
        dng_wall_texture = ITEX_ALIEN_CORRIDOR;
        dng_room_wall_texture = ITEX_ALIEN_WALL;
        dng_floor_texture = ITEX_HUB_FLOOR;
        dng_ceiling_texture = ITEX_HUB_CEILING;
        dng_window_texture = ITEX_ALIEN_WIN;
        dng_skip_pillars = true;

        /* Render ALL generated floors (current + others at Y offsets) */
        {
            float floor_height = DNG_CELL_SIZE;
            sr_dungeon *save_dng = dng_state.dungeon;
            float save_y = dng_state.player.y;
            int cur = dng_state.current_floor;

            static bool _floors_logged = false;
            if (!_floors_logged) {
                printf("[multi_floor] Rendering %d floors, current=%d\n", dng_state.max_floors, cur);
                for (int fl = 0; fl < dng_state.max_floors && fl < DNG_MAX_FLOORS; fl++) {
                    if (!dng_state.floor_generated[fl]) continue;
                    sr_dungeon *fd = &dng_state.floors[fl];
                    printf("[multi_floor] Floor %d: %dx%d has_up=%d up=(%d,%d) dir=%d has_down=%d down=(%d,%d) dir=%d\n",
                           fl, fd->w, fd->h,
                           fd->has_up, fd->stairs_gx, fd->stairs_gy, fd->stairs_dir,
                           fd->has_down, fd->down_gx, fd->down_gy, fd->down_dir);
                }
                fflush(stdout);
                _floors_logged = true;
            }

            for (int fl = 0; fl < dng_state.max_floors && fl < DNG_MAX_FLOORS; fl++) {
                if (!dng_state.floor_generated[fl]) continue;
                dng_state.dungeon = &dng_state.floors[fl];
                dng_state.player.y = save_y - (fl - cur) * floor_height;
                dng_hull_computed = false;
                dng_skip_stairs = (fl != cur);
                /* Skip interior floor/ceiling that overlaps with adjacent floor */
                dng_skip_floor = (fl > cur);   /* upper floor: skip bottom plate */
                dng_skip_ceiling = (fl < cur); /* lower floor: skip top plate */
                /* Skip exterior roof/bottom between adjacent floors */
                bool has_floor_above = (fl + 1 < dng_state.max_floors && dng_state.floor_generated[fl + 1]);
                bool has_floor_below = (fl > 0 && dng_state.floor_generated[fl - 1]);
                dng_skip_roof = has_floor_above;
                dng_skip_bottom = has_floor_below;
                draw_dungeon_scene(&fb, &vp);
            }
            dng_skip_stairs = false;
            dng_skip_floor = false;
            dng_skip_ceiling = false;
            dng_skip_roof = false;
            dng_skip_bottom = false;

            dng_state.dungeon = save_dng;
            dng_state.player.y = save_y;
            dng_hull_computed = false;
        }

        /* Reset to defaults */
        dng_wall_texture = -1;
        dng_room_wall_texture = -1;
        dng_floor_texture = -1;
        dng_ceiling_texture = -1;
        dng_window_texture = -1;
        dng_skip_pillars = false;

        draw_dungeon_minimap(&fb);

        /* Ship HUD overlay (ship simulation disabled) */
        if (current_ship.initialized && current_ship.boarding_active) {
            /* draw_ship_hud(fb.color, fb.width, fb.height, &current_ship); */

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

            /* Floor/terminal/enemy info below minimap */
            {
                int map_scale = 2;
                int map_bottom = 28 + dng_state.dungeon->h * map_scale + 4;
                int hx = fb.width - dng_state.dungeon->w * map_scale - 48;
                int hy = map_bottom;
                uint32_t dim = 0xFF888888;
                uint32_t shadow = 0xFF000000;

                char floorbuf[32];
                snprintf(floorbuf, sizeof(floorbuf), "FLOOR %d/%d",
                         dng_state.current_floor + 1, current_ship.num_decks);
                sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                    hx, hy, floorbuf, dim, shadow);
                hy += 10;

                /* Count total terminals across all generated floors */
                int total_terminals = 0;
                for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
                    if (!dng_state.floor_generated[fl]) continue;
                    sr_dungeon *dd = &dng_state.floors[fl];
                    for (int gy2 = 1; gy2 <= dd->h; gy2++)
                        for (int gx2 = 1; gx2 <= dd->w; gx2++)
                            if (dd->consoles[gy2][gx2] != 0) total_terminals++;
                }
                total_terminals += current_ship.terminals_destroyed; /* add already destroyed */

                char termbuf[32];
                snprintf(termbuf, sizeof(termbuf), "TERMINALS %d/%d",
                         current_ship.terminals_destroyed, total_terminals);
                sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                    hx, hy, termbuf, dim, shadow);
                hy += 10;

                /* Enemies remaining across all floors */
                int enemies_alive = 0;
                for (int fl = 0; fl < current_ship.num_decks && fl < DNG_MAX_FLOORS; fl++) {
                    if (!dng_state.floor_generated[fl]) continue;
                    sr_dungeon *dd = &dng_state.floors[fl];
                    for (int gy2 = 1; gy2 <= dd->h; gy2++)
                        for (int gx2 = 1; gx2 <= dd->w; gx2++)
                            if (dd->aliens[gy2][gx2] != 0) enemies_alive++;
                }
                char enembuf[32];
                snprintf(enembuf, sizeof(enembuf), "HOSTILES %d", enemies_alive);
                sr_draw_text_shadow(fb.color, fb.width, fb.height,
                                    hx, hy, enembuf, dim, shadow);
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

        /* Chest overlay */
        if (chest_overlay_active) {
            int W = fb.width, H = fb.height;
            uint32_t *px = fb.color;
            uint32_t shadow = 0xFF000000;
            /* Darken */
            for (int i = 0; i < W * H; i++) {
                uint32_t c = px[i];
                px[i] = 0xFF000000 | ((((c>>16)&0xFF)/3)<<16) |
                        ((((c>>8)&0xFF)/3)<<8) | (((c)&0xFF)/3);
            }
            sr_draw_text_centered(px, W, H, 20, "CHEST FOUND!", 0xFF44CC44, shadow);
            sr_draw_text_centered(px, W, H, 34, "PICK AN ELEMENTAL CARD", 0xFFCCCCCC, shadow);
            /* 3 card choices */
            int cw = 72, ch = 80, cgap = 12;
            int ctotal = 3 * (cw + cgap) - cgap;
            int cx0 = (W - ctotal) / 2, cy0 = 50;
            for (int i = 0; i < 3; i++) {
                int cx = cx0 + i * (cw + cgap);
                combat_draw_card_content(px, W, H, cx, cy0, cw, ch,
                                         chest_choices[i], false, shadow, -1);
            }
            /* SKIP button */
            int skip_x = (W - 50) / 2, skip_y = cy0 + ch + 14;
            combat_draw_rect(px, W, H, skip_x, skip_y, 50, 14, 0xFF222222);
            combat_draw_rect_outline(px, W, H, skip_x, skip_y, 50, 14, 0xFF666666);
            sr_draw_text_shadow(px, W, H, skip_x + 12, skip_y + 3, "SKIP", 0xFF888888, shadow);
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

    /* Hamburger menu button (top-right, for mobile) */
    if (!game_paused && (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB || app_state == STATE_COMBAT)) {
        int hb_x = fb.width - 16, hb_y = 2, hb_sz = 12;
        uint32_t *px = fb.color;
        int W = fb.width, H = fb.height;
        /* Draw three horizontal lines */
        for (int row = 0; row < 3; row++) {
            int ly = hb_y + 2 + row * 4;
            for (int lx = hb_x + 2; lx < hb_x + hb_sz - 2; lx++)
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = 0xFFAAAAAA;
        }
        /* Click detection */
        if (ui_mouse_clicked &&
            ui_click_x >= hb_x && ui_click_x <= hb_x + hb_sz &&
            ui_click_y >= hb_y && ui_click_y <= hb_y + hb_sz) {
            game_paused = true;
        }
    }

    /* Pause menu overlay (drawn on top of everything) */
    if (game_paused) draw_pause_menu(&fb);

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
    sr_audio_shutdown();
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

    if (game_paused) return; /* pause menu handles its own clicks via ui_button */

    if (app_state == STATE_MISSION_SUMMARY) {
        app_state = STATE_SHIP_HUB;
        game_save();
        return;
    }

    if (app_state == STATE_TITLE) {
        /* Buttons handled by ui_button in draw_title_screen */
        return;
    }

    if (app_state == STATE_CLASS_SELECT) {
        /* Skip intro checkbox */
        {
            int sx = FB_WIDTH/2 - 70;
            int sy = FB_HEIGHT - 28;
            if (fx >= sx && fx <= sx + 80 && fy >= sy && fy <= sy + 10) {
                skip_intro = !skip_intro;
                settings_save();
                return;
            }
        }
        /* Click on class box to select (START button handles game start) */
        int gap = (FB_WIDTH - CLASS_COUNT * CLASS_BOX_W) / (CLASS_COUNT + 1);
        int by = 40;
        for (int ci = 0; ci < CLASS_COUNT; ci++) {
            int bx = gap * (ci + 1) + CLASS_BOX_W * ci;
            if (fx >= bx && fx <= bx + CLASS_BOX_W && fy >= by && fy <= by + CLASS_BOX_H) {
                class_select_cursor = ci;
                return;
            }
        }
        return;
    }

    if (app_state == STATE_INTRO) {
        /* Click to skip or advance */
        if (intro_done) {
            hub_generate(&g_hub);
            app_state = STATE_SHIP_HUB;
        } else {
            intro_done = true;
            intro_char_idx = 9999;
        }
        return;
    }

    if (app_state == STATE_RUN_STATS) {
        app_state = STATE_TITLE;
        save_exists = game_has_save();
        return;
    }

    if (app_state == STATE_EPILOGUE) {
        if (intro_done) {
            app_state = STATE_RUN_STATS;
        } else {
            intro_done = true;
            intro_char_idx = 9999;
        }
        return;
    }

    if (app_state == STATE_SHIP_HUB) {
        if (deck_view_active) {
            /* Close button handled by ui_button in draw_deck_viewer via ui_mouse_clicked */
            return;
        }
        if (g_kit.active) {
            /* Kit display overlay — handle card clicks and close */
            int kit_bx = 10, kit_by = 10;
            int cw = 50, ch = 66, pad = 6;
            int cols = 7;
            if (g_kit.detail_idx >= 0) {
                /* Detail view — click anywhere to close detail */
                g_kit.detail_idx = -1;
            } else {
                /* Check CLOSE button */
                int close_bx = FB_WIDTH - 60, close_by = FB_HEIGHT - 18;
                if (fx >= close_bx && fx < close_bx + 50 && fy >= close_by && fy < close_by + 14) {
                    g_kit.active = false;
                } else {
                    /* Check card clicks */
                    for (int i = 0; i < g_kit.card_count; i++) {
                        int col = i % cols, row = i / cols;
                        int cx = kit_bx + col * (cw + pad);
                        int cy = kit_by + 14 + row * (ch + pad);
                        if (fx >= cx && fx < cx + cw && fy >= cy && fy < cy + ch) {
                            g_kit.detail_idx = i;
                            break;
                        }
                    }
                }
            }
            return;
        }
        if (g_dialog.active) {
            if (g_dialog.confirm_mode) {
                /* Confirm dialog — advance teletype first, then show YES/NO */
                if (!g_dialog.tt_all_done) {
                    dialog_teletype_advance();
                    return;
                }
                int H = FB_HEIGHT;
                int bx = 20, bw = FB_WIDTH - 40, bh = 52;
                int by = H - 66;
                int tab_y = by + bh;
                /* YES button area */
                if (fx >= bx + bw - 140 && fx < bx + bw - 80 &&
                    fy >= tab_y && fy < tab_y + 14) {
                    int action = g_dialog.pending_action;
                    g_dialog.active = false;
                    g_dialog.confirm_mode = false;
                    if (action == DIALOG_ACTION_TELEPORT_GO && g_hub.mission_available) {
                        printf("[teleport] Initiating teleport, sector=%d\n", player_sector);
                        fflush(stdout);
                        { int sw, sh; game_ship_size(player_sector, &sw, &sh);
                        printf("[teleport] Ship size: %dx%d\n", sw, sh);
                        fflush(stdout);
                        dng_game_init_sized(&dng_state, sw, sh); }
                        printf("[teleport] dng_game_init_sized done, calling game_init_ship\n");
                        fflush(stdout);
                        game_init_ship();
                        printf("[teleport] game_init_ship done\n");
                        fflush(stdout);
                        dng_initialized = true;
                        dng_hull_computed = false;
                        last_player_gx = dng_state.player.gx;
                        last_player_gy = dng_state.player.gy;
                        printf("[teleport] Player at (%d,%d), entering beam state\n",
                               last_player_gx, last_player_gy);
                        fflush(stdout);
                        g_hub.mission_available = false;
                        beam_timer = 0;
                        app_state = STATE_BEAM;
                    }
                }
                /* NO button area */
                else if (fx >= bx + bw - 70 && fx < bx + bw - 10 &&
                         fy >= tab_y && fy < tab_y + 14) {
                    g_dialog.active = false;
                    g_dialog.confirm_mode = false;
                }
                return;
            }
            /* Normal dialog — advance teletype, then dismiss */
            if (!dialog_teletype_advance()) {
                /* Teletype still going — don't dismiss yet */
                return;
            }
            int action = g_dialog.pending_action;
            g_dialog.active = false;
            if (action != DIALOG_ACTION_NONE) {
                switch (action) {
                    case DIALOG_ACTION_BRIEFING_NEXT: {
                        captain_briefing_page++;
                        if (captain_briefing_page < g_dlgd.captain_briefing_pages) {
                            /* Show next briefing page */
                            memset(&g_dialog, 0, sizeof(g_dialog));
                            snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "CPT HARDEN");
                            dialog_from_block(&g_dlgd.captain_briefing[captain_briefing_page]);
                            if (captain_briefing_page < g_dlgd.captain_briefing_pages - 1)
                                g_dialog.pending_action = DIALOG_ACTION_BRIEFING_NEXT;
                            else {
                                g_dialog.pending_action = DIALOG_ACTION_NONE;
                                mission_briefed = true;
                            }
                            g_dialog.active = true;
                        } else {
                            mission_briefed = true;
                        }
                        break;
                    }
                    case DIALOG_ACTION_STARMAP:
                        printf("[STARMAP] mission_available=%d boss_done=%d sector=%d\n",
                               g_hub.mission_available, current_map_boss_done, player_sector);
                        if (g_hub.mission_available) {
                            printf("[STARMAP] BLOCKED: mission active\n");
                            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "NEUTRALIZE THE ENEMY SHIP FIRST");
                            g_hub.hud_msg_timer = 90;
                        } else {
                            if (current_map_boss_done) {
                                player_starmap++;
                                current_map_boss_done = false;
                                g_starmap.active = false; /* force new map for next sector */
                            }
                            if (!g_starmap.active) {
                                dng_rng_seed((uint32_t)(player_sector * 1337 + 42 + player_starmap * 9999));
                                starmap_generate_or_load(&g_starmap, player_sector);
                            }
                            app_state = STATE_STARMAP;
                        }
                        break;
                    case DIALOG_ACTION_SHOP:
                        dng_rng_seed((uint32_t)(player_sector * 7777 + 123));
                        shop_generate(&g_shop);
                        if (!mission_armory_done) mission_armory_done = true;
                        active_shop_type = 0;
                        app_state = STATE_SHOP;
                        break;
                    case DIALOG_ACTION_MEDBAY_SHOP:
                        dng_rng_seed((uint32_t)(player_sector * 5555 + 456 + player_biomass * 13));
                        medbay_shop_generate(&g_medbay_shop);
                        active_shop_type = 1;
                        app_state = STATE_SHOP;
                        break;
                    case DIALOG_ACTION_SHOW_KIT: {
                        /* Show starting deck cards as kit display */
                        memset(&g_kit, 0, sizeof(g_kit));
                        g_kit.card_count = g_player.persistent_deck_count;
                        for (int i = 0; i < g_kit.card_count; i++)
                            g_kit.cards[i] = g_player.persistent_deck[i];
                        g_kit.detail_idx = -1;
                        g_kit.active = true;
                        if (!mission_armory_done) mission_armory_done = true;
                        break;
                    }
                    case DIALOG_ACTION_TELEPORT:
                        if (g_hub.mission_available)
                            hub_show_teleport_confirm();
                        break;
                    case DIALOG_ACTION_TELEPORT_GO:
                        if (g_hub.mission_available) {
                            { int sw, sh; game_ship_size(player_sector, &sw, &sh);
                            dng_game_init_sized(&dng_state, sw, sh); }
                            game_init_ship();
                            dng_initialized = true;
    dng_hull_computed = false;
                            last_player_gx = dng_state.player.gx;
                            last_player_gy = dng_state.player.gy;
                            g_hub.mission_available = false;
                            beam_timer = 0;
                            app_state = STATE_BEAM;
                        }
                        break;
                    case DIALOG_ACTION_HEAL:
                        if (!mission_medbay_done && mission_briefed && !mission_armory_done) {
                            /* First visit during prep: free heal + check off objective */
                            g_player.hp = g_player.hp_max;
                            mission_medbay_done = true;
                            snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "VITALS LOGGED. YOU'RE CLEAR.");
                            g_hub.hud_msg_timer = 90;
                        } else {
                            /* Open medbay shop */
                            if (!mission_medbay_done && mission_briefed) mission_medbay_done = true;
                            dng_rng_seed((uint32_t)(player_sector * 5555 + 456 + player_biomass * 13));
                            shop_generate(&g_medbay_shop);
                            active_shop_type = 1;
                            app_state = STATE_SHOP;
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
        if (app_state == STATE_RUNNING && chest_overlay_active) {
            float fx, fy; screen_to_fb(ev->mouse_x, ev->mouse_y, &fx, &fy);
            int cw = 72, ch = 80, cgap = 12;
            int ctotal = 3 * (cw + cgap) - cgap;
            int cx0 = (FB_WIDTH - ctotal) / 2, cy0 = 50;
            for (int i = 0; i < 3; i++) {
                int cx = cx0 + i * (cw + cgap);
                if (fx >= cx && fx < cx + cw && fy >= cy0 && fy < cy0 + ch) {
                    if (g_player.persistent_deck_count < COMBAT_DECK_MAX)
                        g_player.persistent_deck[g_player.persistent_deck_count++] = chest_choices[i];
                    dng_state.dungeon->chests[chest_gy][chest_gx] = 0;
                    chest_overlay_active = false;
                    return;
                }
            }
            int skip_x = (FB_WIDTH - 50) / 2, skip_y = cy0 + ch + 14;
            if (fx >= skip_x && fx < skip_x + 50 && fy >= skip_y && fy < skip_y + 14) {
                dng_state.dungeon->chests[chest_gy][chest_gx] = 0;
                chest_overlay_active = false;
            }
            return;
        }
        if (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB) {
            dng_touch_began(ev->mouse_x, ev->mouse_y, now_time);
        } else {
            handle_screen_tap(ev->mouse_x, ev->mouse_y);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_BEGAN && ev->num_touches > 0) {
        float sx = ev->touches[0].pos_x, sy = ev->touches[0].pos_y;
        if (app_state == STATE_RUNNING && chest_overlay_active) {
            float fx, fy; screen_to_fb(sx, sy, &fx, &fy);
            int cw = 72, ch = 80, cgap = 12;
            int ctotal = 3 * (cw + cgap) - cgap;
            int cx0 = (FB_WIDTH - ctotal) / 2, cy0 = 50;
            for (int i = 0; i < 3; i++) {
                int cx = cx0 + i * (cw + cgap);
                if (fx >= cx && fx < cx + cw && fy >= cy0 && fy < cy0 + ch) {
                    if (g_player.persistent_deck_count < COMBAT_DECK_MAX)
                        g_player.persistent_deck[g_player.persistent_deck_count++] = chest_choices[i];
                    dng_state.dungeon->chests[chest_gy][chest_gx] = 0;
                    chest_overlay_active = false;
                    return;
                }
            }
            int skip_x = (FB_WIDTH - 50) / 2, skip_y = cy0 + ch + 14;
            if (fx >= skip_x && fx < skip_x + 50 && fy >= skip_y && fy < skip_y + 14) {
                dng_state.dungeon->chests[chest_gy][chest_gx] = 0;
                chest_overlay_active = false;
            }
            return;
        }
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

    /* ── Pause menu toggle (ESC during gameplay) ──────────── */
    if (ev->key_code == SAPP_KEYCODE_ESCAPE &&
        (app_state == STATE_RUNNING || app_state == STATE_SHIP_HUB || app_state == STATE_COMBAT)) {
        game_paused = !game_paused;
        if (!game_paused) settings_save();
        return;
    }
    if (game_paused) return; /* block all other input while paused */

    /* ── Debug: Ctrl+F6 = toggle window segments ─────────── */
    if (debug_mode && ev->key_code == SAPP_KEYCODE_F6 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        dng_hide_windows = !dng_hide_windows;
        return;
    }

    /* ── Debug: Ctrl+F7 = toggle interior geometry ──────────── */
    if (debug_mode && ev->key_code == SAPP_KEYCODE_F7 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        dng_hide_interior = !dng_hide_interior;
        return;
    }

    /* ── Debug: Ctrl+F5 = instant win dungeon ──────────────── */
    if (debug_mode && ev->key_code == SAPP_KEYCODE_F5 && (ev->modifiers & SAPP_MODIFIER_CTRL) &&
        app_state == STATE_RUNNING && current_ship.initialized) {
        current_ship.boarding_active = false;
        int reward = 30 + player_sector * 10;
        mission_complete_return_to_hub(reward, "DEBUG: MISSION COMPLETE", true);
        return;
    }

    /* ── Debug: Ctrl+F9 = full heal in combat ────────────────── */
    if (debug_mode && ev->key_code == SAPP_KEYCODE_F9 && (ev->modifiers & SAPP_MODIFIER_CTRL) &&
        app_state == STATE_COMBAT) {
        combat.player_hp = combat.player_hp_max;
        g_player.hp = g_player.hp_max;
        combat_log(&combat, "DEBUG: FULL HEAL");
        combat_set_message(&combat, "DEBUG HEAL!");
        return;
    }

    /* ── Mission summary screen ─────────────────────────────── */
    if (app_state == STATE_MISSION_SUMMARY) {
        if (ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_ENTER ||
            ev->key_code == SAPP_KEYCODE_F) {
            app_state = STATE_SHIP_HUB;
            game_save();
        }
        return;
    }

    /* ── Title screen ────────────────────────────────────────── */
    if (app_state == STATE_TITLE) {
        if (title_confirm_new) {
            if (ev->key_code == SAPP_KEYCODE_Y || ev->key_code == SAPP_KEYCODE_ENTER) {
                game_delete_save();
                save_exists = false;
                title_confirm_new = false;
                app_state = STATE_CLASS_SELECT;
            } else if (ev->key_code == SAPP_KEYCODE_N || ev->key_code == SAPP_KEYCODE_ESCAPE) {
                title_confirm_new = false;
            }
            return;
        }
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
                    if (save_exists)
                        title_confirm_new = true;
                    else
                        app_state = STATE_CLASS_SELECT;
                } else if (save_exists && game_load()) {
                    last_player_gx = dng_state.player.gx;
                    last_player_gy = dng_state.player.gy;
                    /* app_state restored by game_load() */
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
            case SAPP_KEYCODE_TAB:
                skip_intro = !skip_intro;
                settings_save();
                break;
            case SAPP_KEYCODE_ENTER:
            case SAPP_KEYCODE_KP_ENTER:
            case SAPP_KEYCODE_SPACE:
                selected_class = class_select_cursor;
                player_persist_init(selected_class);
                weakness_init((uint32_t)(time(NULL) ^ (selected_class * 31337)));
                player_scrap = 30;
                player_sector = 0;
                captain_briefing_page = 0;
                player_samples = 0;
                player_starmap = 0;
                current_map_boss_done = false;
                current_mission_is_boss = false;
                memset(&g_starmap, 0, sizeof(g_starmap));
                memset(&g_run_stats, 0, sizeof(g_run_stats));
                if (skip_intro) {
                    mission_briefed = true;
                    mission_medbay_done = true;
                    mission_armory_done = true;
                    hub_generate(&g_hub);
                    memset(&g_dialog, 0, sizeof(g_dialog));
                    snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "CPT HARDEN");
                    snprintf(g_dialog.lines[0], DIALOG_LINE_LEN,
                             "GET TO THE TELEPORTER, SOLDIER.");
                    snprintf(g_dialog.lines[1], DIALOG_LINE_LEN,
                             "WE HAVE A DERELICT TO BOARD.");
                    g_dialog.line_count = 2;
                    g_dialog.pending_action = DIALOG_ACTION_NONE;
                    g_dialog.active = true;
                    app_state = STATE_SHIP_HUB;
                } else {
                    mission_briefed = false;
                    mission_medbay_done = false;
                    mission_armory_done = false;
                    medbay_used = false;
                    intro_char_idx = 0;
                    intro_timer = 0;
                    intro_done = false;
                    app_state = STATE_INTRO;
                }
                break;
            default: break;
        }
        return;
    }

    /* ── Intro teletype ─────────────────────────────────────── */
    if (app_state == STATE_INTRO) {
        if (ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_ENTER) {
            if (intro_done) {
                hub_generate(&g_hub);
                app_state = STATE_SHIP_HUB;
            } else {
                intro_done = true;
                intro_char_idx = 9999;
            }
        }
        return;
    }

    /* ── Run stats screen ──────────────────────────────────── */
    if (app_state == STATE_RUN_STATS) {
        if (ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_ENTER) {
            app_state = STATE_TITLE;
            save_exists = game_has_save();
        }
        return;
    }

    /* ── Epilogue teletype ──────────────────────────────────── */
    if (app_state == STATE_EPILOGUE) {
        if (ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_ENTER) {
            if (intro_done) {
                app_state = STATE_RUN_STATS;
            } else {
                intro_done = true;
                intro_char_idx = 9999;
            }
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
        if (g_kit.active) {
            if (ev->key_code == SAPP_KEYCODE_ESCAPE || ev->key_code == SAPP_KEYCODE_F ||
                ev->key_code == SAPP_KEYCODE_SPACE || ev->key_code == SAPP_KEYCODE_ENTER) {
                if (g_kit.detail_idx >= 0)
                    g_kit.detail_idx = -1;
                else
                    g_kit.active = false;
            }
            return;
        }
        if (g_dialog.active) {
            if (g_dialog.confirm_mode) {
                if (!g_dialog.tt_all_done) {
                    /* Advance teletype before showing YES/NO */
                    if (ev->key_code == SAPP_KEYCODE_F || ev->key_code == SAPP_KEYCODE_ENTER ||
                        ev->key_code == SAPP_KEYCODE_SPACE)
                        dialog_teletype_advance();
                    return;
                }
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
                                { int sw, sh; game_ship_size(player_sector, &sw, &sh);
                                dng_game_init_sized(&dng_state, sw, sh); }
                                game_init_ship();
                                dng_initialized = true;
    dng_hull_computed = false;
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
                /* Advance teletype first (ESC skips to dismiss) */
                if (ev->key_code != SAPP_KEYCODE_ESCAPE && !dialog_teletype_advance()) {
                    return; /* still advancing text */
                }
                int action = g_dialog.pending_action;
                g_dialog.active = false;
                /* Trigger pending action on dismiss (not on ESC) */
                if (ev->key_code != SAPP_KEYCODE_ESCAPE && action != DIALOG_ACTION_NONE) {
                    switch (action) {
                        case DIALOG_ACTION_BRIEFING_NEXT: {
                            captain_briefing_page++;
                            if (captain_briefing_page < g_dlgd.captain_briefing_pages) {
                                memset(&g_dialog, 0, sizeof(g_dialog));
                                snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "CPT HARDEN");
                                dialog_from_block(&g_dlgd.captain_briefing[captain_briefing_page]);
                                if (captain_briefing_page < g_dlgd.captain_briefing_pages - 1)
                                    g_dialog.pending_action = DIALOG_ACTION_BRIEFING_NEXT;
                                else {
                                    g_dialog.pending_action = DIALOG_ACTION_NONE;
                                    mission_briefed = true;
                                }
                                g_dialog.active = true;
                            } else {
                                mission_briefed = true;
                            }
                            break;
                        }
                        case DIALOG_ACTION_STARMAP:
                            if (g_hub.mission_available) {
                                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "INVESTIGATE THE DERELICT FIRST");
                                g_hub.hud_msg_timer = 90;
                            } else {
                                if (current_map_boss_done) {
                                    player_starmap++;
                                    current_map_boss_done = false;
                                    g_starmap.active = false;
                                }
                                if (!g_starmap.active) {
                                    dng_rng_seed((uint32_t)(player_sector * 1337 + 42 + player_starmap * 9999));
                                    starmap_generate_or_load(&g_starmap, player_sector);
                                }
                                app_state = STATE_STARMAP;
                            }
                            break;
                        case DIALOG_ACTION_SHOP:
                            dng_rng_seed((uint32_t)(player_sector * 7777 + 123));
                            shop_generate(&g_shop);
                            if (!mission_armory_done) mission_armory_done = true;
                            active_shop_type = 0;
                            app_state = STATE_SHOP;
                            break;
                        case DIALOG_ACTION_MEDBAY_SHOP:
                            dng_rng_seed((uint32_t)(player_sector * 5555 + 456 + player_biomass * 13));
                            medbay_shop_generate(&g_medbay_shop);
                            active_shop_type = 1;
                            app_state = STATE_SHOP;
                            break;
                        case DIALOG_ACTION_SHOW_KIT: {
                            memset(&g_kit, 0, sizeof(g_kit));
                            g_kit.card_count = g_player.persistent_deck_count;
                            for (int i = 0; i < g_kit.card_count; i++)
                                g_kit.cards[i] = g_player.persistent_deck[i];
                            g_kit.detail_idx = -1;
                            g_kit.active = true;
                            if (!mission_armory_done) mission_armory_done = true;
                            break;
                        }
                        case DIALOG_ACTION_TELEPORT:
                            if (g_hub.mission_available)
                                hub_show_teleport_confirm();
                            break;
                        case DIALOG_ACTION_TELEPORT_GO:
                            if (g_hub.mission_available) {
                                { int sw, sh; game_ship_size(player_sector, &sw, &sh);
                                dng_game_init_sized(&dng_state, sw, sh); }
                                game_init_ship();
                                dng_initialized = true;
    dng_hull_computed = false;
                                last_player_gx = dng_state.player.gx;
                                last_player_gy = dng_state.player.gy;
                                g_hub.mission_available = false;
                                app_state = STATE_RUNNING;
                            }
                            break;
                        case DIALOG_ACTION_HEAL:
                            if (!mission_medbay_done && mission_briefed && !mission_armory_done) {
                                g_player.hp = g_player.hp_max;
                                mission_medbay_done = true;
                                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "VITALS LOGGED. YOU'RE CLEAR.");
                                g_hub.hud_msg_timer = 90;
                            } else {
                                if (!mission_medbay_done && mission_briefed) mission_medbay_done = true;
                                dng_rng_seed((uint32_t)(player_sector * 5555 + 456 + player_biomass * 13));
                                shop_generate(&g_medbay_shop);
                                active_shop_type = 1;
                                app_state = STATE_SHOP;
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
            active_shop()->active = false;
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

    /* Chest overlay in dungeon */
    if (chest_overlay_active) {
        if (ev->key_code == SAPP_KEYCODE_ESCAPE) {
            dng_state.dungeon->chests[chest_gy][chest_gx] = 0;
            chest_overlay_active = false;
        }
        return; /* block all other input */
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
            if (dng_play_state == DNG_STATE_PLAYING) {
                int back_dir = (dng_state.player.dir + 2) % 4;
                int bnx = dng_state.player.gx + dng_dir_dx[back_dir];
                int bny = dng_state.player.gy + dng_dir_dz[back_dir];
                if (bnx >= 1 && bnx <= dng_state.dungeon->w &&
                    bny >= 1 && bny <= dng_state.dungeon->h &&
                    (dng_state.dungeon->aliens[bny][bnx] != 0 ||
                     dng_state.dungeon->consoles[bny][bnx] != 0)) {
                    dng_state.player.dir = back_dir;
                    dng_state.player.target_angle = back_dir * 0.25f;
                } else {
                    dng_player_try_move(&dng_state.player, dng_state.dungeon, back_dir);
                }
            }
            break;
        case SAPP_KEYCODE_A:
            if (dng_play_state == DNG_STATE_PLAYING) {
                int strafe_dir_a = (dng_state.player.dir + 3) % 4;
                int snx_a = dng_state.player.gx + dng_dir_dx[strafe_dir_a];
                int sny_a = dng_state.player.gy + dng_dir_dz[strafe_dir_a];
                /* If strafe target has enemy or console, rotate to face it instead */
                if (snx_a >= 1 && snx_a <= dng_state.dungeon->w &&
                    sny_a >= 1 && sny_a <= dng_state.dungeon->h &&
                    (dng_state.dungeon->aliens[sny_a][snx_a] != 0 ||
                     dng_state.dungeon->consoles[sny_a][snx_a] != 0)) {
                    dng_state.player.dir = strafe_dir_a;
                    dng_state.player.target_angle = strafe_dir_a * 0.25f;
                } else {
                    dng_player_try_move(&dng_state.player, dng_state.dungeon, strafe_dir_a);
                }
            }
            break;
        case SAPP_KEYCODE_D:
            if (dng_play_state == DNG_STATE_PLAYING) {
                int strafe_dir_d = (dng_state.player.dir + 1) % 4;
                int snx_d = dng_state.player.gx + dng_dir_dx[strafe_dir_d];
                int sny_d = dng_state.player.gy + dng_dir_dz[strafe_dir_d];
                if (snx_d >= 1 && snx_d <= dng_state.dungeon->w &&
                    sny_d >= 1 && sny_d <= dng_state.dungeon->h &&
                    (dng_state.dungeon->aliens[sny_d][snx_d] != 0 ||
                     dng_state.dungeon->consoles[sny_d][snx_d] != 0)) {
                    dng_state.player.dir = strafe_dir_d;
                    dng_state.player.target_angle = strafe_dir_d * 0.25f;
                } else {
                    dng_player_try_move(&dng_state.player, dng_state.dungeon, strafe_dir_d);
                }
            }
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
        .window_title = "Drake's Void",
        .high_dpi     = true,
        .logger.func  = slog_func,
        .swap_interval = 1,
    };
}
