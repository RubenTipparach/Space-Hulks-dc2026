/*  sr_scene_dungeon.h — Dungeon Crawler scene (rendering, config, game state).
 *  Single-TU header-only. Depends on sr_dungeon.h, sr_config.h, sr_lighting.h, sr_app.h. */
#ifndef SR_SCENE_DUNGEON_H
#define SR_SCENE_DUNGEON_H

#include "sr_config.h"

/* ── Runtime dungeon config (loaded from config/dungeon.yaml) ────── */

static struct {
    float light_color[3];
    float light_brightness;
    float light_min_range;
    float light_attn_dist;
    float ambient_color[3];
    float ambient_brightness;
    int   fog_levels;
    float fog_start[16];
    float fog_intensity[16];
    float fog_density[16];
    float fog_stop;
    float room_light_color[3];
    float room_light_brightness;
    float room_light_radius;
} dng_cfg;

static void dng_load_config(void) {
    sr_config cfg = sr_config_load("config/dungeon.yaml");

    float color[3] = {1,1,1};
    sr_config_array(&cfg, "torch.color", color, 3);
    dng_cfg.light_color[0] = color[0];
    dng_cfg.light_color[1] = color[1];
    dng_cfg.light_color[2] = color[2];
    dng_cfg.light_brightness = sr_config_float(&cfg, "torch.brightness", 1.0f);
    dng_cfg.light_min_range  = sr_config_float(&cfg, "torch.min_range", 1.0f);
    dng_cfg.light_attn_dist  = sr_config_float(&cfg, "torch.attn_dist", 6.0f);

    float amb[3] = {0.15f, 0.12f, 0.18f};
    sr_config_array(&cfg, "ambient.color", amb, 3);
    dng_cfg.ambient_color[0] = amb[0];
    dng_cfg.ambient_color[1] = amb[1];
    dng_cfg.ambient_color[2] = amb[2];
    dng_cfg.ambient_brightness = sr_config_float(&cfg, "ambient.brightness", 0.1f);

    dng_cfg.fog_levels = (int)sr_config_float(&cfg, "fog.levels", 5.0f);
    if (dng_cfg.fog_levels > 16) dng_cfg.fog_levels = 16;
    sr_config_array(&cfg, "fog.start", dng_cfg.fog_start, dng_cfg.fog_levels);
    sr_config_array(&cfg, "fog.intensity", dng_cfg.fog_intensity, dng_cfg.fog_levels);
    sr_config_array(&cfg, "fog.density", dng_cfg.fog_density, dng_cfg.fog_levels);
    dng_cfg.fog_stop = sr_config_float(&cfg, "fog.stop", 8.0f);

    float rl_color[3] = {0.8f, 0.9f, 1.0f};
    sr_config_array(&cfg, "room_light.color", rl_color, 3);
    dng_cfg.room_light_color[0] = rl_color[0];
    dng_cfg.room_light_color[1] = rl_color[1];
    dng_cfg.room_light_color[2] = rl_color[2];
    dng_cfg.room_light_brightness = sr_config_float(&cfg, "room_light.brightness", 1.2f);
    dng_cfg.room_light_radius     = sr_config_float(&cfg, "room_light.radius", 5.0f);

    dng_render_radius = (int)sr_config_float(&cfg, "draw_distance", 10.0f);
    if (dng_render_radius > DNG_RENDER_R) dng_render_radius = DNG_RENDER_R;
    if (dng_render_radius < 1) dng_render_radius = 1;

    sr_config_free(&cfg);
    printf("[dungeon] Config loaded: torch(%.1f/%.1f/%.1f) ambient(%.2f) fog(%d levels) room_light(%.1f/%.1f) draw_dist(%d)\n",
           dng_cfg.light_brightness, dng_cfg.light_min_range, dng_cfg.light_attn_dist,
           dng_cfg.ambient_brightness, dng_cfg.fog_levels,
           dng_cfg.room_light_brightness, dng_cfg.room_light_radius, dng_render_radius);
}

/* ── Game state ──────────────────────────────────────────────────── */

static dng_game dng_state;
static bool dng_initialized = false;

enum {
    DNG_STATE_PLAYING,
    DNG_STATE_CLIMBING,
};
static int dng_play_state = DNG_STATE_PLAYING;

static int dng_light_mode = 0;
static bool dng_show_info = false;
static bool dng_expanded_map = false;
static bool dng_sprites_unlit = false;  /* true = sprites skip fog tint */
static int  dng_wall_texture = -1;      /* -1 = default ITEX_BRICK, else override */
static int  dng_room_wall_texture = -1; /* -1 = same as wall_texture; wall faces facing room cells */
static int  dng_floor_texture = -1;    /* -1 = default ITEX_TILE, else override */
static int  dng_ceiling_texture = -1;  /* -1 = default ITEX_WOOD, else override */
static bool dng_skip_pillars = false;  /* true = don't draw corner pillars */
static bool dng_skip_exterior = false; /* true = don't draw exterior hull */
static float dng_hull_padding = 0.25f; /* hull expansion padding (in cells) */
static float dng_hull_corner = 0.0f;  /* chamfer corner size (in cells, 0 = no chamfer) */
static float (*dng_fog_fn)(float, float, float) = NULL; /* override for fog vertex intensity */

/* ── Hull mask for exterior rendering ──────────────────────────── */

static bool dng_hull_inside[DNG_GRID_H + 2][DNG_GRID_W + 2];
static bool dng_hull_computed = false;
static const sr_dungeon *dng_hull_for = NULL; /* which dungeon the hull was computed for */

static void dng_compute_hull_mask(sr_dungeon *d) {
    int w = d->w, h = d->h;
    memset(dng_hull_inside, 0, sizeof(dng_hull_inside));

    /* BFS flood fill from spawn and room centers */
    static int queue_y[DNG_GRID_W * DNG_GRID_H];
    static int queue_x[DNG_GRID_W * DNG_GRID_H];
    int qhead = 0, qtail = 0;

    /* Seed: spawn point */
    if (d->spawn_gx >= 1 && d->spawn_gx <= w && d->spawn_gy >= 1 && d->spawn_gy <= h
        && d->map[d->spawn_gy][d->spawn_gx] == 0) {
        dng_hull_inside[d->spawn_gy][d->spawn_gx] = true;
        queue_y[qtail] = d->spawn_gy; queue_x[qtail] = d->spawn_gx; qtail++;
    }
    /* Seed: room centers */
    for (int r = 0; r < d->room_count; r++) {
        int cx = d->room_cx[r], cy = d->room_cy[r];
        if (cx >= 1 && cx <= w && cy >= 1 && cy <= h
            && !dng_hull_inside[cy][cx] && d->map[cy][cx] == 0) {
            dng_hull_inside[cy][cx] = true;
            queue_y[qtail] = cy; queue_x[qtail] = cx; qtail++;
        }
    }

    static const int bfs_dx[4] = {0, 0, -1, 1};
    static const int bfs_dy[4] = {-1, 1, 0, 0};
    while (qhead < qtail) {
        int cy = queue_y[qhead], cx = queue_x[qhead]; qhead++;
        for (int dir = 0; dir < 4; dir++) {
            int ny = cy + bfs_dy[dir], nx = cx + bfs_dx[dir];
            if (ny < 1 || ny > h || nx < 1 || nx > w) continue;
            if (dng_hull_inside[ny][nx]) continue;
            if (d->map[ny][nx] != 0) continue;
            dng_hull_inside[ny][nx] = true;
            queue_y[qtail] = ny; queue_x[qtail] = nx; qtail++;
        }
    }

    /* Expand 1 layer: include wall cells adjacent to interior (cardinal only).
     * Window faces block expansion in their direction. */
    static bool to_expand[DNG_GRID_H + 2][DNG_GRID_W + 2];
    memset(to_expand, 0, sizeof(to_expand));
    for (int gy = 1; gy <= h; gy++) {
        for (int gx = 1; gx <= w; gx++) {
            if (d->map[gy][gx] != 1 || dng_hull_inside[gy][gx]) continue;
            bool reachable = false;
            if (gy > 1 && dng_hull_inside[gy-1][gx] && !(d->win_faces[gy][gx] & DNG_WIN_N))
                reachable = true;
            if (gy < h && dng_hull_inside[gy+1][gx] && !(d->win_faces[gy][gx] & DNG_WIN_S))
                reachable = true;
            if (gx > 1 && dng_hull_inside[gy][gx-1] && !(d->win_faces[gy][gx] & DNG_WIN_W))
                reachable = true;
            if (gx < w && dng_hull_inside[gy][gx+1] && !(d->win_faces[gy][gx] & DNG_WIN_E))
                reachable = true;
            if (reachable) to_expand[gy][gx] = true;
        }
    }
    for (int gy = 1; gy <= h; gy++)
        for (int gx = 1; gx <= w; gx++)
            if (to_expand[gy][gx]) dng_hull_inside[gy][gx] = true;

    /* Diagonal corner fill */
    memset(to_expand, 0, sizeof(to_expand));
    for (int gy = 1; gy <= h; gy++) {
        for (int gx = 1; gx <= w; gx++) {
            if (d->map[gy][gx] != 1 || dng_hull_inside[gy][gx]) continue;
            if (d->win_faces[gy][gx]) continue; /* skip cells with any window */
            bool add = false;
            if (gy > 1 && gx > 1 && dng_hull_inside[gy-1][gx] && dng_hull_inside[gy][gx-1]) add = true;
            if (gy > 1 && gx < w && dng_hull_inside[gy-1][gx] && dng_hull_inside[gy][gx+1]) add = true;
            if (gy < h && gx > 1 && dng_hull_inside[gy+1][gx] && dng_hull_inside[gy][gx-1]) add = true;
            if (gy < h && gx < w && dng_hull_inside[gy+1][gx] && dng_hull_inside[gy][gx+1]) add = true;
            if (add) to_expand[gy][gx] = true;
        }
    }
    for (int gy = 1; gy <= h; gy++)
        for (int gx = 1; gx <= w; gx++)
            if (to_expand[gy][gx]) dng_hull_inside[gy][gx] = true;

    dng_hull_computed = true;
    dng_hull_for = d;
}

