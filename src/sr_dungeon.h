#ifndef SR_DUNGEON_H
#define SR_DUNGEON_H

/*  Grid-based dungeon crawler system.
 *  Ported from space_crawler.p64 (Picotron).
 *
 *  - Room-based dungeon generation with L-shaped corridors
 *  - Grid movement with smooth interpolation (WASD + arrows)
 *  - Up/down stairs with multi-floor persistence
 *  - Minimap with visibility
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* ── Enemy type enum (needed by dungeon generation) ─────────────── */

enum { ENEMY_LURKER, ENEMY_BRUTE, ENEMY_SPITTER, ENEMY_HIVEGUARD,
       /* Evolved tier 2 — advanced dragon parasite */
       ENEMY_STALKER, ENEMY_MAULER, ENEMY_ACID_THROWER, ENEMY_WARDEN,
       ENEMY_BOSS_1, ENEMY_BOSS_2, ENEMY_BOSS_3,
       /* Minibosses — tier 2 elites with boosted HP/damage */
       ENEMY_MINIBOSS_1, ENEMY_MINIBOSS_2, ENEMY_MINIBOSS_3, ENEMY_MINIBOSS_4,
       ENEMY_TYPE_COUNT };

/* Map enemy type -> sprite texture index (STEX_*). Declared here, defined after STEX enum exists. */
static int enemy_to_stex[ENEMY_TYPE_COUNT];
static bool enemy_to_stex_init = false;

/* ── Configuration ───────────────────────────────────────────────── */

#define DNG_GRID_W       80
#define DNG_GRID_H       80
#define DNG_CELL_SIZE    2.0f
#define DNG_HALF_CELL    1.0f   /* DNG_CELL_SIZE / 2 */
#define DNG_RENDER_R     20     /* max grid cells rendered from player (compile-time max) */
static int  dng_render_radius = 10;   /* runtime draw distance in cells (max DNG_RENDER_R) */
#define DNG_NUM_RAYS     120    /* DDA visibility rays */
#define DNG_MAX_FLOORS   16
#define DNG_NUM_STEPS    10     /* stair step count */
#define DNG_PILLAR_PAD   0.25f  /* DNG_CELL_SIZE / 8 */

#define DNG_MOVE_SMOOTH  0.25f
#define DNG_TURN_SMOOTH  0.40f

/* Climb animation */
#define DNG_CLIMB_MOVE_FRAMES   35
#define DNG_CLIMB_SETTLE_FRAMES 25
#define DNG_CLIMB_UP_HEIGHT     1.0f
#define DNG_CLIMB_DOWN_HEIGHT   1.0f

/* ── Direction vectors: 0=N, 1=E, 2=S, 3=W ──────────────────────── */

static const int dng_dir_dx[4] = { 0,  1,  0, -1 };
static const int dng_dir_dz[4] = {-1,  0,  1,  0 };

/* ── Dungeon data ────────────────────────────────────────────────── */

typedef struct {
    uint8_t map[DNG_GRID_H + 1][DNG_GRID_W + 1]; /* 1=wall, 0=open (1-indexed) */
    int w, h;
    int spawn_gx, spawn_gy, spawn_dir;
    int stairs_gx, stairs_gy, stairs_dir;   /* up-stairs */
    bool has_up;
    int down_gx, down_gy, down_dir;         /* down-stairs (-1 if none) */
    bool has_down;
    /* Alien entities (for FPS view) */
    uint8_t aliens[DNG_GRID_H + 1][DNG_GRID_W + 1]; /* 0=none, 1-4=enemy type (ENEMY_LURKER+1 etc) */
    char alien_names[DNG_GRID_H + 1][DNG_GRID_W + 1][16]; /* individual name per alien */
    /* Console entities — interactable objects at room centers */
    uint8_t consoles[DNG_GRID_H + 1][DNG_GRID_W + 1]; /* 0=none, room_type (ROOM_BRIDGE etc) */
    /* Chests — loot containers */
    uint8_t chests[DNG_GRID_H + 1][DNG_GRID_W + 1]; /* 0=none, 1=chest present */
    /* Room info for ship system */
    #define DNG_MAX_ROOMS 24
    int room_count;
    int room_cx[DNG_MAX_ROOMS], room_cy[DNG_MAX_ROOMS];
    int room_x[DNG_MAX_ROOMS], room_y[DNG_MAX_ROOMS];
    int room_w[DNG_MAX_ROOMS], room_h[DNG_MAX_ROOMS];
    int room_ship_idx[DNG_MAX_ROOMS];
    bool room_light_on[DNG_MAX_ROOMS];
    /* Window faces: bitmask per wall cell (bit0=N, bit1=S, bit2=E, bit3=W) */
    #define DNG_WIN_N 1
    #define DNG_WIN_S 2
    #define DNG_WIN_E 4
    #define DNG_WIN_W 8
    uint8_t win_faces[DNG_GRID_H + 1][DNG_GRID_W + 1];
} sr_dungeon;

/* ── Simple RNG for dungeon generation ───────────────────────────── */

static uint32_t dng_rng_state = 0;

static void dng_rng_seed(uint32_t seed) { dng_rng_state = seed; }

static int dng_rng_int(int max) {
    dng_rng_state = dng_rng_state * 1103515245u + 12345u;
    return (int)(((dng_rng_state >> 16) & 0x7FFF) % (uint32_t)max);
}

/* ── Alien name generation ───────────────────────────────────────── */

#define DNG_ALIEN_NAME_MAX 32
static char dng_alien_prefix_buf[DNG_ALIEN_NAME_MAX][8];
static char dng_alien_suffix_buf[DNG_ALIEN_NAME_MAX][8];
static const char *dng_alien_prefixes[DNG_ALIEN_NAME_MAX];
static const char *dng_alien_suffixes[DNG_ALIEN_NAME_MAX];
static int dng_alien_prefix_count = 0;
static int dng_alien_suffix_count = 0;

/* Default names (used if config not loaded) */
static void dng_alien_names_init_defaults(void) {
    if (dng_alien_prefix_count > 0) return; /* already loaded */
    const char *dp[] = {"ZR","KR","VX","GH","SK","BL","TR","NX","QZ","XL",
                        "DR","MK","PH","SN","GL","FL","TH","CH","WR","SH"};
    const char *ds[] = {"AAK","IKS","ULL","ORM","AXE","ENT","IRE","OKK","URG","ASH",
                        "ILK","UNG","ARN","EEL","OOZ","AWN","IPP","UTH","AGG","ISS"};
    dng_alien_prefix_count = 20;
    dng_alien_suffix_count = 20;
    for (int i = 0; i < 20; i++) {
        snprintf(dng_alien_prefix_buf[i], 8, "%s", dp[i]);
        dng_alien_prefixes[i] = dng_alien_prefix_buf[i];
        snprintf(dng_alien_suffix_buf[i], 8, "%s", ds[i]);
        dng_alien_suffixes[i] = dng_alien_suffix_buf[i];
    }
}

/* Load from comma-separated config string */
static int dng_alien_parse_csv(const char *csv, char buf[][8], const char *ptrs[], int max) {
    int count = 0;
    const char *p = csv;
    while (*p && count < max) {
        while (*p == ' ' || *p == ',') p++;
        if (!*p) break;
        int len = 0;
        while (p[len] && p[len] != ',' && len < 7) len++;
        memcpy(buf[count], p, len);
        buf[count][len] = 0;
        ptrs[count] = buf[count];
        count++;
        p += len;
    }
    return count;
}

