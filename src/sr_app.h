/*  sr_app.h — Shared application state, constants, and enums.
 *  Included by all scene/input modules. Single-TU header-only. */
#ifndef SR_APP_H
#define SR_APP_H

#include <stdio.h>
#include <math.h>
#include <time.h>

/* ── Configuration ───────────────────────────────────────────────── */

#define FB_WIDTH       480
#define FB_HEIGHT      270
#define TARGET_FPS     60
#define GIF_TARGET_FPS 24.0
#define FOG_NEAR       35.0f
#define FOG_FAR        60.0f
#define FOG_COLOR      0xFFEBCE87

/* ── Globals ─────────────────────────────────────────────────────── */

static sr_framebuffer fb;

/* Texture enums */
enum {
    TEX_BRICK, TEX_GRASS, TEX_ROOF, TEX_WOOD, TEX_TILE, TEX_COUNT
};
static sr_texture textures[TEX_COUNT];

/* Indexed (palette) textures */
enum {
    ITEX_BRICK, ITEX_GRASS, ITEX_ROOF, ITEX_WOOD, ITEX_TILE, ITEX_STONE, ITEX_WALL_A,
    ITEX_HUB_FLOOR, ITEX_HUB_CEILING, ITEX_HUB_CORRIDOR,
    ITEX_WALL_A_WIN,
    ITEX_EXT_WALL, ITEX_EXT_WINDOW, ITEX_ALIEN_EXT, ITEX_ALIEN_EXT_WIN,
    ITEX_ALIEN_WALL, ITEX_ALIEN_CORRIDOR, ITEX_ALIEN_WIN,
    ITEX_COUNT
};
static sr_indexed_texture itextures[ITEX_COUNT];

/* Sprite textures (loaded from PNG) */
enum {
    STEX_LURKER, STEX_BRUTE, STEX_SPITTER, STEX_HIVEGUARD,
    STEX_STALKER, STEX_MAULER, STEX_ACID_THROWER, STEX_WARDEN,
    STEX_SCOUT, STEX_MARINE, STEX_ENGINEER, STEX_SCIENTIST,
    STEX_CREW_CAPTAIN, STEX_CREW_SERGEANT, STEX_CREW_QUARTERMASTER,
    STEX_CREW_PRIVATE, STEX_CREW_DOCTOR, STEX_CREW_BYTOR,
    STEX_ICON_ICE, STEX_ICON_ACID, STEX_ICON_FIRE, STEX_ICON_LIGHTNING,
    STEX_LOOT_CHEST,
    STEX_BOSS_FRAME_0, STEX_BOSS_FRAME_1, STEX_BOSS_FRAME_2,
    /* Starmap node icons */
    STEX_MAP_NORMAL, STEX_MAP_MINIBOSS, STEX_MAP_EVENT, STEX_MAP_BOSS,
    STEX_COUNT
};
static sr_texture stextures[STEX_COUNT];

/* Console textures (built from embedded sprite data at runtime) */
#define CONSOLE_TEX_COUNT 10 /* matches ROOM_TYPE_COUNT */
static sr_texture console_textures[CONSOLE_TEX_COUNT]; /* indexed by room type */

/* Timing */
static double time_acc;
static int    frame_counter;
static double fps_timer;
static int    fps_frame_count;
static int    fps_display;
static double gif_capture_timer;
static int    screenshot_counter;

/* ── Debug mode (loaded from config/game_config.yaml) ──────────── */

static bool debug_mode = false;

/* ── App state ─────────────────────────────────────────────────── */

enum { STATE_TITLE, STATE_CLASS_SELECT, STATE_INTRO, STATE_RUNNING, STATE_COMBAT,
       STATE_SHIP_HUB, STATE_SHOP, STATE_DIALOG, STATE_STARMAP, STATE_EPILOGUE,
       STATE_BEAM, STATE_MISSION_SUMMARY, STATE_RUN_STATS };
static int app_state = STATE_TITLE;
static bool game_paused = false;   /* pause menu overlay */
static int  pause_prev_state = 0;  /* state before pause */
static int selected_class = 0;  /* 0=scout, 1=marine */
static int class_select_cursor = 0;
static bool skip_intro = false;
static int title_cursor = 0;
static bool title_confirm_new = false; /* show "delete save?" confirm */
static bool save_exists = false;

/* ── Mission flow tracking ─────────────────────────────────────── */

static bool mission_briefed = false;     /* talked to captain for briefing */
static bool mission_medbay_done = false; /* cleared by Dr Vasquez */
static bool mission_armory_done = false; /* visited Chen's armory */
static bool medbay_used = false;         /* already healed once between missions */
static int  captain_briefing_page = 0;   /* current page in multi-page briefing */

/* Intro / epilogue teletype state */
static int  intro_char_idx = 0;   /* total chars revealed so far */
static int  intro_timer = 0;      /* frame counter for typing speed */
static bool intro_done = false;   /* all text revealed */

/* ── Beam teleport effect ──────────────────────────────────────── */

static int  beam_timer = 0;       /* frame counter for beam animation */
#define BEAM_DURATION 90          /* total frames for beam effect */