static bool dng_is_win_cell(const sr_dungeon *d, int gx, int gy) {
    if (gx < 1 || gx > d->w || gy < 1 || gy > d->h) return false;
    return d->win_faces[gy][gx] != 0;
}

static bool dng_has_win_face(const sr_dungeon *d, int gx, int gy, uint8_t dir_bit) {
    if (gx < 1 || gx > d->w || gy < 1 || gy > d->h) return false;
    return (d->win_faces[gy][gx] & dir_bit) != 0;
}

/* ── Per-cell room light (set before drawing each cell) ─────────── */

static int   dng_cur_room_light = -1;  /* room index for current cell, -1 = none */
static float dng_cur_rl_x, dng_cur_rl_y, dng_cur_rl_z; /* precomputed light world pos */

static void dng_set_cell_room_light(int gx, int gy) {
    sr_dungeon *dd = dng_state.dungeon;
    int ri = dng_room_at(dd, gx, gy);
    if (ri >= 0 && dd->room_light_on[ri]) {
        dng_cur_room_light = ri;
        dng_cur_rl_x = (dd->room_cx[ri] - 0.5f) * DNG_CELL_SIZE;
        dng_cur_rl_y = DNG_HALF_CELL;
        dng_cur_rl_z = (dd->room_cy[ri] - 0.5f) * DNG_CELL_SIZE;
    } else {
        dng_cur_room_light = -1;
    }
}

/* ── Torch lighting (pixel-lit callback) ─────────────────────────── */

static float dng_torch_light(float wx, float wy, float wz,
                              float nx, float ny, float nz)
{
    dng_player *p = &dng_state.player;

    float ambient = dng_cfg.ambient_brightness *
        (dng_cfg.ambient_color[0] + dng_cfg.ambient_color[1] + dng_cfg.ambient_color[2]) / 3.0f;

    float total = ambient;

    /* Player torch */
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (dist < dng_cfg.light_attn_dist) {
        float atten;
        if (dist <= dng_cfg.light_min_range) {
            atten = 1.0f;
        } else {
            atten = 1.0f - (dist - dng_cfg.light_min_range) /
                            (dng_cfg.light_attn_dist - dng_cfg.light_min_range);
            atten *= atten;
        }

        float inv_dist = 1.0f / (dist + 0.001f);
        float ndotl = (dx * nx + dy * ny + dz * nz) * inv_dist;
        if (ndotl < 0.0f) ndotl = 0.0f;

        float torch_lum = dng_cfg.light_brightness *
            (dng_cfg.light_color[0] + dng_cfg.light_color[1] + dng_cfg.light_color[2]) / 3.0f;

        total += torch_lum * atten * ndotl;
    }

    /* Room ceiling light (one per cell, set before draw) */
    if (dng_cur_room_light >= 0) {
        float rl_radius = dng_cfg.room_light_radius;
        float ldx = dng_cur_rl_x - wx;
        float ldy = dng_cur_rl_y - wy;
        float ldz = dng_cur_rl_z - wz;
        float ldist = sqrtf(ldx*ldx + ldy*ldy + ldz*ldz);

        if (ldist < rl_radius) {
            float la;
            if (ldist <= 0.5f) {
                la = 1.0f;
            } else {
                la = 1.0f - (ldist - 0.5f) / (rl_radius - 0.5f);
                la *= la;
            }

            float linv = 1.0f / (ldist + 0.001f);
            float lndotl = (ldx * nx + ldy * ny + ldz * nz) * linv;
            if (lndotl < 0.0f) lndotl = 0.0f;

            float rl_lum = dng_cfg.room_light_brightness *
                (dng_cfg.room_light_color[0] + dng_cfg.room_light_color[1] + dng_cfg.room_light_color[2]) / 3.0f;

            total += rl_lum * la * lndotl;
        }
    }

    return total;
}

/* ── Depth-fog intensity ─────────────────────────────────────────── */

static float dng_fog_intensity_at_dist(float dist_cells) {
    if (dist_cells < dng_cfg.fog_start[0]) return dng_cfg.fog_intensity[0];

    for (int i = 0; i < dng_cfg.fog_levels - 1; i++) {
        if (dist_cells < dng_cfg.fog_start[i + 1]) {
            float t = (dist_cells - dng_cfg.fog_start[i]) /
                      (dng_cfg.fog_start[i + 1] - dng_cfg.fog_start[i]);
            return dng_cfg.fog_intensity[i] + t * (dng_cfg.fog_intensity[i + 1] - dng_cfg.fog_intensity[i]);
        }
    }
    return dng_cfg.fog_intensity[dng_cfg.fog_levels - 1];
}

static float dng_fog_vertex_intensity(float wx, float wy, float wz) {
    dng_player *p = &dng_state.player;
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz) / DNG_CELL_SIZE;
    float base = dng_fog_intensity_at_dist(dist);

    /* Room ceiling light (one per cell, set before draw) */
    if (dng_cur_room_light >= 0) {
        float rl_radius = dng_cfg.room_light_radius;
        float ldx = dng_cur_rl_x - wx;
        float ldy = dng_cur_rl_y - wy;
        float ldz = dng_cur_rl_z - wz;
        float ldist = sqrtf(ldx*ldx + ldy*ldy + ldz*ldz);

        if (ldist < rl_radius) {
            float la;
            if (ldist <= 0.5f) {
                la = 1.0f;
            } else {
                la = 1.0f - (ldist - 0.5f) / (rl_radius - 0.5f);
                la *= la;
            }

            float rl_lum = dng_cfg.room_light_brightness *
                (dng_cfg.room_light_color[0] + dng_cfg.room_light_color[1] + dng_cfg.room_light_color[2]) / 3.0f;
            base += rl_lum * la;
        }
    }

    if (base > 1.0f) base = 1.0f;
    return base;
}

static inline float dng_get_fog_intensity(float wx, float wy, float wz) {
    return dng_fog_fn ? dng_fog_fn(wx, wy, wz) : dng_fog_vertex_intensity(wx, wy, wz);
}

/* ── Wall / floor drawing ────────────────────────────────────────── */

static void dng_draw_wall(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                           float ax, float ay, float az,
                           float bx, float by, float bz,
                           float cx, float cy, float cz,
                           float dx, float dy, float dz,
                           const sr_indexed_texture *tex,
                           float nx, float ny, float nz) {
    float edge_x = bx - ax, edge_y = by - ay, edge_z = bz - az;
    float width = sqrtf(edge_x*edge_x + edge_y*edge_y + edge_z*edge_z);
    float vert_x = dx - ax, vert_y = dy - ay, vert_z = dz - az;
    float height = sqrtf(vert_x*vert_x + vert_y*vert_y + vert_z*vert_z);
    float u_scale = width / DNG_CELL_SIZE;
    float v_scale = height / DNG_CELL_SIZE;

    if (dng_light_mode == 0) {
        sr_draw_quad_indexed_pixellit(fb_ptr,
            sr_vert_world(ax,ay,az, 0,0, 0xFFFFFFFF, ax,ay,az, nx,ny,nz),
            sr_vert_world(bx,by,bz, u_scale,0, 0xFFFFFFFF, bx,by,bz, nx,ny,nz),
            sr_vert_world(cx,cy,cz, u_scale,v_scale, 0xFFFFFFFF, cx,cy,cz, nx,ny,nz),
            sr_vert_world(dx,dy,dz, 0,v_scale, 0xFFFFFFFF, dx,dy,dz, nx,ny,nz),
            tex, mvp);
    } else {
        uint32_t ca = pal_intensity_color(dng_get_fog_intensity(ax,ay,az));
        uint32_t cb = pal_intensity_color(dng_get_fog_intensity(bx,by,bz));
        uint32_t cc = pal_intensity_color(dng_get_fog_intensity(cx,cy,cz));
        uint32_t cd = pal_intensity_color(dng_get_fog_intensity(dx,dy,dz));
        sr_draw_quad_indexed(fb_ptr,
            sr_vert_c(ax,ay,az, 0,0, ca),
            sr_vert_c(bx,by,bz, u_scale,0, cb),
            sr_vert_c(cx,cy,cz, u_scale,v_scale, cc),
            sr_vert_c(dx,dy,dz, 0,v_scale, cd),
            tex, mvp);
    }
}

static void dng_draw_wall_ds(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                              float ax, float ay, float az,
                              float bx, float by, float bz,
                              float cx, float cy, float cz,
                              float dx, float dy, float dz,
                              const sr_indexed_texture *tex,
                              float nx, float ny, float nz) {
    dng_draw_wall(fb_ptr, mvp, ax,ay,az, bx,by,bz, cx,cy,cz, dx,dy,dz, tex, nx,ny,nz);
    dng_draw_wall(fb_ptr, mvp, bx,by,bz, ax,ay,az, dx,dy,dz, cx,cy,cz, tex, -nx,-ny,-nz);
}