static void dng_gen_alien_name(char *out, int max_len) {
    if (dng_alien_prefix_count == 0) dng_alien_names_init_defaults();
    int pi = dng_rng_int(dng_alien_prefix_count);
    int si = dng_rng_int(dng_alien_suffix_count);
    snprintf(out, max_len, "%s-%s", dng_alien_prefixes[pi], dng_alien_suffixes[si]);
}

/* ── Dungeon queries ─────────────────────────────────────────────── */

static inline bool dng_is_wall(const sr_dungeon *d, int gx, int gy) {
    if (gx < 1 || gx > d->w || gy < 1 || gy > d->h) return true;
    return d->map[gy][gx] == 1;
}

static inline bool dng_is_open(const sr_dungeon *d, int gx, int gy) {
    return !dng_is_wall(d, gx, gy);
}

/* Up-stairs can only be entered from the entry side (opposite of stairs_dir) */
static bool dng_can_enter(const sr_dungeon *d, int fx, int fy, int tx, int ty) {
    if (dng_is_wall(d, tx, ty)) return false;
    if (d->has_up && tx == d->stairs_gx && ty == d->stairs_gy) {
        int dir = d->stairs_dir;
        int entry_gx = d->stairs_gx - dng_dir_dx[dir];
        int entry_gy = d->stairs_gy - dng_dir_dz[dir];
        if (fx != entry_gx || fy != entry_gy) return false;
    }
    return true;
}

/* ── Dungeon generation ──────────────────────────────────────────── */

typedef struct { int x, y, w, h, cx, cy; } dng_room;

static void dng_carve_corridor(sr_dungeon *d, int x1, int y1, int x2, int y2) {
    int sx = x2 >= x1 ? 1 : -1;
    for (int x = x1; x != x2 + sx; x += sx)
        if (x >= 1 && x <= d->w && y1 >= 1 && y1 <= d->h)
            d->map[y1][x] = 0;
    int sy = y2 >= y1 ? 1 : -1;
    for (int y = y1; y != y2 + sy; y += sy)
        if (x2 >= 1 && x2 <= d->w && y >= 1 && y <= d->h)
            d->map[y][x2] = 0;
}

static bool dng_find_up_stairs(sr_dungeon *d, const dng_room *room,
                                int *out_x, int *out_y, int *out_dir) {
    for (int py = room->y; py < room->y + room->h; py++) {
        for (int px = room->x; px < room->x + room->w; px++) {
            if (d->map[py][px] != 0) continue;
            for (int dir = 0; dir < 4; dir++) {
                int wx = px + dng_dir_dx[dir], wy = py + dng_dir_dz[dir];
                int ex = px - dng_dir_dx[dir], ey = py - dng_dir_dz[dir];
                bool has_wall = wx < 1 || wx > d->w || wy < 1 || wy > d->h || d->map[wy][wx] == 1;
                bool has_entry = ex >= 1 && ex <= d->w && ey >= 1 && ey <= d->h && d->map[ey][ex] == 0;
                if (has_wall && has_entry) {
                    *out_x = px; *out_y = py; *out_dir = dir;
                    return true;
                }
            }
        }
    }
    /* Fallback: room center */
    *out_x = room->cx; *out_y = room->cy; *out_dir = 2;
    /* Carve entry side */
    int ex = room->cx - dng_dir_dx[2], ey = room->cy - dng_dir_dz[2];
    if (ex >= 1 && ex <= d->w && ey >= 1 && ey <= d->h)
        d->map[ey][ex] = 0;
    return true;
}

static bool dng_all8_open(const sr_dungeon *d, int px, int py) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = px + dx, ny = py + dy;
            if (nx < 1 || nx > d->w || ny < 1 || ny > d->h || d->map[ny][nx] != 0)
                return false;
        }
    return true;
}

static void dng_carve8(sr_dungeon *d, int px, int py) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = px + dx, ny = py + dy;
            if (nx >= 1 && nx <= d->w && ny >= 1 && ny <= d->h)
                d->map[ny][nx] = 0;
        }
}

static void dng_find_down_stairs(sr_dungeon *d, const dng_room *room,
                                  int *out_x, int *out_y) {
    for (int py = room->y; py < room->y + room->h; py++)
        for (int px = room->x; px < room->x + room->w; px++)
            if (d->map[py][px] == 0 && dng_all8_open(d, px, py)) {
                *out_x = px; *out_y = py;
                return;
            }
    /* Fallback: carve around room center */
    dng_carve8(d, room->cx, room->cy);
    *out_x = room->cx; *out_y = room->cy;
}