/* Simple RNG for beam sparkle particles */
static uint32_t beam_rng_state = 12345;
static int beam_rng(void) {
    beam_rng_state = beam_rng_state * 1103515245 + 12345;
    return (beam_rng_state >> 16) & 0x7FFF;
}

/* ── Boss / sample progression ─────────────────────────────────── */

#define SAMPLES_REQUIRED 3
#define STARMAPS_TOTAL   3
static int  player_samples = 0;     /* samples collected (0-3) */
static int  player_starmap = 0;     /* which star map we're on (0-2) */
static bool current_map_boss_done = false;  /* boss beaten on current star map */
static bool current_mission_is_boss = false; /* true if currently on a boss node mission */
static bool current_mission_is_miniboss = false; /* true if currently on a miniboss node */
static int  current_miniboss_type = ENEMY_MINIBOSS_1; /* which miniboss to spawn */
static bool epilogue_is_win = false; /* true = victory, false = game over */

/* ── Run stats (accumulated across the entire run) ─────────────── */
typedef struct {
    int cards_gathered;
    int damage_taken;
    int damage_dealt;
    int chests_found;
    int enemies_killed;
    int bosses_killed;
    int sectors_visited;
    int scrap_earned;
    int biomass_earned;
} run_stats;
static run_stats g_run_stats;

/* ── Player progression / currency ─────────────────────────────── */

static int player_scrap = 0;       /* currency: buy normal cards, trash, heal */
static int player_biomass = 0;     /* currency: buy elemental cards */
static int player_sector = 0;      /* current sector (progression depth) */

/* ── Consumables ──────────────────────────────────────────────── */

enum {
    CONSUMABLE_NONE,
    CONSUMABLE_HEALTH_KIT,  /* heal 10 HP, usable any time in combat */
    CONSUMABLE_GRENADE,     /* deal 4 damage to all enemies */
    CONSUMABLE_TYPE_COUNT
};

static const char *consumable_names[] = { "", "HEALTH KIT", "GRENADE" };
static const int consumable_prices[] = { 0, 20, 25 };  /* scrap cost */

#define CONSUMABLE_SLOTS 2
static int player_consumables[CONSUMABLE_SLOTS]; /* item type per slot, 0 = empty */

/* ── Mission summary (shown after each mission) ───────────────── */

typedef struct {
    int enemies_killed;
    int terminals_destroyed;
    int terminals_total;
    int scrap_earned;
    int biomass_earned;
    bool all_killed;         /* true = killed all enemies */
    bool is_boss;
    char completion_method[32]; /* "ALL HOSTILES ELIMINATED" etc. */
} mission_summary;

static mission_summary g_summary;

#define SAVE_FILE "spacehulks.sav"
#define SETTINGS_FILE "spacehulks.cfg"
#define SAVE_MAGIC 0x534B4C48  /* "HLKS" */

/* ── User settings (persisted to SETTINGS_FILE) ───────────────── */

static float settings_master_vol = 1.0f;

static void settings_save(void) {
    FILE *f = fopen(SETTINGS_FILE, "wb");
    if (!f) return;
    fprintf(f, "skip_intro=%d\n", skip_intro ? 1 : 0);
    fprintf(f, "master_vol=%.2f\n", settings_master_vol);
    fclose(f);
}

static void settings_load(void) {
    FILE *f = fopen(SETTINGS_FILE, "rb");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        int val;
        float fval;
        if (sscanf(line, "skip_intro=%d", &val) == 1) skip_intro = (val != 0);
        if (sscanf(line, "master_vol=%f", &fval) == 1) {
            settings_master_vol = fval;
            if (settings_master_vol < 0) settings_master_vol = 0;
            if (settings_master_vol > 1) settings_master_vol = 1;
        }
    }
    fclose(f);
}

/* ── Simple RNG (deterministic) ─────────────────────────────────── */

static uint32_t rng_state = 12345;
static float rng_float(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (float)((rng_state >> 16) & 0x7FFF) / 32768.0f;
}
static float rng_range(float lo, float hi) {
    return lo + rng_float() * (hi - lo);
}

/* ── Coordinate mapping ─────────────────────────────────────────── */

static void screen_to_fb(float sx, float sy, float *fbx, float *fby) {
    float fb_aspect  = (float)FB_WIDTH / (float)FB_HEIGHT;
    float win_w = sapp_widthf(), win_h = sapp_heightf();
    float win_aspect = win_w / win_h;

    float scaled_w, scaled_h;
    if (win_aspect > fb_aspect) {
        scaled_h = win_h;
        scaled_w = win_h * fb_aspect;
    } else {
        scaled_w = win_w;
        scaled_h = win_w / fb_aspect;
    }

    float ox = (win_w - scaled_w) * 0.5f;
    float oy = (win_h - scaled_h) * 0.5f;

    *fbx = (sx - ox) / scaled_w * (float)FB_WIDTH;
    *fby = (sy - oy) / scaled_h * (float)FB_HEIGHT;
}

#endif /* SR_APP_H */
