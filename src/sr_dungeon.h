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

/* ── Configuration ───────────────────────────────────────────────── */

#define DNG_GRID_W       20
#define DNG_GRID_H       20
#define DNG_CELL_SIZE    2.0f
#define DNG_HALF_CELL    1.0f   /* DNG_CELL_SIZE / 2 */
#define DNG_RENDER_R     7      /* max grid cells rendered from player */
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
    int spawn_gx, spawn_gy;
    int stairs_gx, stairs_gy, stairs_dir;   /* up-stairs */
    bool has_up;
    int down_gx, down_gy, down_dir;         /* down-stairs (-1 if none) */
    bool has_down;
    /* Alien entities (for FPS view) */
    uint8_t aliens[DNG_GRID_H + 1][DNG_GRID_W + 1]; /* 0=none, 1-4=enemy type (ENEMY_LURKER+1 etc) */
    /* Room info for ship system */
    int room_count;
    int room_cx[12], room_cy[12];  /* room centers */
    int room_x[12], room_y[12], room_w[12], room_h[12]; /* room bounds */
    int room_ship_idx[12];         /* index into ship_room array (-1 = none) */
} sr_dungeon;

/* ── Simple RNG for dungeon generation ───────────────────────────── */

static uint32_t dng_rng_state = 0;

static void dng_rng_seed(uint32_t seed) { dng_rng_state = seed; }