static void dng_generate_ex(sr_dungeon *d, int w, int h, bool has_down_stairs, bool has_up_stairs, uint32_t seed, int floor_num, int desired_rooms);
static void dng_generate(sr_dungeon *d, int w, int h, bool has_down_stairs, bool has_up_stairs, uint32_t seed, int floor_num) {
    dng_generate_ex(d, w, h, has_down_stairs, has_up_stairs, seed, floor_num, 0);
}
static void dng_generate_ex(sr_dungeon *d, int w, int h, bool has_down_stairs, bool has_up_stairs, uint32_t seed, int floor_num, int desired_rooms) {
    dng_rng_seed(seed);
    memset(d, 0, sizeof(*d));
    d->w = w; d->h = h;
    d->down_gx = -1; d->down_gy = -1;
    d->stairs_gx = -1; d->stairs_gy = -1;
    d->has_down = has_down_stairs;
    d->has_up = has_up_stairs;

    /* Fill with walls */
    for (int y = 1; y <= h; y++)
        for (int x = 1; x <= w; x++)
            d->map[y][x] = 1;

    /* ── FTL-style ship layout ──────────────────────────────────
     * Central corridor runs horizontally through the middle.
     * Rooms branch off port (top) and starboard (bottom) sides.
     * Layout: [Engines] -- [rooms] -- [rooms] -- [Bridge]
     */

    dng_room rooms[DNG_MAX_ROOMS];
    /* Scale room count with grid size */
    int num_rooms;
    if (desired_rooms > 0) {
        num_rooms = desired_rooms;
    } else {
        int max_rooms = (w <= 20) ? 8 : (w <= 40) ? 14 : 22;
        int min_rooms = (w <= 20) ? 5 : (w <= 40) ? 8 : 12;
        num_rooms = min_rooms + dng_rng_int(max_rooms - min_rooms + 1);
    }
    if (num_rooms > DNG_MAX_ROOMS) num_rooms = DNG_MAX_ROOMS;

    /* Scale room sizes with grid */
    int room_min_w = (w <= 20) ? 3 : (w <= 40) ? 4 : 5;
    int room_max_w = (w <= 20) ? 4 : (w <= 40) ? 6 : 8;
    int room_min_h = room_min_w;
    int room_max_h = room_max_w;

    int mid_y = h / 2;
    int corridor_y1 = mid_y;
    int corridor_y2 = mid_y;    /* 1 tile wide */

    int ship_left = 3;
    int ship_right = w - 2;
    int ship_span = ship_right - ship_left;

    /* Carve the central corridor (1 tile wide) */
    for (int x = ship_left; x <= ship_right; x++) {
        if (mid_y >= 1 && mid_y <= h && x >= 1 && x <= w)
            d->map[mid_y][x] = 0;
    }

    /* Place rooms along the corridor with overlap checking */
    int spacing = ship_span / (num_rooms + 1);
    if (spacing < room_max_w + 2) spacing = room_max_w + 2;

    int placed = 0;
    for (int i = 0; i < num_rooms; i++) {
        int rw = room_min_w + dng_rng_int(room_max_w - room_min_w + 1);
        int rh = room_min_h + dng_rng_int(room_max_h - room_min_h + 1);

        int rx = ship_left + spacing * (i + 1) - rw / 2;
        if (rx < 2) rx = 2;
        if (rx + rw > w) rx = w - rw;

        int ry;
        if (i % 2 == 0) {
            ry = corridor_y1 - rh - 1;
            if (ry < 2) ry = 2;
        } else {
            ry = corridor_y2 + 2;
            if (ry + rh > h) ry = h - rh;
        }

        /* Check overlap with existing rooms (1-tile margin) */
        bool overlaps = false;
        for (int j = 0; j < placed; j++) {
            if (rx - 1 < rooms[j].x + rooms[j].w &&
                rx + rw + 1 > rooms[j].x &&
                ry - 1 < rooms[j].y + rooms[j].h &&
                ry + rh + 1 > rooms[j].y) {
                overlaps = true;
                break;
            }
        }
        if (overlaps) continue;

        rooms[placed] = (dng_room){ rx, ry, rw, rh, rx + rw/2, ry + rh/2 };

        /* Carve room */
        for (int py = ry; py < ry + rh; py++)
            for (int px = rx; px < rx + rw; px++)
                if (py >= 1 && py <= h && px >= 1 && px <= w)
                    d->map[py][px] = 0;

        /* Connect room to corridor with a 1-wide doorway through the gap */
        int conn_x = rx + rw / 2;
        if (i % 2 == 0) {
            for (int y = ry + rh; y <= corridor_y1; y++)
                if (y >= 1 && y <= h && conn_x >= 1 && conn_x <= w)
                    d->map[y][conn_x] = 0;
        } else {
            for (int y = corridor_y2 + 1; y < ry; y++)
                if (y >= 1 && y <= h && conn_x >= 1 && conn_x <= w)
                    d->map[y][conn_x] = 0;
        }
        placed++;
    }
    num_rooms = placed;

    /* Spawn at leftmost corridor (airlock entry) */
    d->spawn_gx = ship_left;
    d->spawn_gy = mid_y;
    d->spawn_dir = 1; /* face east into the ship */

    /* Up-stairs at the far end (rightmost corridor) */
    if (has_up_stairs) {
        d->stairs_gx = ship_right;
        d->stairs_gy = mid_y;
        d->stairs_dir = 1; /* facing east */
        /* Ensure the stairs cell and its entry are open */
        if (d->stairs_gx >= 1 && d->stairs_gx <= w && d->stairs_gy >= 1 && d->stairs_gy <= h)
            d->map[d->stairs_gy][d->stairs_gx] = 0;
        int ex = d->stairs_gx - dng_dir_dx[1];
        int ey = d->stairs_gy - dng_dir_dz[1];
        if (ex >= 1 && ex <= w && ey >= 1 && ey <= h)
            d->map[ey][ex] = 0;
    }

    /* Down-stairs near spawn */
    if (has_down_stairs) {
        d->down_gx = ship_left + 1;
        d->down_gy = mid_y;
        d->down_dir = 3; /* facing west */
        if (d->down_gx >= 1 && d->down_gx <= w && d->down_gy >= 1 && d->down_gy <= h)
            d->map[d->down_gy][d->down_gx] = 0;
    }

    /* Store room info for ship system */
    d->room_count = num_rooms;
    for (int i = 0; i < num_rooms; i++) {
        d->room_cx[i] = rooms[i].cx;
        d->room_cy[i] = rooms[i].cy;
        d->room_x[i] = rooms[i].x;
        d->room_y[i] = rooms[i].y;
        d->room_w[i] = rooms[i].w;
        d->room_h[i] = rooms[i].h;
        d->room_ship_idx[i] = -1;
        d->room_light_on[i] = true;  /* lights start on */
    }

    /* Place a pair of windows per room on the outer wall, facing toward the room.
     * Avoid corner tiles (first/last column of the room wall). */
    for (int i = 0; i < num_rooms; i++) {
        int rx = rooms[i].x, ry = rooms[i].y;
        int rw = rooms[i].w, rh = rooms[i].h;
        int cx = rx + rw / 2;
        int wx1 = cx, wx2 = cx - 1; /* pair: center and one left */
        /* Avoid corners: clamp to [rx+1, rx+rw-2] */
        if (wx1 > rx + rw - 2) wx1 = rx + rw - 2;
        if (wx1 < rx + 1) wx1 = rx + 1;
        if (wx2 > rx + rw - 2) wx2 = rx + rw - 2;
        if (wx2 < rx + 1) wx2 = rx + 1;
        if (ry + rh <= mid_y) {
            /* Room above corridor — S window on wall above (faces room) */
            int wy = ry - 1;
            if (wy >= 1 && wy <= h) {
                if (wx1 >= 1 && wx1 <= w && d->map[wy][wx1] == 1)
                    d->win_faces[wy][wx1] |= DNG_WIN_S;
                if (wx2 >= 1 && wx2 <= w && wx2 != wx1 && d->map[wy][wx2] == 1)
                    d->win_faces[wy][wx2] |= DNG_WIN_S;
            }
        } else if (ry > mid_y) {
            /* Room below corridor — N window on wall below (faces room) */
            int wy = ry + rh;
            if (wy >= 1 && wy <= h) {
                if (wx1 >= 1 && wx1 <= w && d->map[wy][wx1] == 1)
                    d->win_faces[wy][wx1] |= DNG_WIN_N;
                if (wx2 >= 1 && wx2 <= w && wx2 != wx1 && d->map[wy][wx2] == 1)
                    d->win_faces[wy][wx2] |= DNG_WIN_N;
            }
        }
    }

    /* Place alien entities (not spawn, not stairs) — ~half the rooms get aliens */
    for (int i = 0; i < num_rooms; i++) {
        if (dng_rng_int(2) == 0) continue; /* skip ~half the rooms */
        int aliens_in_room = 1;
        for (int a = 0; a < aliens_in_room; a++) {
            int ax = rooms[i].x + dng_rng_int(rooms[i].w);
            int ay = rooms[i].y + dng_rng_int(rooms[i].h);
            if (ax < 1 || ax > w || ay < 1 || ay > h) continue;
            if (d->map[ay][ax] != 0) continue;
            if (ax == d->spawn_gx && ay == d->spawn_gy) continue;
            if (d->has_up && ax == d->stairs_gx && ay == d->stairs_gy) continue;
            if (d->has_down && ax == d->down_gx && ay == d->down_gy) continue;
            if (d->consoles[ay][ax] != 0) continue; /* don't place on consoles */
            if (d->aliens[ay][ax] != 0) continue;  /* don't stack on existing alien */
            int etype;
            if (floor_num >= 2) {
                /* Difficulty 2+: evolved tier 2 enemies (STALKER..WARDEN) */
                etype = ENEMY_STALKER + dng_rng_int(4);
            } else {
                /* Difficulty 0-1: tier 1 enemies (LURKER..HIVEGUARD) */
                int max_type = (floor_num <= 0) ? 2 : 4;
                etype = dng_rng_int(max_type);
            }
            d->aliens[ay][ax] = 1 + (uint8_t)etype;
            dng_gen_alien_name(d->alien_names[ay][ax], 16);
        }
    }

    /* Place 1-2 chests per floor in random rooms (guaranteed at least 1) */
    {
        int num_chests = 1 + dng_rng_int(2); /* 1-2 */
        int placed = 0;
        for (int attempt = 0; attempt < num_rooms * 10 && placed < num_chests; attempt++) {
            int ri = dng_rng_int(num_rooms);
            int cx = rooms[ri].x + dng_rng_int(rooms[ri].w);
            int cy = rooms[ri].y + dng_rng_int(rooms[ri].h);
            if (cx < 1 || cx > w || cy < 1 || cy > h) continue;
            if (d->map[cy][cx] != 0) continue;
            if (cx == d->spawn_gx && cy == d->spawn_gy) continue;
            if (d->has_up && cx == d->stairs_gx && cy == d->stairs_gy) continue;
            if (d->has_down && cx == d->down_gx && cy == d->down_gy) continue;
            if (d->consoles[cy][cx] != 0) continue;
            if (d->aliens[cy][cx] != 0) continue;
            if (d->chests[cy][cx] != 0) continue;
            d->chests[cy][cx] = 1;
            placed++;
        }
        /* Fallback: brute-force scan to guarantee at least 1 chest */
        if (placed == 0) {
            for (int ri = 0; ri < num_rooms && placed == 0; ri++) {
                for (int cy = rooms[ri].y; cy < rooms[ri].y + rooms[ri].h && placed == 0; cy++)
                    for (int cx = rooms[ri].x; cx < rooms[ri].x + rooms[ri].w && placed == 0; cx++) {
                        if (cx < 1 || cx > w || cy < 1 || cy > h) continue;
                        if (d->map[cy][cx] != 0) continue;
                        if (cx == d->spawn_gx && cy == d->spawn_gy) continue;
                        if (d->consoles[cy][cx] != 0 || d->aliens[cy][cx] != 0) continue;
                        d->chests[cy][cx] = 1;
                        placed++;
                    }
            }
        }
    }
}