static void dng_draw_hquad(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                            float ax, float ay, float az,
                            float bx, float by, float bz,
                            float cx, float cy, float cz,
                            float dx, float dy, float dz,
                            float u0, float v0, float u1, float v1,
                            const sr_indexed_texture *tex,
                            float nx, float ny, float nz) {
    if (dng_light_mode == 0) {
        sr_draw_quad_indexed_pixellit(fb_ptr,
            sr_vert_world(ax,ay,az, u0,v0, 0xFFFFFFFF, ax,ay,az, nx,ny,nz),
            sr_vert_world(bx,by,bz, u1,v0, 0xFFFFFFFF, bx,by,bz, nx,ny,nz),
            sr_vert_world(cx,cy,cz, u1,v1, 0xFFFFFFFF, cx,cy,cz, nx,ny,nz),
            sr_vert_world(dx,dy,dz, u0,v1, 0xFFFFFFFF, dx,dy,dz, nx,ny,nz),
            tex, mvp);
    } else {
        uint32_t ca = pal_intensity_color(dng_get_fog_intensity(ax,ay,az));
        uint32_t cb = pal_intensity_color(dng_get_fog_intensity(bx,by,bz));
        uint32_t cc = pal_intensity_color(dng_get_fog_intensity(cx,cy,cz));
        uint32_t cd = pal_intensity_color(dng_get_fog_intensity(dx,dy,dz));
        sr_draw_quad_indexed(fb_ptr,
            sr_vert_c(ax,ay,az, u0,v0, ca),
            sr_vert_c(bx,by,bz, u1,v0, cb),
            sr_vert_c(cx,cy,cz, u1,v1, cc),
            sr_vert_c(dx,dy,dz, u0,v1, cd),
            tex, mvp);
    }
}

/* Triangle drawing for roof/bottom fan polygons */
static void dng_draw_tri(sr_framebuffer *fb_ptr, const sr_mat4 *mvp,
                          float ax, float ay, float az, float au, float av,
                          float bx, float by, float bz, float bu, float bv,
                          float cx, float cy, float cz, float cu, float cv,
                          const sr_indexed_texture *tex,
                          float nx, float ny, float nz) {
    uint32_t ca = pal_intensity_color(dng_get_fog_intensity(ax,ay,az));
    uint32_t cb = pal_intensity_color(dng_get_fog_intensity(bx,by,bz));
    uint32_t cc = pal_intensity_color(dng_get_fog_intensity(cx,cy,cz));
    sr_draw_triangle_indexed(fb_ptr,
        sr_vert_c(ax,ay,az, au,av, ca),
        sr_vert_c(bx,by,bz, bu,bv, cb),
        sr_vert_c(cx,cy,cz, cu,cv, cc),
        tex, mvp);
}

/* ── Draw the dungeon scene ──────────────────────────────────────── */

