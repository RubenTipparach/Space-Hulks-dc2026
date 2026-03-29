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
#include "sr_ship.h"
#include "sr_menu.h"
#include "sr_mobile_input.h"

static void game_init_ship(void); /* forward decl */

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
    for (int fl = 0; fl <= sd.current_floor; fl++) {
        bool is_last = (fl >= DNG_MAX_FLOORS - 1);
        dng_generate(&dng_state.floors[fl], DNG_GRID_W, DNG_GRID_H,
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

static void game_init_ship(void) {
    /* Generate ship based on current floor as difficulty */
    int difficulty = dng_state.current_floor;
    uint32_t ship_seed = dng_state.seed_base + 9999;
    ship_generate(&current_ship, difficulty, ship_seed);

    /* For each deck, populate the dungeon floor with ship room data */
    for (int deck = 0; deck < current_ship.num_decks && deck < DNG_MAX_FLOORS; deck++) {
        if (!dng_state.floor_generated[deck]) continue;
        sr_dungeon *dd = &dng_state.floors[deck];

        /* Clear existing aliens and consoles (will be re-placed below) */
        memset(dd->aliens, 0, sizeof(dd->aliens));
        memset(dd->consoles, 0, sizeof(dd->consoles));

        /* Build dng_room array from stored room info */
        dng_room rooms[12];
        for (int r = 0; r < dd->room_count && r < 12; r++) {
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

        /* Populate with officers and enemies */
        ship_populate_deck(&current_ship, dd, deck, dd->room_count, rooms);

        /* Place consoles at room centers for rooms with active subsystems or cargo */
        for (int r = 0; r < count && r < dd->room_count; r++) {
            ship_room *rm = &current_ship.rooms[start + r];
            if (rm->type == ROOM_CORRIDOR) continue;
            /* Only place console if subsystem is active or it's unsearched cargo */
            if (rm->subsystem_hp_max > 0 && rm->subsystem_hp <= 0) continue;
            if (rm->type == ROOM_CARGO && rm->cleared) continue;
            int cx = dd->room_cx[r], cy = dd->room_cy[r];
            if (cx >= 1 && cx <= dd->w && cy >= 1 && cy <= dd->h) {
                /* Don't place on top of aliens — offset if needed */
                if (dd->aliens[cy][cx] == 0)
                    dd->consoles[cy][cx] = (uint8_t)rm->type;
            }
        }
    }
}

/* ── Handle combat end (shared by tap and keyboard) ────────────── */

static void handle_combat_end(void) {
    if (current_ship.initialized) {
        if (combat.player_won && current_combat_room >= 0) {
            current_ship.rooms[current_combat_room].cleared = true;
            /* Console sabotage deals heavy subsystem damage */
            int sub_dmg = console_combat ? 10 : 3;
            ship_damage_subsystem(&current_ship, current_combat_room, sub_dmg);

            for (int o = 0; o < current_ship.officer_count; o++) {
                if (current_ship.officers[o].room_idx == current_combat_room &&
                    current_ship.officers[o].alive) {
                    current_ship.officers[o].alive = false;
                    current_ship.officers[o].captured = true;
                }
            }
            ship_check_missions(&current_ship);
        }
        console_combat = false;

        if (!combat.player_won) {
            g_player.hp = g_player.hp_max / 2;
            dng_state.current_floor = 0;
            dng_state.dungeon = &dng_state.floors[0];
            dng_player_init(&dng_state.player,
                            dng_state.dungeon->spawn_gx,
                            dng_state.dungeon->spawn_gy, 0);
            last_player_gx = dng_state.player.gx;
            last_player_gy = dng_state.player.gy;
        }

        if (current_ship.player_ship_destroyed) {
            g_player.hp = g_player.hp_max / 4;
            if (g_player.hp < 1) g_player.hp = 1;
            dng_state.current_floor = 0;
            dng_state.dungeon = &dng_state.floors[0];
            dng_player_init(&dng_state.player,
                            dng_state.dungeon->spawn_gx,
                            dng_state.dungeon->spawn_gy, 0);
            last_player_gx = dng_state.player.gx;
            last_player_gy = dng_state.player.gy;
            current_ship.boarding_active = false;
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
            /* Remove the console from the map */
            dng_state.dungeon->consoles[p->gy][p->gx] = 0;

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
    }
}

/* ── Class select screen ─────────────────────────────────────────── */
static void draw_class_select(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;
    uint32_t yellow = 0xFF00DDDD;

    for (int i = 0; i < W * H; i++) px[i] = 0xFF0D0D11;

    sr_draw_text_shadow(px, W, H, W/2 - 45, 30, "SPACE HULKS", white, shadow);
    sr_draw_text_shadow(px, W, H, W/2 - 55, 50, "SELECT YOUR CLASS", gray, shadow);

    /* Scout */
    {
        int bx = W/4 - 40, by = 80;
        bool sel = (class_select_cursor == 0);
        uint32_t border = sel ? yellow : gray;
        combat_draw_rect_outline(px, W, H, bx, by, 80, 120, border);
        if (sel) combat_draw_rect_outline(px, W, H, bx+1, by+1, 78, 118, border);

        spr_draw(px, W, H, spr_scout, bx + 24, by + 8, 2);
        sr_draw_text_shadow(px, W, H, bx + 24, by + 48, "SCOUT", sel ? yellow : white, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 62, "HP: 20", gray, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 74, "FAST", gray, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 86, "2 BURST", gray, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 98, "2 MOVE", gray, shadow);
    }

    /* Marine */
    {
        int bx = 3*W/4 - 40, by = 80;
        bool sel = (class_select_cursor == 1);
        uint32_t border = sel ? yellow : gray;
        combat_draw_rect_outline(px, W, H, bx, by, 80, 120, border);
        if (sel) combat_draw_rect_outline(px, W, H, bx+1, by+1, 78, 118, border);

        spr_draw(px, W, H, spr_marine, bx + 24, by + 8, 2);
        sr_draw_text_shadow(px, W, H, bx + 22, by + 48, "MARINE", sel ? yellow : white, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 62, "HP: 30", gray, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 74, "TOUGH", gray, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 86, "3 SHOOT", gray, shadow);
        sr_draw_text_shadow(px, W, H, bx + 8, by + 98, "4 SHIELD", gray, shadow);
    }

    sr_draw_text_shadow(px, W, H, W/2 - 55, H - 20,
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
    itextures[ITEX_STONE] = sr_indexed_load("assets/indexed/stone.idx");

    stextures[STEX_LURKER]    = sr_texture_load("assets/sprites/lurker.png");
    stextures[STEX_BRUTE]     = sr_texture_load("assets/sprites/brute.png");
    stextures[STEX_SPITTER]   = sr_texture_load("assets/sprites/spitter.png");
    stextures[STEX_HIVEGUARD] = sr_texture_load("assets/sprites/hiveguard.png");
    stextures[STEX_SCOUT]     = sr_texture_load("assets/sprites/scout.png");
    stextures[STEX_MARINE]    = sr_texture_load("assets/sprites/marine.png");

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

            /* Recolor minimap cells by ship room type */
            {
                sr_dungeon *md = dng_state.dungeon;
                int mscale = 2;
                int mmx = fb.width - md->w * mscale - 4;
                int mmy = 4;
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

            /* Room label at bottom */
            dng_player *rp = &dng_state.player;
            int local_room = dng_room_at(dng_state.dungeon, rp->gx, rp->gy);
            if (local_room >= 0 && local_room < dng_state.dungeon->room_count) {
                int sr_idx = dng_state.dungeon->room_ship_idx[local_room];
                if (sr_idx >= 0 && sr_idx < current_ship.room_count) {
                    draw_room_label(fb.color, fb.width, fb.height,
                                    &current_ship.rooms[sr_idx], sr_idx, &current_ship);
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
        /* Scout box: left quarter */
        int sb_x = FB_WIDTH/4 - 40, sb_y = 80;
        if (fx >= sb_x && fx <= sb_x + 80 && fy >= sb_y && fy <= sb_y + 120) {
            if (class_select_cursor == 0) {
                /* Already selected — start */
                selected_class = 0;
                player_persist_init(selected_class);
                dng_game_init(&dng_state);
                game_init_ship();
                dng_initialized = true;
                last_player_gx = dng_state.player.gx;
                last_player_gy = dng_state.player.gy;
                app_state = STATE_RUNNING;
            } else {
                class_select_cursor = 0;
            }
            return;
        }
        /* Marine box: right quarter */
        int mb_x = 3*FB_WIDTH/4 - 40, mb_y = 80;
        if (fx >= mb_x && fx <= mb_x + 80 && fy >= mb_y && fy <= mb_y + 120) {
            if (class_select_cursor == 1) {
                selected_class = 1;
                player_persist_init(selected_class);
                dng_game_init(&dng_state);
                game_init_ship();
                dng_initialized = true;
                last_player_gx = dng_state.player.gx;
                last_player_gy = dng_state.player.gy;
                app_state = STATE_RUNNING;
            } else {
                class_select_cursor = 1;
            }
            return;
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

    /* ── Mouse click / touch began → tap handling ────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        if (app_state == STATE_RUNNING) {
            dng_touch_began(ev->mouse_x, ev->mouse_y, now_time);
        } else {
            handle_screen_tap(ev->mouse_x, ev->mouse_y);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_BEGAN && ev->num_touches > 0) {
        float sx = ev->touches[0].pos_x, sy = ev->touches[0].pos_y;
        if (app_state == STATE_RUNNING) {
            dng_touch_began(sx, sy, now_time);
        } else {
            handle_screen_tap(sx, sy);
        }
        return;
    }

    /* ── Mouse move / touch move ─────────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        if (app_state == STATE_RUNNING) dng_touch_moved(ev->mouse_x, ev->mouse_y);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->mouse_x, ev->mouse_y, &fx, &fy);
            combat_touch_moved(&combat, fx, fy);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_MOVED && ev->num_touches > 0) {
        if (app_state == STATE_RUNNING) dng_touch_moved(ev->touches[0].pos_x, ev->touches[0].pos_y);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->touches[0].pos_x, ev->touches[0].pos_y, &fx, &fy);
            combat_touch_moved(&combat, fx, fy);
        }
        return;
    }

    /* ── Mouse up / touch end ────────────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_UP && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        if (app_state == STATE_RUNNING) dng_touch_ended(ev->mouse_x, ev->mouse_y, now_time);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->mouse_x, ev->mouse_y, &fx, &fy);
            combat_touch_ended(&combat, fx, fy);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_ENDED && ev->num_touches > 0) {
        if (app_state == STATE_RUNNING) dng_touch_ended(ev->touches[0].pos_x, ev->touches[0].pos_y, now_time);
        else if (app_state == STATE_COMBAT) {
            float fx, fy; screen_to_fb(ev->touches[0].pos_x, ev->touches[0].pos_y, &fx, &fy);
            combat_touch_ended(&combat, fx, fy);
        }
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED) {
        if (app_state == STATE_RUNNING) dng_touch_cancelled();
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
                class_select_cursor = 0;
                break;
            case SAPP_KEYCODE_RIGHT:
            case SAPP_KEYCODE_D:
                class_select_cursor = 1;
                break;
            case SAPP_KEYCODE_ENTER:
            case SAPP_KEYCODE_KP_ENTER:
            case SAPP_KEYCODE_SPACE:
                selected_class = class_select_cursor;
                player_persist_init(selected_class);
                dng_game_init(&dng_state);
                game_init_ship();
                dng_initialized = true;
                last_player_gx = dng_state.player.gx;
                last_player_gy = dng_state.player.gy;
                app_state = STATE_RUNNING;
                break;
            default: break;
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

    switch (ev->key_code) {
        case SAPP_KEYCODE_M:
            dng_expanded_map = true;
            break;
        case SAPP_KEYCODE_F:
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