/* ── Room lookup ─────────────────────────────────────────────────── */

/* Find which room index a grid position belongs to (-1 if none) */
static int dng_room_at(const sr_dungeon *d, int gx, int gy) {
    for (int i = 0; i < d->room_count; i++) {
        if (gx >= d->room_x[i] && gx < d->room_x[i] + d->room_w[i] &&
            gy >= d->room_y[i] && gy < d->room_y[i] + d->room_h[i])
            return i;
    }
    return -1;
}

/* ── Player ──────────────────────────────────────────────────────── */

typedef struct {
    int gx, gy;         /* grid position (1-indexed) */
    int dir;            /* facing: 0=N, 1=E, 2=S, 3=W */
    float x, z;         /* smooth world position */
    float y;            /* vertical offset (for stair climb) */
    float angle;        /* smooth facing angle (turns) */
    float target_x, target_z;
    float target_angle;
    int bounce_timer;    /* >0 = bouncing back from blocked tile */
    float bounce_mid_x, bounce_mid_z; /* midpoint to animate toward before snapping back */
    double last_move_time; /* timestamp of last successful move (seconds) */
} dng_player;

static void (*dng_on_move_callback)(void) = NULL;

static void dng_player_init(dng_player *p, int gx, int gy, int dir) {
    p->gx = gx; p->gy = gy; p->dir = dir;
    p->x = (gx - 0.5f) * DNG_CELL_SIZE;
    p->z = (gy - 0.5f) * DNG_CELL_SIZE;
    p->y = 0;
    p->target_x = p->x;
    p->target_z = p->z;
    p->angle = dir * 0.25f;
    p->target_angle = p->angle;
    p->bounce_timer = 0;
}

#define DNG_MOVE_INTERVAL 0.2  /* seconds between moves (= 5 cells/sec) */
#define DNG_MOVE_INTERVAL_INSTANT 0.05  /* faster rate for instant step mode */

static double dng_time = 0; /* global time accumulator, updated each frame */
static bool dng_instant_step = false; /* true = snap position, fast repeat */

static void dng_player_try_move(dng_player *p, const sr_dungeon *d, int dir) {
    if (p->bounce_timer > 0) return; /* blocked during bounce-back */
    double interval = dng_instant_step ? DNG_MOVE_INTERVAL_INSTANT : DNG_MOVE_INTERVAL;
    if (dng_time - p->last_move_time < interval) return; /* rate limited */
    int nx = p->gx + dng_dir_dx[dir];
    int ny = p->gy + dng_dir_dz[dir];
    if (dng_can_enter(d, p->gx, p->gy, nx, ny)) {
        p->gx = nx;
        p->gy = ny;
        p->target_x = (p->gx - 0.5f) * DNG_CELL_SIZE;
        p->target_z = (p->gy - 0.5f) * DNG_CELL_SIZE;
        p->last_move_time = dng_time;
        if (dng_on_move_callback) dng_on_move_callback();
    }
}

static void dng_player_update(dng_player *p) {
    if (p->bounce_timer > 0) {
        p->bounce_timer--;
        if (p->bounce_timer > 5) {
            /* First phase: move toward the blocked tile midpoint */
            p->x += (p->bounce_mid_x - p->x) * 0.25f;
            p->z += (p->bounce_mid_z - p->z) * 0.25f;
        } else {
            /* Second phase: snap back to grid target */
            p->x += (p->target_x - p->x) * 0.3f;
            p->z += (p->target_z - p->z) * 0.3f;
        }
    } else {
        p->x += (p->target_x - p->x) * DNG_MOVE_SMOOTH;
        p->z += (p->target_z - p->z) * DNG_MOVE_SMOOTH;
    }
    p->angle += (p->target_angle - p->angle) * DNG_TURN_SMOOTH;
}

/* ── Visibility (DDA raycast) ────────────────────────────────────── */

#define DNG_VIS_SIZE (DNG_GRID_W + 2) * (DNG_GRID_H + 2)

static bool dng_vis[DNG_GRID_H + 2][DNG_GRID_W + 2];

static void dng_build_visibility(const dng_player *p, const sr_dungeon *d) {
    /* BFS flood-fill: spread from player through open cells, stop at walls.
     * Wall cells adjacent to visible open cells are also marked visible
     * so their faces render correctly. */
    memset(dng_vis, 0, sizeof(dng_vis));

    int pgx = p->gx, pgz = p->gy;

    /* BFS queue (grid coords packed as gy * stride + gx) */
    #define VIS_Q_SIZE ((DNG_RENDER_R * 2 + 1) * (DNG_RENDER_R * 2 + 1))
    int queue[VIS_Q_SIZE];
    int qhead = 0, qtail = 0;

    /* Visited array for BFS (separate from dng_vis — tracks open cells we've queued) */
    static bool visited[DNG_GRID_H + 2][DNG_GRID_W + 2];
    memset(visited, 0, sizeof(visited));

    /* Seed with player cell */
    if (pgx >= 1 && pgx <= d->w && pgz >= 1 && pgz <= d->h) {
        queue[qtail++] = pgz * (DNG_GRID_W + 2) + pgx;
        visited[pgz][pgx] = true;
        dng_vis[pgz][pgx] = true;
    }

    while (qhead < qtail) {
        int packed = queue[qhead++];
        int cy = packed / (DNG_GRID_W + 2);
        int cx = packed % (DNG_GRID_W + 2);

        /* Explore 4 cardinal neighbors */
        for (int dir = 0; dir < 4; dir++) {
            int nx = cx + dng_dir_dx[dir];
            int nz = cy + dng_dir_dz[dir];

            /* Bounds check */
            if (nx < 1 || nx > d->w || nz < 1 || nz > d->h) continue;

            /* Distance check (Manhattan would miss corners, use Chebyshev) */
            int ddx = nx - pgx; if (ddx < 0) ddx = -ddx;
            int ddz = nz - pgz; if (ddz < 0) ddz = -ddz;
            if (ddx > dng_render_radius || ddz > dng_render_radius) continue;

            if (d->map[nz][nx] == 1) {
                /* Wall cell — mark visible (for face rendering) but don't spread through */
                dng_vis[nz][nx] = true;
            } else if (!visited[nz][nx]) {
                /* Open cell — mark visible and continue BFS */
                visited[nz][nx] = true;
                dng_vis[nz][nx] = true;
                if (qtail < VIS_Q_SIZE)
                    queue[qtail++] = nz * (DNG_GRID_W + 2) + nx;
            }
        }
    }
    #undef VIS_Q_SIZE
}