static void draw_dungeon_scene(sr_framebuffer *fb_ptr, const sr_mat4 *vp) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;

    if (dng_light_mode == 0)
        sr_set_pixel_light_fn(dng_torch_light);

    float cam_x = p->x;
    float cam_y = p->y;
    float cam_z = p->z;
    float cam_angle = p->angle * 6.28318f;

    float ca_cos = cosf(cam_angle), ca_sin = sinf(cam_angle);
    sr_vec3 eye = { cam_x, cam_y, cam_z };
    sr_vec3 fwd = { ca_sin, 0, -ca_cos };
    sr_vec3 target = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
    sr_vec3 up = { 0, 1, 0 };

    sr_mat4 view = sr_mat4_lookat(eye, target, up);
    sr_mat4 proj = sr_mat4_perspective(
        70.0f * 3.14159f / 180.0f,
        (float)FB_WIDTH / (float)FB_HEIGHT,
        0.05f, 40.0f
    );
    sr_mat4 mvp = sr_mat4_mul(proj, view);

    dng_build_visibility(p, d);

    float y_lo = -DNG_HALF_CELL;
    float y_hi = DNG_HALF_CELL;
    float P = DNG_PILLAR_PAD;

    int pgx = p->gx, pgy = p->gy;
    int gx0 = pgx - dng_render_radius; if (gx0 < 1) gx0 = 1;
    int gx1 = pgx + dng_render_radius; if (gx1 > d->w) gx1 = d->w;
    int gy0 = pgy - dng_render_radius; if (gy0 < 1) gy0 = 1;
    int gy1 = pgy + dng_render_radius; if (gy1 > d->h) gy1 = d->h;

    const sr_indexed_texture *wall_tex = (dng_wall_texture >= 0)
        ? &itextures[dng_wall_texture] : &itextures[ITEX_BRICK];
    const sr_indexed_texture *room_wall_tex = (dng_room_wall_texture >= 0)
        ? &itextures[dng_room_wall_texture] : wall_tex;
    const sr_indexed_texture *floor_tex = (dng_floor_texture >= 0)
        ? &itextures[dng_floor_texture] : &itextures[ITEX_TILE];
    const sr_indexed_texture *ceil_tex = (dng_ceiling_texture >= 0)
        ? &itextures[dng_ceiling_texture] : &itextures[ITEX_WOOD];
    float WP = dng_skip_pillars ? 0.0f : P; /* wall padding (0 = flush, no pillar gaps) */

    /* Render cells */
    for (int gy = gy0; gy <= gy1; gy++) {
        for (int gx = gx0; gx <= gx1; gx++) {
            if (!dng_vis[gy][gx]) continue;

            dng_set_cell_room_light(gx, gy);

            float x0 = (gx - 1) * DNG_CELL_SIZE;
            float x1 = gx * DNG_CELL_SIZE;
            float z0 = (gy - 1) * DNG_CELL_SIZE;
            float z1 = gy * DNG_CELL_SIZE;

            if (d->map[gy][gx] == 1) {
                /* Wall cell — draw faces toward open cells.
                 * Pick texture per-face: room faces get room_wall_tex, corridor faces get wall_tex.
                 * Window faces get a window texture (wall_A_window). */
                static const sr_indexed_texture *win_tex_ptr = NULL;
                if (!win_tex_ptr) win_tex_ptr = &itextures[ITEX_EXT_WINDOW];
                uint8_t wf = d->win_faces[gy][gx];
                if (gy < d->h && d->map[gy+1][gx] != 1 && dng_vis[gy+1][gx]) {
                    const sr_indexed_texture *ft = (wf & DNG_WIN_S) ? win_tex_ptr
                        : (dng_room_at(d, gx, gy+1) >= 0) ? room_wall_tex : wall_tex;
                    dng_draw_wall(fb_ptr, &mvp,
                        x0+WP, y_hi, z1,  x1-WP, y_hi, z1,
                        x1-WP, y_lo, z1,  x0+WP, y_lo, z1,
                        ft, 0, 0, 1);
                }
                if (gy > 1 && d->map[gy-1][gx] != 1 && dng_vis[gy-1][gx]) {
                    const sr_indexed_texture *ft = (wf & DNG_WIN_N) ? win_tex_ptr
                        : (dng_room_at(d, gx, gy-1) >= 0) ? room_wall_tex : wall_tex;
                    dng_draw_wall(fb_ptr, &mvp,
                        x1-WP, y_hi, z0,  x0+WP, y_hi, z0,
                        x0+WP, y_lo, z0,  x1-WP, y_lo, z0,
                        ft, 0, 0, -1);
                }
                if (gx < d->w && d->map[gy][gx+1] != 1 && dng_vis[gy][gx+1]) {
                    const sr_indexed_texture *ft = (wf & DNG_WIN_E) ? win_tex_ptr
                        : (dng_room_at(d, gx+1, gy) >= 0) ? room_wall_tex : wall_tex;
                    dng_draw_wall(fb_ptr, &mvp,
                        x1, y_hi, z1-WP,  x1, y_hi, z0+WP,
                        x1, y_lo, z0+WP,  x1, y_lo, z1-WP,
                        ft, 1, 0, 0);
                }
                if (gx > 1 && d->map[gy][gx-1] != 1 && dng_vis[gy][gx-1]) {
                    const sr_indexed_texture *ft = (wf & DNG_WIN_W) ? win_tex_ptr
                        : (dng_room_at(d, gx-1, gy) >= 0) ? room_wall_tex : wall_tex;
                    dng_draw_wall(fb_ptr, &mvp,
                        x0, y_hi, z0+WP,  x0, y_hi, z1-WP,
                        x0, y_lo, z1-WP,  x0, y_lo, z0+WP,
                        ft, -1, 0, 0);
                }
            } else {
                /* Open cell */
                bool is_up_stairs = (d->has_up && gx == d->stairs_gx && gy == d->stairs_gy);
                bool is_down_stairs = (d->has_down && gx == d->down_gx && gy == d->down_gy);

                if (is_up_stairs || is_down_stairs) {
                    int sdir = is_up_stairs ? d->stairs_dir : d->down_dir;
                    bool going_down = is_down_stairs;
                    float y_range = y_hi - y_lo;
                    float step_h = y_range / DNG_NUM_STEPS;
                    float step_d = DNG_CELL_SIZE / DNG_NUM_STEPS;

                    static const float riser_nx[4] = { 0, -1, 0,  1};
                    static const float riser_nz[4] = { 1,  0,-1,  0};

                    const sr_indexed_texture *stex = &itextures[ITEX_STONE];

                    for (int i = 0; i < DNG_NUM_STEPS; i++) {
                        float tread_y, riser_top, riser_bot;
                        float side_top, side_bot;
                        if (going_down) {
                            tread_y = y_lo - (i + 1) * step_h;
                            riser_top = y_lo - i * step_h;
                            riser_bot = tread_y;
                            side_top = y_lo;
                            side_bot = tread_y;
                        } else {
                            tread_y = y_lo + (i + 1) * step_h;
                            riser_top = tread_y;
                            riser_bot = y_lo + i * step_h;
                            side_top = tread_y;
                            side_bot = y_lo;
                        }

                        float sx0, sx1, sz0, sz1;
                        if (sdir == 0) { /* North */
                            sz0 = z1 - (i + 1) * step_d;
                            sz1 = z1 - i * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0,tread_y,sz0, x1,tread_y,sz0,
                                x1,tread_y,sz1, x0,tread_y,sz1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x0,riser_top,sz1, x1,riser_top,sz1,
                                x1,riser_bot,sz1, x0,riser_bot,sz1,
                                stex, riser_nx[0],0,riser_nz[0]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x0,side_top,sz0, x0,side_top,sz1,
                                    x0,side_bot,sz1, x0,side_bot,sz0,
                                    stex, -1,0,0);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x1,side_top,sz1, x1,side_top,sz0,
                                    x1,side_bot,sz0, x1,side_bot,sz1,
                                    stex, 1,0,0);
                            }
                        } else if (sdir == 1) { /* East */
                            sx0 = x0 + i * step_d;
                            sx1 = x0 + (i + 1) * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                sx0,tread_y,z0, sx1,tread_y,z0,
                                sx1,tread_y,z1, sx0,tread_y,z1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx0,riser_top,z0, sx0,riser_top,z1,
                                sx0,riser_bot,z1, sx0,riser_bot,z0,
                                stex, riser_nx[1],0,riser_nz[1]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx0,side_top,z0, sx1,side_top,z0,
                                    sx1,side_bot,z0, sx0,side_bot,z0,
                                    stex, 0,0,-1);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx1,side_top,z1, sx0,side_top,z1,
                                    sx0,side_bot,z1, sx1,side_bot,z1,
                                    stex, 0,0,1);
                            }
                        } else if (sdir == 2) { /* South */
                            sz0 = z0 + i * step_d;
                            sz1 = z0 + (i + 1) * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0,tread_y,sz0, x1,tread_y,sz0,
                                x1,tread_y,sz1, x0,tread_y,sz1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                x1,riser_top,sz0, x0,riser_top,sz0,
                                x0,riser_bot,sz0, x1,riser_bot,sz0,
                                stex, riser_nx[2],0,riser_nz[2]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x0,side_top,sz1, x0,side_top,sz0,
                                    x0,side_bot,sz0, x0,side_bot,sz1,
                                    stex, -1,0,0);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    x1,side_top,sz0, x1,side_top,sz1,
                                    x1,side_bot,sz1, x1,side_bot,sz0,
                                    stex, 1,0,0);
                            }
                        } else { /* West (3) */
                            sx0 = x1 - (i + 1) * step_d;
                            sx1 = x1 - i * step_d;
                            dng_draw_hquad(fb_ptr, &mvp,
                                sx0,tread_y,z0, sx1,tread_y,z0,
                                sx1,tread_y,z1, sx0,tread_y,z1,
                                0,0,1,1, stex, 0,1,0);
                            dng_draw_wall(fb_ptr, &mvp,
                                sx1,riser_top,z1, sx1,riser_top,z0,
                                sx1,riser_bot,z0, sx1,riser_bot,z1,
                                stex, riser_nx[3],0,riser_nz[3]);
                            if (!going_down) {
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx1,side_top,z0, sx0,side_top,z0,
                                    sx0,side_bot,z0, sx1,side_bot,z0,
                                    stex, 0,0,-1);
                                dng_draw_wall_ds(fb_ptr, &mvp,
                                    sx0,side_top,z1, sx1,side_top,z1,
                                    sx1,side_bot,z1, sx0,side_bot,z1,
                                    stex, 0,0,1);
                            }
                        }
                    }

                    if (is_down_stairs) {
                        dng_draw_hquad(fb_ptr, &mvp,
                            x0,y_hi,z1, x1,y_hi,z1,
                            x1,y_hi,z0, x0,y_hi,z0,
                            0,1,1,0, ceil_tex, 0,-1,0);
                    }
                } else {
                    /* Normal floor + ceiling */
                    dng_draw_hquad(fb_ptr, &mvp,
                        x0,y_lo,z0, x1,y_lo,z0,
                        x1,y_lo,z1, x0,y_lo,z1,
                        0,0,1,1, floor_tex, 0,1,0);

                    dng_draw_hquad(fb_ptr, &mvp,
                        x0,y_hi,z1, x1,y_hi,z1,
                        x1,y_hi,z0, x0,y_hi,z0,
                        0,1,1,0, ceil_tex, 0,-1,0);
                }
            }
        }
    }

    /* Pillars at grid intersections
     * dng_skip_pillars: use P=0 (flush walls, no protruding columns) */
    {
    float PP = dng_skip_pillars ? 0.0f : P;
    for (int vy = gy0; vy <= gy1 + 1; vy++) {
        for (int vx = gx0; vx <= gx1 + 1; vx++) {
            bool nw_open = vx > 1 && vy > 1 && d->map[vy-1][vx-1] != 1;
            bool ne_open = vx <= d->w && vy > 1 && d->map[vy-1][vx] != 1;
            bool sw_open = vx > 1 && vy <= d->h && d->map[vy][vx-1] != 1;
            bool se_open = vx <= d->w && vy <= d->h && d->map[vy][vx] != 1;

            bool has_open = nw_open || ne_open || sw_open || se_open;
            bool all_open = nw_open && ne_open && sw_open && se_open;

            if (has_open && !all_open) {
                bool visible = false;
                if (nw_open && vx-1 >= 1 && vy-1 >= 1 && dng_vis[vy-1][vx-1]) visible = true;
                if (ne_open && vx <= d->w && vy-1 >= 1 && dng_vis[vy-1][vx]) visible = true;
                if (sw_open && vx-1 >= 1 && vy <= d->h && dng_vis[vy][vx-1]) visible = true;
                if (se_open && vx <= d->w && vy <= d->h && dng_vis[vy][vx]) visible = true;
                if (!visible) continue;

                float wx = (vx - 1) * DNG_CELL_SIZE;
                float wz = (vy - 1) * DNG_CELL_SIZE;

                if (sw_open || se_open) {
                    float fx0 = sw_open ? wx - PP : wx;
                    float fx1 = se_open ? wx + PP : wx;
                    if (fx0 < fx1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            fx0, y_hi, wz+PP,  fx1, y_hi, wz+PP,
                            fx1, y_lo, wz+PP,  fx0, y_lo, wz+PP,
                            wall_tex, 0, 0, 1);
                    }
                }
                if (nw_open || ne_open) {
                    float fx0 = nw_open ? wx - PP : wx;
                    float fx1 = ne_open ? wx + PP : wx;
                    if (fx0 < fx1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            fx1, y_hi, wz-PP,  fx0, y_hi, wz-PP,
                            fx0, y_lo, wz-PP,  fx1, y_lo, wz-PP,
                            wall_tex, 0, 0, -1);
                    }
                }
                if (ne_open || se_open) {
                    float fz0 = ne_open ? wz - PP : wz;
                    float fz1 = se_open ? wz + PP : wz;
                    if (fz0 < fz1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            wx+PP, y_hi, fz1,  wx+PP, y_hi, fz0,
                            wx+PP, y_lo, fz0,  wx+PP, y_lo, fz1,
                            wall_tex, 1, 0, 0);
                    }
                }
                if (nw_open || sw_open) {
                    float fz0 = nw_open ? wz - PP : wz;
                    float fz1 = sw_open ? wz + PP : wz;
                    if (fz0 < fz1) {
                        dng_draw_wall(fb_ptr, &mvp,
                            wx-PP, y_hi, fz0,  wx-PP, y_hi, fz1,
                            wx-PP, y_lo, fz1,  wx-PP, y_lo, fz0,
                            wall_tex, -1, 0, 0);
                    }
                }
            }
        }
    }
    }

    /* ── Alien billboards (camera-facing textured quads) ──────── */
    {
        float cam_angle = p->angle * 6.28318f;
        float right_x = cosf(cam_angle);
        float right_z = sinf(cam_angle);
        float sprite_half = 0.5f;  /* half-size of billboard quad in world units */

        for (int bgy = gy0; bgy <= gy1; bgy++) {
            for (int bgx = gx0; bgx <= gx1; bgx++) {
                if (!dng_vis[bgy][bgx]) continue;
                uint8_t alien_type = d->aliens[bgy][bgx];
                if (alien_type == 0) continue;

                int type_idx = alien_type - 1;
                if (type_idx < 0 || type_idx >= STEX_COUNT) continue;
                const sr_texture *stex = &stextures[type_idx];
                if (!stex->pixels) continue;

                /* Use lerp position if available for smooth movement */
                float cx = (bgx - 0.5f) * DNG_CELL_SIZE;
                float cz = (bgy - 0.5f) * DNG_CELL_SIZE;
                for (int ei = 0; ei < dng_enemy_count; ei++) {
                    if (dng_enemies[ei].alive && dng_enemies[ei].gx == bgx && dng_enemies[ei].gy == bgy) {
                        cx = dng_enemies[ei].lerp_x;
                        cz = dng_enemies[ei].lerp_y;
                        break;
                    }
                }
                float bot_y = -DNG_HALF_CELL;
                float top_y = bot_y + sprite_half * 2.0f;

                /* Quad corners: left-bottom, right-bottom, right-top, left-top */
                float lx = cx - right_x * sprite_half;
                float lz = cz - right_z * sprite_half;
                float rx2 = cx + right_x * sprite_half;
                float rz = cz + right_z * sprite_half;

                /* Compute fog/light tint based on distance to player */
                uint32_t tint;
                if (dng_sprites_unlit) {
                    tint = 0xFFFFFFFF;
                } else {
                    float fog_i = dng_get_fog_intensity(cx, 0, cz);
                    tint = pal_intensity_color(fog_i);
                }

                sr_draw_quad_doublesided(fb_ptr,
                    sr_vert_c(lx, bot_y, lz, 0, 1, tint),
                    sr_vert_c(rx2, bot_y, rz, 1, 1, tint),
                    sr_vert_c(rx2, top_y, rz, 1, 0, tint),
                    sr_vert_c(lx, top_y, lz, 0, 0, tint),
                    stex, &mvp);
            }
        }
    }

    /* ── Console billboards (room subsystem consoles) ────────── */
    {
        float cam_angle2 = p->angle * 6.28318f;
        float cright_x = cosf(cam_angle2);
        float cright_z = sinf(cam_angle2);
        float console_half = 0.4f;  /* slightly smaller than alien sprites */

        for (int bgy = gy0; bgy <= gy1; bgy++) {
            for (int bgx = gx0; bgx <= gx1; bgx++) {
                if (!dng_vis[bgy][bgx]) continue;
                uint8_t con_type = d->consoles[bgy][bgx];
                if (con_type == 0 || con_type >= CONSOLE_TEX_COUNT) continue;
                const sr_texture *ctex = &console_textures[con_type];
                if (!ctex->pixels) continue;

                float ccx = (bgx - 0.5f) * DNG_CELL_SIZE;
                float ccz = (bgy - 0.5f) * DNG_CELL_SIZE;
                float cbot_y = -DNG_HALF_CELL;
                float ctop_y = cbot_y + console_half * 2.0f;

                float clx = ccx - cright_x * console_half;
                float clz = ccz - cright_z * console_half;
                float crx = ccx + cright_x * console_half;
                float crz = ccz + cright_z * console_half;

                uint32_t tint;
                if (dng_sprites_unlit) {
                    tint = 0xFFFFFFFF;
                } else {
                    float fog_i = dng_get_fog_intensity(ccx, 0, ccz);
                    tint = pal_intensity_color(fog_i);
                }

                sr_draw_quad_doublesided(fb_ptr,
                    sr_vert_c(clx, cbot_y, clz, 0, 1, tint),
                    sr_vert_c(crx, cbot_y, crz, 1, 1, tint),
                    sr_vert_c(crx, ctop_y, crz, 1, 0, tint),
                    sr_vert_c(clx, ctop_y, clz, 0, 0, tint),
                    ctex, &mvp);
            }
        }
    }

    /* ── Exterior hull walls (full port from C# BuildExteriorForFloor) ── */
    if (!dng_skip_exterior) {
        if (!dng_hull_computed || dng_hull_for != d) dng_compute_hull_mask(d);

        const sr_indexed_texture *ext_tex = &itextures[ITEX_EXT_WALL];
        const sr_indexed_texture *ext_win_tex = &itextures[ITEX_EXT_WINDOW];
        const sr_indexed_texture *roof_tex = &itextures[ITEX_ROOF];

        float ch = dng_hull_corner * DNG_CELL_SIZE; /* chamfer offset */
        float hp_ceil = ceilf(dng_hull_padding);
        float f = (hp_ceil - dng_hull_padding) * DNG_CELL_SIZE; /* fractional inset */

        /* Disable pixel lighting for exterior */
        sr_set_pixel_light_fn(NULL);

        /* ── Pass 1: Exterior walls (with chamfer + inset) ──────── */
        for (int gy = gy0; gy <= gy1; gy++) {
            for (int gx = gx0; gx <= gx1; gx++) {
                if (!dng_hull_inside[gy][gx]) continue;

                float x0 = (gx - 1) * DNG_CELL_SIZE;
                float x1 = gx * DNG_CELL_SIZE;
                float z0 = (gy - 1) * DNG_CELL_SIZE;
                float z1 = gy * DNG_CELL_SIZE;

                bool oN = gy <= 1 || !dng_hull_inside[gy-1][gx];
                bool oS = gy >= d->h || !dng_hull_inside[gy+1][gx];
                bool oW = gx <= 1 || !dng_hull_inside[gy][gx-1];
                bool oE = gx >= d->w || !dng_hull_inside[gy][gx+1];

                /* Window cell detection for flush faces */
                bool wN = oN && dng_is_win_cell(d, gx, gy-1);
                bool wS = oS && dng_is_win_cell(d, gx, gy+1);
                bool wW = oW && dng_is_win_cell(d, gx-1, gy);
                bool wE = oE && dng_is_win_cell(d, gx+1, gy);

                /* Convex corners (skip when diagonal is a window) */
                bool cNW = oN && oW && ch > 0 && !dng_is_win_cell(d, gx-1, gy-1);
                bool cNE = oN && oE && ch > 0 && !dng_is_win_cell(d, gx+1, gy-1);
                bool cSW = oS && oW && ch > 0 && !dng_is_win_cell(d, gx-1, gy+1);
                bool cSE = oS && oE && ch > 0 && !dng_is_win_cell(d, gx+1, gy+1);

                /* Per-face inset: 0 for window faces, also flush if diagonal neighbor is window */
                float fN = wN ? 0 : f, fS = wS ? 0 : f;
                float fW = wW ? 0 : f, fE = wE ? 0 : f;
                if (oN && fN > 0 && (dng_is_win_cell(d,gx-1,gy-1) || dng_is_win_cell(d,gx+1,gy-1))) fN = 0;
                if (oS && fS > 0 && (dng_is_win_cell(d,gx-1,gy+1) || dng_is_win_cell(d,gx+1,gy+1))) fS = 0;
                if (oW && fW > 0 && (dng_is_win_cell(d,gx-1,gy-1) || dng_is_win_cell(d,gx-1,gy+1))) fW = 0;
                if (oE && fE > 0 && (dng_is_win_cell(d,gx+1,gy-1) || dng_is_win_cell(d,gx+1,gy+1))) fE = 0;

                float ex0 = oW ? x0 + fW : x0;
                float ex1 = oE ? x1 - fE : x1;
                float ez0 = oN ? z0 + fN : z0;
                float ez1 = oS ? z1 - fS : z1;

                /* Per-face window texture: only when window points toward us */
                bool winTexN = oN && gy > 1 && dng_has_win_face(d, gx, gy-1, DNG_WIN_S);
                bool winTexS = oS && gy < d->h && dng_has_win_face(d, gx, gy+1, DNG_WIN_N);
                bool winTexW = oW && gx > 1 && dng_has_win_face(d, gx-1, gy, DNG_WIN_E);
                bool winTexE = oE && gx < d->w && dng_has_win_face(d, gx+1, gy, DNG_WIN_W);

                /* North wall — trimmed at chamfer corners */
                if (oN) {
                    const sr_indexed_texture *ft = winTexN ? ext_win_tex : ext_tex;
                    float nx0 = cNW ? ex0 + ch : ex0;
                    float nx1 = cNE ? ex1 - ch : ex1;
                    if (nx0 < nx1)
                        dng_draw_wall(fb_ptr, &mvp,
                            nx0, y_hi, ez0,  nx1, y_hi, ez0,
                            nx1, y_lo, ez0,  nx0, y_lo, ez0,
                            ft, 0, 0, -1);
                }
                /* South wall */
                if (oS) {
                    const sr_indexed_texture *ft = winTexS ? ext_win_tex : ext_tex;
                    float sx0 = cSW ? ex0 + ch : ex0;
                    float sx1 = cSE ? ex1 - ch : ex1;
                    if (sx0 < sx1)
                        dng_draw_wall(fb_ptr, &mvp,
                            sx1, y_hi, ez1,  sx0, y_hi, ez1,
                            sx0, y_lo, ez1,  sx1, y_lo, ez1,
                            ft, 0, 0, 1);
                }
                /* West wall */
                if (oW) {
                    const sr_indexed_texture *ft = winTexW ? ext_win_tex : ext_tex;
                    float wz0 = cNW ? ez0 + ch : ez0;
                    float wz1 = cSW ? ez1 - ch : ez1;
                    if (wz0 < wz1)
                        dng_draw_wall(fb_ptr, &mvp,
                            ex0, y_hi, wz1,  ex0, y_hi, wz0,
                            ex0, y_lo, wz0,  ex0, y_lo, wz1,
                            ft, -1, 0, 0);
                }
                /* East wall */
                if (oE) {
                    const sr_indexed_texture *ft = winTexE ? ext_win_tex : ext_tex;
                    float wz0e = cNE ? ez0 + ch : ez0;
                    float wz1e = cSE ? ez1 - ch : ez1;
                    if (wz0e < wz1e)
                        dng_draw_wall(fb_ptr, &mvp,
                            ex1, y_hi, wz0e,  ex1, y_hi, wz1e,
                            ex1, y_lo, wz1e,  ex1, y_lo, wz0e,
                            ft, 1, 0, 0);
                }

                /* Diagonal chamfer walls at convex corners */
                if (cNW)
                    dng_draw_wall(fb_ptr, &mvp,
                        ex0, y_hi, ez0+ch,  ex0+ch, y_hi, ez0,
                        ex0+ch, y_lo, ez0,  ex0, y_lo, ez0+ch,
                        ext_tex, -0.707f, 0, -0.707f);
                if (cNE)
                    dng_draw_wall(fb_ptr, &mvp,
                        ex1-ch, y_hi, ez0,  ex1, y_hi, ez0+ch,
                        ex1, y_lo, ez0+ch,  ex1-ch, y_lo, ez0,
                        ext_tex, 0.707f, 0, -0.707f);
                if (cSW)
                    dng_draw_wall(fb_ptr, &mvp,
                        ex0+ch, y_hi, ez1,  ex0, y_hi, ez1-ch,
                        ex0, y_lo, ez1-ch,  ex0+ch, y_lo, ez1,
                        ext_tex, -0.707f, 0, 0.707f);
                if (cSE)
                    dng_draw_wall(fb_ptr, &mvp,
                        ex1, y_hi, ez1-ch,  ex1-ch, y_hi, ez1,
                        ex1-ch, y_lo, ez1,  ex1, y_lo, ez1-ch,
                        ext_tex, 0.707f, 0, 0.707f);
            }
        }

        /* ── Pass 2: Concave corner fill walls ──────────────────── */
        {
            float ccFill = f > ch ? f : ch; /* max(f, c) */
            if (ccFill > 0) {
                for (int gy = gy0; gy <= gy1; gy++) {
                    for (int gx = gx0; gx <= gx1; gx++) {
                        if (!dng_hull_inside[gy][gx]) continue;

                        bool iN = gy > 1 && dng_hull_inside[gy-1][gx];
                        bool iS = gy < d->h && dng_hull_inside[gy+1][gx];
                        bool iW = gx > 1 && dng_hull_inside[gy][gx-1];
                        bool iE = gx < d->w && dng_hull_inside[gy][gx+1];

                        /* NE concave corner */
                        if (iN && iE && !(gy > 1 && gx < d->w && dng_hull_inside[gy-1][gx+1])
                            && !dng_is_win_cell(d, gx+1, gy-1)) {
                            float xb = gx * DNG_CELL_SIZE, zb = (gy-1) * DNG_CELL_SIZE;
                            dng_draw_wall(fb_ptr, &mvp,
                                xb-ccFill, y_hi, zb,  xb, y_hi, zb+ccFill,
                                xb, y_lo, zb+ccFill,  xb-ccFill, y_lo, zb,
                                ext_tex, -0.707f, 0, -0.707f);
                        }
                        /* NW concave corner */
                        if (iN && iW && !(gy > 1 && gx > 1 && dng_hull_inside[gy-1][gx-1])
                            && !dng_is_win_cell(d, gx-1, gy-1)) {
                            float xb = (gx-1) * DNG_CELL_SIZE, zb = (gy-1) * DNG_CELL_SIZE;
                            dng_draw_wall(fb_ptr, &mvp,
                                xb, y_hi, zb+ccFill,  xb+ccFill, y_hi, zb,
                                xb+ccFill, y_lo, zb,  xb, y_lo, zb+ccFill,
                                ext_tex, 0.707f, 0, -0.707f);
                        }
                        /* SE concave corner */
                        if (iS && iE && !(gy < d->h && gx < d->w && dng_hull_inside[gy+1][gx+1])
                            && !dng_is_win_cell(d, gx+1, gy+1)) {
                            float xb = gx * DNG_CELL_SIZE, zb = gy * DNG_CELL_SIZE;
                            dng_draw_wall(fb_ptr, &mvp,
                                xb, y_hi, zb-ccFill,  xb-ccFill, y_hi, zb,
                                xb-ccFill, y_lo, zb,  xb, y_lo, zb-ccFill,
                                ext_tex, 0.707f, 0, 0.707f);
                        }
                        /* SW concave corner */
                        if (iS && iW && !(gy < d->h && gx > 1 && dng_hull_inside[gy+1][gx-1])
                            && !dng_is_win_cell(d, gx-1, gy+1)) {
                            float xb = (gx-1) * DNG_CELL_SIZE, zb = gy * DNG_CELL_SIZE;
                            dng_draw_wall(fb_ptr, &mvp,
                                xb+ccFill, y_hi, zb,  xb, y_hi, zb-ccFill,
                                xb, y_lo, zb-ccFill,  xb+ccFill, y_lo, zb,
                                ext_tex, -0.707f, 0, 0.707f);
                        }
                    }
                }
            }
        }

        /* ── Pass 3: Window connector walls ─────────────────────── */
        /* Fill gaps between flush window faces and inset neighbors
         * (port of HullGeometry.ComputeWindowConnectors) */
        if (f > 0) {
            int w = d->w, h = d->h;
            for (int gy = 1; gy <= h; gy++) {
                for (int gx = 1; gx <= w; gx++) {
                    uint8_t wf = d->win_faces[gy][gx];
                    if (!wf) continue;

                    /* N/S facing windows */
                    if (wf & (DNG_WIN_N | DNG_WIN_S)) {
                        int dir_s = (wf & DNG_WIN_S) ? 1 : -1; /* +1=south, -1=north */
                        int iy = gy + dir_s; /* inside cell row */
                        if (iy < 1 || iy > h || !dng_hull_inside[iy][gx]) goto skip_ns;
                        {
                            float wz = (wf & DNG_WIN_S) ? gy * DNG_CELL_SIZE : (gy-1) * DNG_CELL_SIZE;
                            float iz = (wf & DNG_WIN_S) ? wz - f : wz + f;
                            float minz = wz < iz ? wz : iz, maxz = wz > iz ? wz : iz;
                            int oy = (wf & DNG_WIN_S) ? iy + 1 : iy - 1; /* outside row for neighbor check */
                            /* Left neighbor connector */
                            if (gx-1 >= 1 && dng_hull_inside[iy][gx-1]
                                && (oy < 1 || oy > h || !dng_hull_inside[oy][gx-1])
                                && !dng_is_win_cell(d, gx-1, gy)) {
                                float bx = (gx-1) * DNG_CELL_SIZE;
                                dng_draw_wall(fb_ptr, &mvp,
                                    bx, y_hi, minz,  bx, y_hi, maxz,
                                    bx, y_lo, maxz,  bx, y_lo, minz,
                                    ext_tex, -1, 0, 0);
                            }
                            /* Right neighbor connector */
                            if (gx+1 <= w && dng_hull_inside[iy][gx+1]
                                && (oy < 1 || oy > h || !dng_hull_inside[oy][gx+1])
                                && !dng_is_win_cell(d, gx+1, gy)) {
                                float bx = gx * DNG_CELL_SIZE;
                                dng_draw_wall(fb_ptr, &mvp,
                                    bx, y_hi, minz,  bx, y_hi, maxz,
                                    bx, y_lo, maxz,  bx, y_lo, minz,
                                    ext_tex, 1, 0, 0);
                            }
                        }
                        skip_ns:;
                    }

                    /* E/W facing windows */
                    if (wf & (DNG_WIN_E | DNG_WIN_W)) {
                        int dir_e = (wf & DNG_WIN_E) ? 1 : -1;
                        int ix = gx + dir_e;
                        if (ix < 1 || ix > w || !dng_hull_inside[gy][ix]) goto skip_ew;
                        {
                            float wx = (wf & DNG_WIN_E) ? gx * DNG_CELL_SIZE : (gx-1) * DNG_CELL_SIZE;
                            float ix2 = (wf & DNG_WIN_E) ? wx - f : wx + f;
                            float minx = wx < ix2 ? wx : ix2, maxx = wx > ix2 ? wx : ix2;
                            int ox = (wf & DNG_WIN_E) ? ix + 1 : ix - 1;
                            /* Top neighbor connector */
                            if (gy-1 >= 1 && dng_hull_inside[gy-1][ix]
                                && (ox < 1 || ox > w || !dng_hull_inside[gy-1][ox])
                                && !dng_is_win_cell(d, gx, gy-1)) {
                                float bz = (gy-1) * DNG_CELL_SIZE;
                                dng_draw_wall(fb_ptr, &mvp,
                                    minx, y_hi, bz,  maxx, y_hi, bz,
                                    maxx, y_lo, bz,  minx, y_lo, bz,
                                    ext_tex, 0, 0, -1);
                            }
                            /* Bottom neighbor connector */
                            if (gy+1 <= h && dng_hull_inside[gy+1][ix]
                                && (ox < 1 || ox > w || !dng_hull_inside[gy+1][ox])
                                && !dng_is_win_cell(d, gx, gy+1)) {
                                float bz = gy * DNG_CELL_SIZE;
                                dng_draw_wall(fb_ptr, &mvp,
                                    minx, y_hi, bz,  maxx, y_hi, bz,
                                    maxx, y_lo, bz,  minx, y_lo, bz,
                                    ext_tex, 0, 0, 1);
                            }
                        }
                        skip_ew:;
                    }
                }
            }
        }

        /* ── Pass 4: Roof + bottom hull plates (polygon fan) ────── */
        {
            int w = d->w, h = d->h;
            for (int gy = gy0; gy <= gy1; gy++) {
                for (int gx = gx0; gx <= gx1; gx++) {
                    if (!dng_hull_inside[gy][gx]) continue;

                    float x0 = (gx - 1) * DNG_CELL_SIZE;
                    float x1 = gx * DNG_CELL_SIZE;
                    float z0 = (gy - 1) * DNG_CELL_SIZE;
                    float z1 = gy * DNG_CELL_SIZE;

                    bool oN = gy <= 1 || !dng_hull_inside[gy-1][gx];
                    bool oS = gy >= h || !dng_hull_inside[gy+1][gx];
                    bool oW = gx <= 1 || !dng_hull_inside[gy][gx-1];
                    bool oE = gx >= w || !dng_hull_inside[gy][gx+1];

                    /* Simple case: no chamfer/inset */
                    if (ch <= 0 && f <= 0) {
                        if (d->map[gy][gx] == 1) {
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0, y_hi, z1,  x1, y_hi, z1,
                                x1, y_hi, z0,  x0, y_hi, z0,
                                0, 1, 1, 0, roof_tex, 0, 1, 0);
                            dng_draw_hquad(fb_ptr, &mvp,
                                x0, y_lo, z0,  x1, y_lo, z0,
                                x1, y_lo, z1,  x0, y_lo, z1,
                                0, 0, 1, 1, roof_tex, 0, -1, 0);
                        }
                        continue;
                    }

                    /* Window-aware per-face inset for roof */
                    bool rwN = oN && dng_is_win_cell(d, gx, gy-1);
                    bool rwS = oS && dng_is_win_cell(d, gx, gy+1);
                    bool rwW = oW && dng_is_win_cell(d, gx-1, gy);
                    bool rwE = oE && dng_is_win_cell(d, gx+1, gy);
                    float rfN = rwN ? 0 : f, rfS = rwS ? 0 : f;
                    float rfW = rwW ? 0 : f, rfE = rwE ? 0 : f;
                    if (oN && rfN > 0 && (dng_is_win_cell(d,gx-1,gy-1) || dng_is_win_cell(d,gx+1,gy-1))) rfN = 0;
                    if (oS && rfS > 0 && (dng_is_win_cell(d,gx-1,gy+1) || dng_is_win_cell(d,gx+1,gy+1))) rfS = 0;
                    if (oW && rfW > 0 && (dng_is_win_cell(d,gx-1,gy-1) || dng_is_win_cell(d,gx-1,gy+1))) rfW = 0;
                    if (oE && rfE > 0 && (dng_is_win_cell(d,gx+1,gy-1) || dng_is_win_cell(d,gx+1,gy+1))) rfE = 0;
                    float rx0 = oW ? x0 + rfW : x0;
                    float rx1 = oE ? x1 - rfE : x1;
                    float rz0 = oN ? z0 + rfN : z0;
                    float rz1 = oS ? z1 - rfS : z1;

                    /* Convex corners */
                    bool cvNW = oN && oW && ch > 0 && !dng_is_win_cell(d, gx-1, gy-1);
                    bool cvNE = oN && oE && ch > 0 && !dng_is_win_cell(d, gx+1, gy-1);
                    bool cvSW = oS && oW && ch > 0 && !dng_is_win_cell(d, gx-1, gy+1);
                    bool cvSE = oS && oE && ch > 0 && !dng_is_win_cell(d, gx+1, gy+1);

                    /* Concave corners */
                    float cc = f > ch ? f : ch;
                    bool ccNW = cc > 0 && !oN && !oW && !(gy > 1 && gx > 1 && dng_hull_inside[gy-1][gx-1]) && !dng_is_win_cell(d, gx-1, gy-1);
                    bool ccNE = cc > 0 && !oN && !oE && !(gy > 1 && gx < w && dng_hull_inside[gy-1][gx+1]) && !dng_is_win_cell(d, gx+1, gy-1);
                    bool ccSE = cc > 0 && !oS && !oE && !(gy < h && gx < w && dng_hull_inside[gy+1][gx+1]) && !dng_is_win_cell(d, gx+1, gy+1);
                    bool ccSW = cc > 0 && !oS && !oW && !(gy < h && gx > 1 && dng_hull_inside[gy+1][gx-1]) && !dng_is_win_cell(d, gx-1, gy+1);

                    /* Build polygon vertices CW from top-left (max 12 verts) */
                    float pts_x[12], pts_z[12];
                    int npts = 0;

                    /* NW corner */
                    if (cvNW)       { pts_x[npts]=rx0;     pts_z[npts]=rz0+ch; npts++;
                                      pts_x[npts]=rx0+ch;  pts_z[npts]=rz0;    npts++; }
                    else if (ccNW)  { pts_x[npts]=x0;      pts_z[npts]=z0+cc;  npts++;
                                      pts_x[npts]=x0+cc;   pts_z[npts]=z0;     npts++; }
                    else            { pts_x[npts]=rx0;      pts_z[npts]=rz0;    npts++; }

                    /* NE corner */
                    if (cvNE)       { pts_x[npts]=rx1-ch;  pts_z[npts]=rz0;    npts++;
                                      pts_x[npts]=rx1;     pts_z[npts]=rz0+ch; npts++; }
                    else if (ccNE)  { pts_x[npts]=x1-cc;   pts_z[npts]=z0;     npts++;
                                      pts_x[npts]=x1;      pts_z[npts]=z0+cc;  npts++; }
                    else            { pts_x[npts]=rx1;      pts_z[npts]=rz0;    npts++; }

                    /* SE corner */
                    if (cvSE)       { pts_x[npts]=rx1;     pts_z[npts]=rz1-ch; npts++;
                                      pts_x[npts]=rx1-ch;  pts_z[npts]=rz1;    npts++; }
                    else if (ccSE)  { pts_x[npts]=x1;      pts_z[npts]=z1-cc;  npts++;
                                      pts_x[npts]=x1-cc;   pts_z[npts]=z1;     npts++; }
                    else            { pts_x[npts]=rx1;      pts_z[npts]=rz1;    npts++; }

                    /* SW corner */
                    if (cvSW)       { pts_x[npts]=rx0+ch;  pts_z[npts]=rz1;    npts++;
                                      pts_x[npts]=rx0;     pts_z[npts]=rz1-ch; npts++; }
                    else if (ccSW)  { pts_x[npts]=x0+cc;   pts_z[npts]=z1;     npts++;
                                      pts_x[npts]=x0;      pts_z[npts]=z1-cc;  npts++; }
                    else            { pts_x[npts]=rx0;      pts_z[npts]=rz1;    npts++; }

                    /* Triangle fan from polygon centroid */
                    float centx = 0, centz = 0;
                    for (int pi = 0; pi < npts; pi++) { centx += pts_x[pi]; centz += pts_z[pi]; }
                    centx /= npts; centz /= npts;
                    float invCW = 1.0f / (x1 - x0), invCH = 1.0f / (z1 - z0);
                    float cu = (centx - x0) * invCW, cv = (centz - z0) * invCH;

                    for (int pi = 0; pi < npts; pi++) {
                        int ni = (pi + 1) % npts;
                        float au = (pts_x[pi] - x0) * invCW, av = (pts_z[pi] - z0) * invCH;
                        float bu = (pts_x[ni] - x0) * invCW, bv = (pts_z[ni] - z0) * invCH;

                        /* Roof (facing up — reversed winding) */
                        dng_draw_tri(fb_ptr, &mvp,
                            centx, y_hi, centz, cu, cv,
                            pts_x[pi], y_hi, pts_z[pi], au, av,
                            pts_x[ni], y_hi, pts_z[ni], bu, bv,
                            roof_tex, 0, 1, 0);
                        /* Bottom plate (facing down — reversed winding) */
                        dng_draw_tri(fb_ptr, &mvp,
                            centx, y_lo, centz, cu, cv,
                            pts_x[ni], y_lo, pts_z[ni], bu, bv,
                            pts_x[pi], y_lo, pts_z[pi], au, av,
                            roof_tex, 0, -1, 0);
                    }
                }
            }
        }

        /* Restore lighting */
        if (dng_light_mode == 0)
            sr_set_pixel_light_fn(dng_torch_light);
    }
}