static int dng_rng_int(int max) {
    dng_rng_state = dng_rng_state * 1103515245u + 12345u;
    return (int)(((dng_rng_state >> 16) & 0x7FFF) % (uint32_t)max);
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

static void dng_generate(sr_dungeon *d, int w, int h, bool has_down_stairs, bool has_up_stairs, uint32_t seed, int floor_num) {
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

    dng_room rooms[12];
    int num_rooms = 4 + dng_rng_int(4); /* 4-7 rooms */
    if (num_rooms > 12) num_rooms = 12;

    int mid_y = h / 2;          /* central corridor Y */
    int corridor_y1 = mid_y - 1;
    int corridor_y2 = mid_y + 1;

    /* Ship hull boundaries (leave margin) */
    int ship_left = 3;
    int ship_right = w - 2;
    int ship_span = ship_right - ship_left;

    /* Carve the central corridor */
    for (int x = ship_left; x <= ship_right; x++) {
        for (int y = corridor_y1; y <= corridor_y2; y++) {
            if (y >= 1 && y <= h && x >= 1 && x <= w)
                d->map[y][x] = 0;
        }
    }

    /* Place rooms along the corridor, evenly spaced */
    int spacing = ship_span / (num_rooms + 1);
    if (spacing < 4) spacing = 4;

    for (int i = 0; i < num_rooms; i++) {
        int rw = 3 + dng_rng_int(2);  /* 3-4 wide */
        int rh = 3 + dng_rng_int(2);  /* 3-4 tall */

        /* X position: evenly distributed left-to-right */
        int rx = ship_left + spacing * (i + 1) - rw / 2;
        if (rx < 2) rx = 2;
        if (rx + rw > w) rx = w - rw;

        /* Alternate rooms above/below corridor */
        int ry;
        if (i % 2 == 0) {
            /* Port side (above corridor) */
            ry = corridor_y1 - rh;
            if (ry < 2) ry = 2;
        } else {
            /* Starboard side (below corridor) */
            ry = corridor_y2 + 1;
            if (ry + rh > h) ry = h - rh;
        }

        rooms[i] = (dng_room){ rx, ry, rw, rh, rx + rw/2, ry + rh/2 };

        /* Carve room */
        for (int py = ry; py < ry + rh; py++)
            for (int px = rx; px < rx + rw; px++)
                if (py >= 1 && py <= h && px >= 1 && px <= w)
                    d->map[py][px] = 0;

        /* Connect room to central corridor with a vertical passage */
        int conn_x = rx + rw / 2;
        if (i % 2 == 0) {
            /* Room is above: carve down to corridor */
            for (int y = ry + rh; y <= corridor_y1; y++)
                if (y >= 1 && y <= h && conn_x >= 1 && conn_x <= w)
                    d->map[y][conn_x] = 0;
        } else {
            /* Room is below: carve up to corridor */
            for (int y = corridor_y2; y < ry; y++)
                if (y >= 1 && y <= h && conn_x >= 1 && conn_x <= w)
                    d->map[y][conn_x] = 0;
        }
    }

    /* Spawn at leftmost corridor (airlock entry) */
    d->spawn_gx = ship_left;
    d->spawn_gy = mid_y;

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
    }

    /* Place alien entities (not spawn, not stairs) */
    for (int i = 0; i < num_rooms; i++) {
        int aliens_in_room = 1 + dng_rng_int(2);
        for (int a = 0; a < aliens_in_room; a++) {
            int ax = rooms[i].x + dng_rng_int(rooms[i].w);
            int ay = rooms[i].y + dng_rng_int(rooms[i].h);
            if (ax < 1 || ax > w || ay < 1 || ay > h) continue;
            if (d->map[ay][ax] != 0) continue;
            if (ax == d->spawn_gx && ay == d->spawn_gy) continue;
            if (d->has_up && ax == d->stairs_gx && ay == d->stairs_gy) continue;
            if (d->has_down && ax == d->down_gx && ay == d->down_gy) continue;
            int max_type = (floor_num <= 1) ? 2 : (floor_num <= 3) ? 3 : 4;
            d->aliens[ay][ax] = 1 + (uint8_t)dng_rng_int(max_type);
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
} dng_player;

static void dng_player_init(dng_player *p, int gx, int gy, int dir) {
    p->gx = gx; p->gy = gy; p->dir = dir;
    p->x = (gx - 0.5f) * DNG_CELL_SIZE;
    p->z = (gy - 0.5f) * DNG_CELL_SIZE;
    p->y = 0;
    p->target_x = p->x;
    p->target_z = p->z;
    p->angle = dir * 0.25f;
    p->target_angle = p->angle;
}

static void dng_player_try_move(dng_player *p, const sr_dungeon *d, int dir) {
    int nx = p->gx + dng_dir_dx[dir];
    int ny = p->gy + dng_dir_dz[dir];
    if (dng_can_enter(d, p->gx, p->gy, nx, ny)) {
        p->gx = nx;
        p->gy = ny;
        p->target_x = (p->gx - 0.5f) * DNG_CELL_SIZE;
        p->target_z = (p->gy - 0.5f) * DNG_CELL_SIZE;
    }
}

static void dng_player_update(dng_player *p) {
    p->x += (p->target_x - p->x) * DNG_MOVE_SMOOTH;
    p->z += (p->target_z - p->z) * DNG_MOVE_SMOOTH;
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
            if (ddx > DNG_RENDER_R || ddz > DNG_RENDER_R) continue;

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
} dng_game;

static void dng_game_init(dng_game *g) {
    memset(g, 0, sizeof(*g));
    g->seed_base = 42;
    g->current_floor = 0;

    /* Generate floor 0 */
    dng_generate(&g->floors[0], DNG_GRID_W, DNG_GRID_H, false, true,
                 g->seed_base, 0);
    g->floor_generated[0] = true;
    g->dungeon = &g->floors[0];

    dng_player_init(&g->player, g->dungeon->spawn_gx, g->dungeon->spawn_gy, 2);
}

static void dng_go_up(dng_game *g) {
    g->current_floor++;
    if (g->current_floor >= DNG_MAX_FLOORS) { g->current_floor--; return; }

    if (!g->floor_generated[g->current_floor]) {
        bool is_last = (g->current_floor >= DNG_MAX_FLOORS - 1);
        dng_generate(&g->floors[g->current_floor], DNG_GRID_W, DNG_GRID_H,
                     true, !is_last, g->seed_base + (uint32_t)g->current_floor * 777,
                     g->current_floor);
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

#endif /* SR_DUNGEON_H */