/* ── Climb animation ─────────────────────────────────────────────── */

typedef struct {
    bool active;
    bool direction_up;  /* true=climbing up, false=climbing down */
    int phase;          /* 0=walk_in, 1=walk_out */
    int timer;
    float start_x, start_z, start_y, start_angle;
    float end_x, end_z, end_angle;
    float arrival_y;
    float exit_x, exit_z;  /* walk-out destination on new floor */
} dng_climb;

/* ── Game state ──────────────────────────────────────────────────── */

typedef struct {
    sr_dungeon floors[DNG_MAX_FLOORS];
    bool floor_generated[DNG_MAX_FLOORS];
    int current_floor;
    sr_dungeon *dungeon;   /* points into floors[] */
    dng_player player;
    dng_climb climb;
    bool on_stairs;        /* suppress re-trigger after climb */
    uint32_t seed_base;
    int max_floors;        /* set to ship num_decks; caps stair generation */
    int deck_room_counts[DNG_MAX_FLOORS]; /* rooms per deck (from ship) */
    int grid_w, grid_h;    /* actual grid size for this ship (20, 40, or 80) */
} dng_game;

static void dng_game_init_sized(dng_game *g, int gw, int gh) {
    memset(g, 0, sizeof(*g));
    g->seed_base = 42;
    g->current_floor = 0;
    g->grid_w = gw > 0 ? gw : 20;
    g->grid_h = gh > 0 ? gh : 20;

    /* Generate floor 0 (stairs added later by game_init_ship if needed) */
    dng_generate(&g->floors[0], g->grid_w, g->grid_h, false, false,
                 g->seed_base, 0);
    g->floor_generated[0] = true;
    g->dungeon = &g->floors[0];

    dng_player_init(&g->player, g->dungeon->spawn_gx, g->dungeon->spawn_gy, g->dungeon->spawn_dir);
}

static void dng_game_init(dng_game *g) {
    dng_game_init_sized(g, 20, 20);
}

static void dng_go_up(dng_game *g) {
    int cap = g->max_floors > 0 ? g->max_floors : DNG_MAX_FLOORS;
    g->current_floor++;
    if (g->current_floor >= cap) { g->current_floor--; return; }

    if (!g->floor_generated[g->current_floor]) {
        bool is_last = (g->current_floor >= cap - 1);
        int dr = g->deck_room_counts[g->current_floor];
        dng_generate_ex(&g->floors[g->current_floor], g->grid_w, g->grid_h,
                        true, !is_last, g->seed_base + (uint32_t)g->current_floor * 777,
                        g->current_floor, dr);
        g->floor_generated[g->current_floor] = true;
    }
    g->dungeon = &g->floors[g->current_floor];

    int face = (g->dungeon->down_dir + 2) % 4;
    dng_player_init(&g->player,
        g->dungeon->down_gx + dng_dir_dx[face],
        g->dungeon->down_gy + dng_dir_dz[face], face);
}

static void dng_go_down(dng_game *g) {
    if (g->current_floor <= 0) return;
    g->current_floor--;
    g->dungeon = &g->floors[g->current_floor];

    int face = (g->dungeon->stairs_dir + 2) % 4;
    dng_player_init(&g->player,
        g->dungeon->stairs_gx + dng_dir_dx[face],
        g->dungeon->stairs_gy + dng_dir_dz[face], face);
}

static void dng_start_climb(dng_game *g, bool going_up) {
    dng_climb *c = &g->climb;
    c->active = true;
    c->direction_up = going_up;
    c->phase = 0; /* ease_out */
    c->timer = 0;
    c->start_x = g->player.x;
    c->start_z = g->player.z;
    c->start_y = g->player.y;
    c->start_angle = g->player.angle;

    int target_gx, target_gy, stair_dir;
    if (going_up) {
        target_gx = g->dungeon->stairs_gx;
        target_gy = g->dungeon->stairs_gy;
        stair_dir = g->dungeon->stairs_dir;
    } else {
        target_gx = g->dungeon->down_gx;
        target_gy = g->dungeon->down_gy;
        stair_dir = g->dungeon->down_dir;
    }

    c->end_x = (target_gx - 0.5f) * DNG_CELL_SIZE;
    c->end_z = (target_gy - 0.5f) * DNG_CELL_SIZE;

    /* Find shortest rotation to face stairs direction */
    float target_a = stair_dir * 0.25f;
    float da = target_a - g->player.angle;
    while (da > 0.5f) da -= 1.0f;
    while (da < -0.5f) da += 1.0f;
    c->end_angle = g->player.angle + da;
}

/* Smoothstep */
static inline float dng_smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

/* Returns true if climb finished */
static bool dng_update_climb(dng_game *g) {
    dng_climb *c = &g->climb;
    if (!c->active) return false;
    c->timer++;

    if (c->phase == 0) { /* walk into stairs + rise/sink */
        float peak = c->direction_up ? DNG_CLIMB_UP_HEIGHT : -DNG_CLIMB_DOWN_HEIGHT;
        float t = (float)c->timer / DNG_CLIMB_MOVE_FRAMES;
        if (t >= 1.0f) {
            t = 1.0f;
            /* Teleport to new floor — player_init places at adjacent cell */
            if (c->direction_up) dng_go_up(g);
            else dng_go_down(g);

            /* Exit position = where player_init placed us (adjacent to stairs) */
            c->exit_x = g->player.x;
            c->exit_z = g->player.z;

            /* Start walk-out FROM the stairs cell itself */
            int stair_gx, stair_gy;
            if (c->direction_up) {
                stair_gx = g->dungeon->down_gx;
                stair_gy = g->dungeon->down_gy;
            } else {
                stair_gx = g->dungeon->stairs_gx;
                stair_gy = g->dungeon->stairs_gy;
            }
            c->start_x = (stair_gx - 0.5f) * DNG_CELL_SIZE;
            c->start_z = (stair_gy - 0.5f) * DNG_CELL_SIZE;
            g->player.x = c->start_x;
            g->player.z = c->start_z;

            float arrival_y = c->direction_up ? -DNG_CLIMB_UP_HEIGHT : DNG_CLIMB_DOWN_HEIGHT;
            c->phase = 1;
            c->timer = 0;
            g->player.y = arrival_y;
            c->arrival_y = arrival_y;
            return false;
        }
        /* Map phase 0 time to first half of smoothstep: 0→0.5 */
        float st = dng_smoothstep(t * 0.5f) * 2.0f;
        g->player.x = c->start_x + (c->end_x - c->start_x) * st;
        g->player.z = c->start_z + (c->end_z - c->start_z) * st;
        g->player.y = c->start_y + (peak - c->start_y) * st;
        g->player.angle = c->start_angle + (c->end_angle - c->start_angle) * st;
    } else { /* walk out of stairs + settle Y */
        float t = (float)c->timer / DNG_CLIMB_SETTLE_FRAMES;
        if (t >= 1.0f) {
            g->player.x = c->exit_x;
            g->player.z = c->exit_z;
            g->player.target_x = c->exit_x;
            g->player.target_z = c->exit_z;
            g->player.y = 0;
            c->active = false;
            g->on_stairs = true;
            return true;
        }
        /* Map phase 1 time to second half of smoothstep: 0.5→1 */
        float st = (dng_smoothstep(0.5f + t * 0.5f) - 0.5f) * 2.0f;
        g->player.x = c->start_x + (c->exit_x - c->start_x) * st;
        g->player.z = c->start_z + (c->exit_z - c->start_z) * st;
        g->player.y = c->arrival_y * (1.0f - st);
    }
    return false;
}