/* ── Minimap ─────────────────────────────────────────────────────── */

static void minimap_pixel(uint32_t *px, int rx, int ry, uint32_t col) {
    if (rx >= 0 && rx < FB_WIDTH && ry >= 0 && ry < FB_HEIGHT)
        px[ry * FB_WIDTH + rx] = col;
}

static void minimap_line(uint32_t *px, int x0, int y0, int x1, int y1, uint32_t col) {
    int ddx = x1 - x0; if (ddx < 0) ddx = -ddx;
    int ddy = y1 - y0; if (ddy < 0) ddy = -ddy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = ddx - ddy;
    for (;;) {
        minimap_pixel(px, x0, y0, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -ddy) { err -= ddy; x0 += sx; }
        if (e2 <  ddx) { err += ddx; y0 += sy; }
    }
}

static void draw_minimap_player(sr_framebuffer *fb_ptr); /* forward decl */

static void draw_dungeon_minimap(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;
    int scale = 2;
    int mx = FB_WIDTH - d->w * scale - 4;
    int my = 28;  /* below HUD text */
    uint32_t *px = fb_ptr->color;

    for (int y = 1; y <= d->h; y++) {
        for (int x = 1; x <= d->w; x++) {
            if (d->map[y][x] == 1) continue;
            int px0 = mx + (x - 1) * scale;
            int py0 = my + (y - 1) * scale;
            uint32_t cell_col = 0xFF444444;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    minimap_pixel(px, px0 + dx, py0 + dy, cell_col);
        }
    }

    if (d->has_up) {
        int sx = d->stairs_gx, sy = d->stairs_gy;
        int px0 = mx + (sx - 1) * scale, py0 = my + (sy - 1) * scale;
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                minimap_pixel(px, px0 + dx, py0 + dy, 0xFF00CC00);
    }
    if (d->has_down) {
        int sx = d->down_gx, sy = d->down_gy;
        int px0 = mx + (sx - 1) * scale, py0 = my + (sy - 1) * scale;
        for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++)
                minimap_pixel(px, px0 + dx, py0 + dy, 0xFF0000CC);
    }

    /* Console icons */
    {
        static const uint32_t mm_con_colors[] = {
            0xFF555555, 0xFF22CCEE, 0xFF44CC44, 0xFFCC8822, 0xFFCCAA22,
            0xFF44CCCC, 0xFF44AA88, 0xFF66AA44, 0xFF6666AA, 0xFF44CC88
        };
        for (int y = 1; y <= d->h; y++)
            for (int x = 1; x <= d->w; x++) {
                uint8_t ct = d->consoles[y][x];
                if (ct == 0 || ct >= 10) continue;
                int px0 = mx + (x - 1) * scale;
                int py0 = my + (y - 1) * scale;
                uint32_t cc = mm_con_colors[ct];
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        minimap_pixel(px, px0 + dx, py0 + dy, cc);
            }
    }

    draw_minimap_player(fb_ptr);
}

/* Draw player dot + FOV cone on minimap (call after any minimap recoloring) */
static void draw_minimap_player(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;
    int scale = 2;
    int mx = FB_WIDTH - d->w * scale - 4;
    int my = 28;  /* match minimap offset */
    uint32_t *px = fb_ptr->color;

    /* Player dot */
    float pcx = mx + (p->gx - 1) * scale + scale * 0.5f;
    float pcy = my + (p->gy - 1) * scale + scale * 0.5f;
    for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++)
            minimap_pixel(px, (int)pcx - scale/2 + dx, (int)pcy - scale/2 + dy, 0xFF00FFFF);

    /* View cone */
    float cone_len = 6.0f * scale;
    float face_angle = p->angle * 2.0f * 3.14159265f;
    float fwd_sx = sinf(face_angle);
    float fwd_sy = -cosf(face_angle);

    float half_fov = 0.52f;
    float cos_hf = cosf(half_fov), sin_hf = sinf(half_fov);

    float lx = fwd_sx * cos_hf - fwd_sy * sin_hf;
    float ly = fwd_sx * sin_hf + fwd_sy * cos_hf;
    float rx = fwd_sx * cos_hf + fwd_sy * sin_hf;
    float ry = -fwd_sx * sin_hf + fwd_sy * cos_hf;

    int lx1 = (int)(pcx + lx * cone_len);
    int ly1 = (int)(pcy + ly * cone_len);
    int rx1 = (int)(pcx + rx * cone_len);
    int ry1 = (int)(pcy + ry * cone_len);

    uint32_t cone_col = 0xFF00CCCC;
    minimap_line(px, (int)pcx, (int)pcy, lx1, ly1, cone_col);
    minimap_line(px, (int)pcx, (int)pcy, rx1, ry1, cone_col);
}