/* ══════════════════════════════════════════════════════════════════
 *  Enemy AI system — wandering, chasing, A* pathfinding
 * ══════════════════════════════════════════════════════════════════ */

#define DNG_MAX_ENEMIES   64
#define DNG_ASTAR_MAX     256   /* max open-set nodes for A* */
#define DNG_PATH_MAX      32    /* max steps stored per enemy path */
#define DNG_PATROL_MAX    4     /* max waypoints per patrol route */
#define DNG_ALERT_RADIUS  12    /* chase lock-on distance for hallway enemies */
#define DNG_CHASE_RANGE    6    /* max distance to chase before giving up */

enum {
    DNG_AI_IDLE,      /* standing still in a room, hasn't seen player */
    DNG_AI_WANDER,    /* slowly moving to random spots within room */
    DNG_AI_CHASE,     /* pursuing the player (A*) */
    DNG_AI_PATROL,    /* hallway enemy walking between waypoints */
    DNG_AI_RETURN,    /* lost sight of player, returning to spawn point */
};

typedef struct {
    bool alive;
    int  gx, gy;          /* grid position */
    uint8_t type;          /* alien type value (1-4, same as aliens[][]) */
    int  ai_state;         /* DNG_AI_* */
    int  prev_ai_state;    /* state before chase (to restore after returning) */
    int  home_room;        /* room index this enemy belongs to (-1 = hallway) */
    int  spawn_gx, spawn_gy; /* starting position (return here when losing player) */
    /* Smooth movement lerp */
    float lerp_x, lerp_y; /* visual position (world coords, lerps toward gx/gy) */
    /* Pathfinding */
    int  path[DNG_PATH_MAX * 2]; /* gx,gy pairs; path[0..1]=next step */
    int  path_len;         /* number of steps remaining */
    /* Wander */
    int  wander_cooldown;  /* ticks until next wander step */
    /* Patrol (hallway enemies) */
    int  patrol_pts[DNG_PATROL_MAX * 2]; /* gx,gy waypoints */
    int  patrol_count;     /* number of waypoints */
    int  patrol_idx;       /* current target waypoint */
    bool patrol_forward;   /* direction along patrol path */
} dng_enemy;

/* Enemy storage lives alongside the dungeon */
static dng_enemy dng_enemies[DNG_MAX_ENEMIES];
static int       dng_enemy_count = 0;

/* ── A* pathfinding ─────────────────────────────────────────────── */

typedef struct { int gx, gy, g, f, parent; } dng_astar_node;

static bool dng_astar_blocked(const sr_dungeon *d, int gx, int gy,
                               int ignore_gx, int ignore_gy) {
    if (dng_is_wall(d, gx, gy)) return true;
    if (d->consoles[gy][gx] != 0) return true;
    /* Check for other enemies on this tile (but ignore our own position) */
    if (gx == ignore_gx && gy == ignore_gy) return false;
    if (d->aliens[gy][gx] != 0) return true;
    return false;
}

/* Manhattan distance heuristic */
static inline int dng_astar_h(int ax, int ay, int bx, int by) {
    int dx = ax - bx; if (dx < 0) dx = -dx;
    int dy = ay - by; if (dy < 0) dy = -dy;
    return dx + dy;
}

/* A* from (sx,sy) to (tx,ty). Writes path into out_path (gx,gy pairs),
 * returns number of steps (0 = no path). Path is from FIRST step to target. */
static int dng_astar(const sr_dungeon *d, int sx, int sy, int tx, int ty,
                     int *out_path, int max_steps) {
    if (sx == tx && sy == ty) return 0;

    static dng_astar_node open_list[DNG_ASTAR_MAX];
    static dng_astar_node closed_list[DNG_ASTAR_MAX];
    int open_count = 0, closed_count = 0;

    /* Seed start node */
    open_list[open_count++] = (dng_astar_node){
        sx, sy, 0, dng_astar_h(sx, sy, tx, ty), -1
    };

    while (open_count > 0) {
        /* Find lowest f in open list */
        int best = 0;
        for (int i = 1; i < open_count; i++)
            if (open_list[i].f < open_list[best].f) best = i;

        dng_astar_node cur = open_list[best];
        /* Remove from open list */
        open_list[best] = open_list[--open_count];

        /* Check if reached goal */
        if (cur.gx == tx && cur.gy == ty) {
            /* Reconstruct path from closed list */
            int trace[DNG_ASTAR_MAX * 2];
            int trace_len = 0;
            trace[trace_len * 2] = cur.gx;
            trace[trace_len * 2 + 1] = cur.gy;
            trace_len++;
            int pi = cur.parent;
            while (pi >= 0 && trace_len < DNG_ASTAR_MAX) {
                trace[trace_len * 2] = closed_list[pi].gx;
                trace[trace_len * 2 + 1] = closed_list[pi].gy;
                trace_len++;
                pi = closed_list[pi].parent;
            }
            /* Reverse (skip the start node) into out_path */
            int steps = 0;
            for (int i = trace_len - 2; i >= 0 && steps < max_steps; i--) {
                out_path[steps * 2] = trace[i * 2];
                out_path[steps * 2 + 1] = trace[i * 2 + 1];
                steps++;
            }
            return steps;
        }

        /* Add to closed list */
        if (closed_count >= DNG_ASTAR_MAX) break; /* out of space */
        int closed_idx = closed_count;
        closed_list[closed_count++] = cur;

        /* Expand 4 neighbors */
        for (int dir = 0; dir < 4; dir++) {
            int nx = cur.gx + dng_dir_dx[dir];
            int ny = cur.gy + dng_dir_dz[dir];

            /* Allow stepping onto the target even if an enemy is there (player tile) */
            bool blocked;
            if (nx == tx && ny == ty) {
                blocked = dng_is_wall(d, nx, ny); /* only walls block the target tile */
            } else {
                blocked = dng_astar_blocked(d, nx, ny, sx, sy);
            }
            if (blocked) continue;

            /* Check if in closed list */
            bool in_closed = false;
            for (int c = 0; c < closed_count; c++)
                if (closed_list[c].gx == nx && closed_list[c].gy == ny)
                    { in_closed = true; break; }
            if (in_closed) continue;

            int ng = cur.g + 1;
            int nf = ng + dng_astar_h(nx, ny, tx, ty);

            /* Check if in open list with better g */
            bool in_open = false;
            for (int o = 0; o < open_count; o++) {
                if (open_list[o].gx == nx && open_list[o].gy == ny) {
                    in_open = true;
                    if (ng < open_list[o].g) {
                        open_list[o].g = ng;
                        open_list[o].f = nf;
                        open_list[o].parent = closed_idx;
                    }
                    break;
                }
            }
            if (!in_open && open_count < DNG_ASTAR_MAX) {
                open_list[open_count++] = (dng_astar_node){
                    nx, ny, ng, nf, closed_idx
                };
            }
        }
    }
    return 0; /* no path found */
}

/* ── Line-of-sight check (Bresenham) ───────────────────────────── */

static bool dng_has_los(const sr_dungeon *d, int x0, int y0, int x1, int y1) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    int cx = x0, cy = y0;
    while (cx != x1 || cy != y1) {
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx)  { err += dx; cy += sy; }
        /* Hit destination = can see */
        if (cx == x1 && cy == y1) return true;
        /* Hit wall = blocked */
        if (dng_is_wall(d, cx, cy)) return false;
    }
    return true;
}

/* ── Update enemy lerp positions (call each frame) ─────────────── */

static void dng_enemies_lerp_update(float dt) {
    float speed = 8.0f; /* cells per second */
    for (int i = 0; i < dng_enemy_count; i++) {
        dng_enemy *e = &dng_enemies[i];
        if (!e->alive) continue;
        float tx = (e->gx - 0.5f) * DNG_CELL_SIZE;
        float ty = (e->gy - 0.5f) * DNG_CELL_SIZE;
        float dx = tx - e->lerp_x;
        float dy = ty - e->lerp_y;
        float dist = sqrtf(dx * dx + dy * dy);
        float step = speed * DNG_CELL_SIZE * dt;
        if (dist <= step) {
            e->lerp_x = tx;
            e->lerp_y = ty;
        } else {
            e->lerp_x += dx / dist * step;
            e->lerp_y += dy / dist * step;
        }
    }
}

/* ── Initialize enemy entities from aliens[][] grid ─────────────── */

static void dng_enemies_init(sr_dungeon *d) {
    dng_enemy_count = 0;
    for (int gy = 1; gy <= d->h && dng_enemy_count < DNG_MAX_ENEMIES; gy++) {
        for (int gx = 1; gx <= d->w && dng_enemy_count < DNG_MAX_ENEMIES; gx++) {
            if (d->aliens[gy][gx] == 0) continue;
            dng_enemy *e = &dng_enemies[dng_enemy_count++];
            memset(e, 0, sizeof(*e));
            e->alive = true;
            e->gx = gx;
            e->gy = gy;
            e->spawn_gx = gx;
            e->spawn_gy = gy;
            e->lerp_x = (gx - 0.5f) * DNG_CELL_SIZE;
            e->lerp_y = (gy - 0.5f) * DNG_CELL_SIZE;
            e->type = d->aliens[gy][gx];
            e->home_room = dng_room_at(d, gx, gy);
            e->ai_state = (e->home_room >= 0) ? DNG_AI_WANDER : DNG_AI_PATROL;
            e->prev_ai_state = e->ai_state;
            e->wander_cooldown = 2 + dng_rng_int(4); /* stagger initial wander */
            e->patrol_forward = true;
        }
    }
}

/* ── Spawn hallway patrol enemies ───────────────────────────────── */

/* Find corridor tiles (open, not in any room, not on stairs/spawn) */
static void dng_spawn_hallway_enemies(sr_dungeon *d, int floor_num) {
    /* Collect corridor tiles */
    int corridor_tiles[512 * 2]; /* gx, gy pairs */
    int corridor_count = 0;

    for (int gy = 1; gy <= d->h; gy++) {
        for (int gx = 1; gx <= d->w; gx++) {
            if (d->map[gy][gx] != 0) continue; /* wall */
            if (dng_room_at(d, gx, gy) >= 0) continue; /* in a room */
            if (gx == d->spawn_gx && gy == d->spawn_gy) continue;
            if (d->has_up && gx == d->stairs_gx && gy == d->stairs_gy) continue;
            if (d->has_down && gx == d->down_gx && gy == d->down_gy) continue;
            if (d->aliens[gy][gx] != 0) continue;
            if (d->consoles[gy][gx] != 0) continue;
            if (corridor_count < 512) {
                corridor_tiles[corridor_count * 2] = gx;
                corridor_tiles[corridor_count * 2 + 1] = gy;
                corridor_count++;
            }
        }
    }

    if (corridor_count < 4) return; /* not enough corridor space */

    /* Spawn 1-2 hallway patrol enemies */
    int num_patrol = 1 + dng_rng_int(2);
    for (int p = 0; p < num_patrol && dng_enemy_count < DNG_MAX_ENEMIES; p++) {
        /* Pick a random corridor tile */
        int ci = dng_rng_int(corridor_count);
        int sgx = corridor_tiles[ci * 2];
        int sgy = corridor_tiles[ci * 2 + 1];

        /* Verify still empty */
        if (d->aliens[sgy][sgx] != 0) continue;

        uint8_t etype;
        if (floor_num >= 2) {
            /* Evolved: STALKER or ACID_THROWER in hallways (fast/ranged) */
            etype = (uint8_t)(1 + (dng_rng_int(2) == 0 ? ENEMY_STALKER : ENEMY_ACID_THROWER));
        } else {
            int max_type = (floor_num <= 0) ? 2 : 4;
            do { etype = 1 + (uint8_t)dng_rng_int(max_type); } while (etype == 2); /* skip brute in hallways */
        }
        d->aliens[sgy][sgx] = etype;
        dng_gen_alien_name(d->alien_names[sgy][sgx], 16);

        /* Create entity */
        dng_enemy *e = &dng_enemies[dng_enemy_count++];
        memset(e, 0, sizeof(*e));
        e->alive = true;
        e->gx = sgx;
        e->gy = sgy;
        e->spawn_gx = sgx;
        e->spawn_gy = sgy;
        e->lerp_x = (sgx - 0.5f) * DNG_CELL_SIZE;
        e->lerp_y = (sgy - 0.5f) * DNG_CELL_SIZE;
        e->type = etype;
        e->home_room = -1;
        e->ai_state = DNG_AI_PATROL;
        e->prev_ai_state = DNG_AI_PATROL;
        e->patrol_forward = true;
        e->wander_cooldown = 0;

        /* Build patrol path: pick 2-3 nearby rooms as waypoints */
        e->patrol_count = 0;
        /* First waypoint = spawn position */
        e->patrol_pts[0] = sgx;
        e->patrol_pts[1] = sgy;
        e->patrol_count = 1;

        /* Find nearest rooms and use their doorway-adjacent corridor tiles */
        for (int ri = 0; ri < d->room_count && e->patrol_count < DNG_PATROL_MAX; ri++) {
            int rcx = d->room_cx[ri];
            int rcy = d->room_cy[ri];
            int dist = dng_astar_h(sgx, sgy, rcx, rcy);
            if (dist < 15 && dist > 2) {
                /* Use the corridor tile closest to this room's center
                 * (the room connector is at room center x, on the corridor) */
                int conn_x = d->room_x[ri] + d->room_w[ri] / 2;
                /* Check which side the room is on relative to corridor */
                int mid_y = d->h / 2;
                int conn_y = mid_y; /* corridor y */
                if (conn_x >= 1 && conn_x <= d->w && conn_y >= 1 && conn_y <= d->h
                    && d->map[conn_y][conn_x] == 0) {
                    e->patrol_pts[e->patrol_count * 2] = conn_x;
                    e->patrol_pts[e->patrol_count * 2 + 1] = conn_y;
                    e->patrol_count++;
                }
            }
        }
        e->patrol_idx = 0;
    }
}

/* ── Move an enemy one step along its path ──────────────────────── */