/* ── Expanded map overlay ───────────────────────────────────────── */

static void draw_expanded_map(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = dng_state.dungeon;
    dng_player *p = &dng_state.player;
    uint32_t *px = fb_ptr->color;
    int W = fb_ptr->width, H = fb_ptr->height;

    /* Darken entire screen */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        int r = ((c >> 0) & 0xFF) / 5;
        int g = ((c >> 8) & 0xFF) / 5;
        int b = ((c >> 16) & 0xFF) / 5;
        px[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }

    /* Scale map to fit screen with margins */
    int margin = 20;
    int avail_w = W - margin * 2;
    int avail_h = H - margin * 2 - 16; /* leave room for title */
    int scale_x = avail_w / d->w;
    int scale_y = avail_h / d->h;
    int scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale < 3) scale = 3;
    if (scale > 12) scale = 12;

    int map_w = d->w * scale;
    int map_h = d->h * scale;
    int ox = (W - map_w) / 2;
    int oy = (H - map_h) / 2 + 8; /* offset for title */

    /* Title */
    sr_draw_text_shadow(px, W, H, W / 2 - 30, oy - 12, "DECK MAP", 0xFFFFFFFF, 0xFF000000);

    /* Room type colors (ABGR) indexed by console type value */
    static const uint32_t emap_room_colors[] = {
        0xFF555555, /* 0: CORRIDOR */
        0xFF22CCEE, /* 1: BRIDGE */
        0xFF44CC44, /* 2: MEDBAY */
        0xFFCC8822, /* 3: WEAPONS */
        0xFFCCAA22, /* 4: ENGINES */
        0xFF44CCCC, /* 5: REACTOR */
        0xFF44AA88, /* 6: SHIELDS */
        0xFF66AA44, /* 7: CARGO */
        0xFF6666AA, /* 8: BARRACKS */
        0xFF44CC88, /* 9: TELEPORTER */
    };
    static const char *emap_room_names[] = {
        "CORRIDOR", "BRIDGE", "MEDBAY", "WEAPONS", "ENGINES",
        "REACTOR", "SHIELDS", "CARGO", "BARRACKS", "TELEPORTER"
    };
    #define EMAP_ROOM_TYPE_COUNT 10

    /* Find room type for each room (from console at center) */
    int room_types[DNG_MAX_ROOMS];
    for (int ri = 0; ri < d->room_count; ri++) {
        room_types[ri] = 0;
        uint8_t ct = d->consoles[d->room_cy[ri]][d->room_cx[ri]];
        if (ct > 0 && ct < EMAP_ROOM_TYPE_COUNT) room_types[ri] = ct;
        /* Also check ship_idx for rooms assigned via ship system */
        if (room_types[ri] == 0 && d->room_ship_idx[ri] >= 0)
            room_types[ri] = -1; /* has ship data but no console — will be colored by overlay */
    }

    /* Draw cells */
    for (int gy = 1; gy <= d->h; gy++) {
        for (int gx = 1; gx <= d->w; gx++) {
            if (d->map[gy][gx] == 1) continue;

            uint32_t cell_col = 0xFF333333;
            int ri = dng_room_at(d, gx, gy);
            if (ri >= 0 && ri < d->room_count) {
                int rt = room_types[ri];
                if (rt > 0 && rt < EMAP_ROOM_TYPE_COUNT) {
                    uint32_t rc = emap_room_colors[rt];
                    int rr = ((rc >> 0) & 0xFF) * 2 / 5;
                    int rg = ((rc >> 8) & 0xFF) * 2 / 5;
                    int rb = ((rc >> 16) & 0xFF) * 2 / 5;
                    cell_col = 0xFF000000 | (rb << 16) | (rg << 8) | rr;
                } else {
                    cell_col = 0xFF444444;
                }
            }

            int px0 = ox + (gx - 1) * scale;
            int py0 = oy + (gy - 1) * scale;
            for (int dy = 0; dy < scale - 1; dy++)
                for (int dx = 0; dx < scale - 1; dx++) {
                    int rx = px0 + dx, ry = py0 + dy;
                    if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                        px[ry * W + rx] = cell_col;
                }
        }
    }

    /* Room labels */
    for (int ri = 0; ri < d->room_count; ri++) {
        int rt = room_types[ri];
        if (rt <= 0 || rt >= EMAP_ROOM_TYPE_COUNT) continue;
        const char *name = emap_room_names[rt];
        int cx = ox + (d->room_cx[ri] - 1) * scale + scale / 2;
        int cy = oy + (d->room_cy[ri] - 1) * scale + scale / 2;
        int nlen = 0; while (name[nlen]) nlen++;
        sr_draw_text_shadow(px, W, H, cx - nlen * 3, cy - 3,
                            name, emap_room_colors[rt], 0xFF000000);
    }

    /* Console icons on minimap */
    for (int gy = 1; gy <= d->h; gy++) {
        for (int gx = 1; gx <= d->w; gx++) {
            uint8_t ct = d->consoles[gy][gx];
            if (ct == 0 || ct >= EMAP_ROOM_TYPE_COUNT) continue;
            int px0 = ox + (gx - 1) * scale + scale / 2 - 1;
            int py0 = oy + (gy - 1) * scale + scale / 2 - 1;
            uint32_t cc = emap_room_colors[ct];
            for (int dy = 0; dy < 3; dy++)
                for (int dx = 0; dx < 3; dx++) {
                    int rx = px0 + dx, ry = py0 + dy;
                    if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                        px[ry * W + rx] = cc;
                }
        }
    }

    /* Up-stairs marker */
    if (d->has_up) {
        int px0 = ox + (d->stairs_gx - 1) * scale;
        int py0 = oy + (d->stairs_gy - 1) * scale;
        for (int dy = 0; dy < scale - 1; dy++)
            for (int dx = 0; dx < scale - 1; dx++) {
                int rx = px0 + dx, ry = py0 + dy;
                if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                    px[ry * W + rx] = 0xFF00CC00;
            }
    }

    /* Down-stairs marker */
    if (d->has_down) {
        int px0 = ox + (d->down_gx - 1) * scale;
        int py0 = oy + (d->down_gy - 1) * scale;
        for (int dy = 0; dy < scale - 1; dy++)
            for (int dx = 0; dx < scale - 1; dx++) {
                int rx = px0 + dx, ry = py0 + dy;
                if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                    px[ry * W + rx] = 0xFF0000CC;
            }
    }

    /* Player marker */
    {
        int px0 = ox + (p->gx - 1) * scale;
        int py0 = oy + (p->gy - 1) * scale;
        for (int dy = 0; dy < scale - 1; dy++)
            for (int dx = 0; dx < scale - 1; dx++) {
                int rx = px0 + dx, ry = py0 + dy;
                if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                    px[ry * W + rx] = 0xFF00FFFF;
            }
    }

    /* Hint at bottom */
    sr_draw_text_shadow(px, W, H, W / 2 - 40, H - 12, "TAP TO CLOSE", 0xFF888888, 0xFF000000);
}

#endif /* SR_SCENE_DUNGEON_H */