static void dng_enemy_move_step(dng_enemy *e, sr_dungeon *d) {
    /* Bosses never move from their room */
    if (e->type >= ENEMY_BOSS_1 && e->type <= ENEMY_BOSS_3) return;
    if (e->path_len <= 0) return;
    int nx = e->path[0];
    int ny = e->path[1];

    /* Verify target is not blocked by wall, console, or another enemy */
    if (dng_is_wall(d, nx, ny) || d->consoles[ny][nx] != 0 || d->aliens[ny][nx] != 0) {
        e->path_len = 0; /* path invalidated, will recalc next tick */
        return;
    }

    /* Save name before clearing old position */
    char name_buf[16];
    memcpy(name_buf, d->alien_names[e->gy][e->gx], 16);

    /* Clear old position */
    d->aliens[e->gy][e->gx] = 0;
    memset(d->alien_names[e->gy][e->gx], 0, 16);

    /* Move to new position */
    e->gx = nx;
    e->gy = ny;
    d->aliens[ny][nx] = e->type;
    memcpy(d->alien_names[ny][nx], name_buf, 16);

    /* Shift path */
    for (int i = 0; i < (e->path_len - 1) * 2; i++)
        e->path[i] = e->path[i + 2];
    e->path_len--;
}

/* ── Tick all enemies (call once per player move) ───────────────── */

static void dng_enemies_tick(sr_dungeon *d, int player_gx, int player_gy) {
    int player_room = dng_room_at(d, player_gx, player_gy);

    for (int i = 0; i < dng_enemy_count; i++) {
        dng_enemy *e = &dng_enemies[i];
        if (!e->alive) continue;
        /* Verify entity matches grid (may have been killed in combat) */
        if (d->aliens[e->gy][e->gx] != e->type) {
            e->alive = false;
            continue;
        }

        int enemy_room = dng_room_at(d, e->gx, e->gy);
        int dist_to_player = dng_astar_h(e->gx, e->gy, player_gx, player_gy);
        bool can_see_player = dist_to_player <= DNG_ALERT_RADIUS &&
                              dng_has_los(d, e->gx, e->gy, player_gx, player_gy);

        /* ── Aggro: acquire player target ── */
        if (e->ai_state != DNG_AI_CHASE && e->ai_state != DNG_AI_RETURN) {
            bool aggro = false;
            /* Room alert: player enters a room → all enemies in that room chase */
            if (player_room >= 0 && enemy_room == player_room) aggro = true;
            /* Hallway enemies lock on when they can see the player */
            if (e->home_room < 0 && can_see_player) aggro = true;
            /* Any enemy that sees player nearby */
            if (can_see_player && dist_to_player <= 3) aggro = true;

            if (aggro) {
                e->prev_ai_state = e->ai_state;
                e->ai_state = DNG_AI_CHASE;
            }
        }

        /* ── De-aggro: lost sight OR too far → return to spawn ── */
        if (e->ai_state == DNG_AI_CHASE) {
            if (!can_see_player || dist_to_player > DNG_CHASE_RANGE)
                e->ai_state = DNG_AI_RETURN;
        }
        /* Re-aggro during return if player comes back into view AND in range */
        if (e->ai_state == DNG_AI_RETURN && can_see_player && dist_to_player <= DNG_CHASE_RANGE) {
            e->ai_state = DNG_AI_CHASE;
        }

        switch (e->ai_state) {
        case DNG_AI_IDLE:
            /* Do nothing */
            break;

        case DNG_AI_WANDER: {
            if (e->wander_cooldown > 0) { e->wander_cooldown--; break; }
            e->wander_cooldown = 2 + dng_rng_int(4); /* 2-5 ticks between moves */

            /* Pick a random open neighbor within the room */
            int dirs[4] = {0, 1, 2, 3};
            /* Shuffle */
            for (int s = 3; s > 0; s--) {
                int j = dng_rng_int(s + 1);
                int tmp = dirs[s]; dirs[s] = dirs[j]; dirs[j] = tmp;
            }
            for (int di = 0; di < 4; di++) {
                int nx = e->gx + dng_dir_dx[dirs[di]];
                int ny = e->gy + dng_dir_dz[dirs[di]];
                if (dng_is_wall(d, nx, ny)) continue;
                if (d->consoles[ny][nx] != 0) continue;
                if (d->aliens[ny][nx] != 0) continue;
                if (nx == player_gx && ny == player_gy) continue;
                /* Stay within home room if we have one */
                if (e->home_room >= 0 && dng_room_at(d, nx, ny) != e->home_room) continue;
                /* Move directly (one step) */
                e->path[0] = nx; e->path[1] = ny;
                e->path_len = 1;
                dng_enemy_move_step(e, d);
                break;
            }
            break;
        }

        case DNG_AI_CHASE: {
            /* Don't move if already adjacent to player (distance 1) */
            if (dist_to_player <= 1) break;

            /* Pathfind toward player */
            int path_buf[DNG_PATH_MAX * 2];
            int steps = dng_astar(d, e->gx, e->gy, player_gx, player_gy,
                                  path_buf, DNG_PATH_MAX);
            if (steps > 0) {
                /* Take one step */
                int nx = path_buf[0];
                int ny = path_buf[1];
                /* Don't step onto the player */
                if (nx == player_gx && ny == player_gy) break;
                e->path[0] = nx; e->path[1] = ny;
                e->path_len = 1;
                dng_enemy_move_step(e, d);
            }
            /* If no path, stay put (blocked) */
            break;
        }

        case DNG_AI_RETURN: {
            /* Walk back to spawn point */
            if (e->gx == e->spawn_gx && e->gy == e->spawn_gy) {
                /* Arrived home — restore previous AI state */
                e->ai_state = e->prev_ai_state;
                e->wander_cooldown = 2 + dng_rng_int(4);
                break;
            }
            int ret_buf[DNG_PATH_MAX * 2];
            int ret_steps = dng_astar(d, e->gx, e->gy, e->spawn_gx, e->spawn_gy,
                                      ret_buf, DNG_PATH_MAX);
            if (ret_steps > 0) {
                int rnx = ret_buf[0];
                int rny = ret_buf[1];
                if (rnx != player_gx || rny != player_gy) {
                    e->path[0] = rnx; e->path[1] = rny;
                    e->path_len = 1;
                    dng_enemy_move_step(e, d);
                }
            }
            break;
        }

        case DNG_AI_PATROL: {
            /* Check if should switch to chase (handled above) */
            if (e->patrol_count < 2) {
                /* Not enough waypoints, just wander */
                e->ai_state = DNG_AI_WANDER;
                break;
            }

            int tgx = e->patrol_pts[e->patrol_idx * 2];
            int tgy = e->patrol_pts[e->patrol_idx * 2 + 1];

            /* Reached waypoint? */
            if (e->gx == tgx && e->gy == tgy) {
                if (e->patrol_forward) {
                    e->patrol_idx++;
                    if (e->patrol_idx >= e->patrol_count) {
                        e->patrol_idx = e->patrol_count - 2;
                        e->patrol_forward = false;
                    }
                } else {
                    e->patrol_idx--;
                    if (e->patrol_idx < 0) {
                        e->patrol_idx = 1;
                        e->patrol_forward = true;
                    }
                }
                tgx = e->patrol_pts[e->patrol_idx * 2];
                tgy = e->patrol_pts[e->patrol_idx * 2 + 1];
            }

            /* Pathfind toward current waypoint */
            int path_buf[DNG_PATH_MAX * 2];
            int steps = dng_astar(d, e->gx, e->gy, tgx, tgy,
                                  path_buf, DNG_PATH_MAX);
            if (steps > 0) {
                int nx = path_buf[0];
                int ny = path_buf[1];
                if (nx != player_gx || ny != player_gy) {
                    e->path[0] = nx; e->path[1] = ny;
                    e->path_len = 1;
                    dng_enemy_move_step(e, d);
                }
            }
            break;
        }
        }
    }
}

#endif /* SR_DUNGEON_H */
