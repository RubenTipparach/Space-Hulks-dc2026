/*  sr_sprites.h - sprite drawing utilities for combat/dungeon rendering.
 *  Single-TU header-only. Colors are ABGR (0xAABBGGRR). 0 = transparent.
 *  All character/enemy/boss sprites are loaded from PNG at runtime (stextures[]). */
#ifndef SR_SPRITES_H
#define SR_SPRITES_H

#include <stdint.h>

#define SPR_W 16
#define SPR_H 16
#define SPR_TRANS 0x00000000

/* ── Helper: draw a 16x16 sprite scaled to the framebuffer ───────── */

static void spr_draw(uint32_t *px, int fb_w, int fb_h,
                     const uint32_t *sprite, int dx, int dy, int scale) {
    for (int sy = 0; sy < SPR_H; sy++) {
        for (int sx = 0; sx < SPR_W; sx++) {
            uint32_t c = sprite[sy * SPR_W + sx];
            if (c == SPR_TRANS) continue;
            for (int j = 0; j < scale; j++) {
                for (int i = 0; i < scale; i++) {
                    int px2 = dx + sx * scale + i;
                    int py2 = dy + sy * scale + j;
                    if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                        px[py2 * fb_w + px2] = c;
                }
            }
        }
    }
}

/* Draw sprite flashing white (for damage feedback) */
static void spr_draw_flash(uint32_t *px, int fb_w, int fb_h,
                           const uint32_t *sprite, int dx, int dy, int scale) {
    for (int sy = 0; sy < SPR_H; sy++) {
        for (int sx = 0; sx < SPR_W; sx++) {
            uint32_t c = sprite[sy * SPR_W + sx];
            if (c == SPR_TRANS) continue;
            for (int j = 0; j < scale; j++) {
                for (int i = 0; i < scale; i++) {
                    int px2 = dx + sx * scale + i;
                    int py2 = dy + sy * scale + j;
                    if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                        px[py2 * fb_w + px2] = 0xFFFFFFFF;
                }
            }
        }
    }
}

/* Draw a 16x16 sprite with float scale (nearest-neighbor) */
static void spr_draw_f(uint32_t *px, int fb_w, int fb_h,
                       const uint32_t *sprite, int dx, int dy, float scale) {
    int out_w = (int)(SPR_W * scale + 0.5f);
    int out_h = (int)(SPR_H * scale + 0.5f);
    if (out_w < 1 || out_h < 1) return;
    for (int oy = 0; oy < out_h; oy++) {
        int sy = (int)(oy / scale);
        if (sy >= SPR_H) sy = SPR_H - 1;
        for (int ox = 0; ox < out_w; ox++) {
            int sx = (int)(ox / scale);
            if (sx >= SPR_W) sx = SPR_W - 1;
            uint32_t c = sprite[sy * SPR_W + sx];
            if (c == SPR_TRANS) continue;
            int px2 = dx + ox;
            int py2 = dy + oy;
            if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                px[py2 * fb_w + px2] = c;
        }
    }
}

/* Draw sprite flashing white with float scale */
static void spr_draw_flash_f(uint32_t *px, int fb_w, int fb_h,
                             const uint32_t *sprite, int dx, int dy, float scale) {
    int out_w = (int)(SPR_W * scale + 0.5f);
    int out_h = (int)(SPR_H * scale + 0.5f);
    if (out_w < 1 || out_h < 1) return;
    for (int oy = 0; oy < out_h; oy++) {
        int sy = (int)(oy / scale);
        if (sy >= SPR_H) sy = SPR_H - 1;
        for (int ox = 0; ox < out_w; ox++) {
            int sx = (int)(ox / scale);
            if (sx >= SPR_W) sx = SPR_W - 1;
            uint32_t c = sprite[sy * SPR_W + sx];
            if (c == SPR_TRANS) continue;
            int px2 = dx + ox;
            int py2 = dy + oy;
            if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                px[py2 * fb_w + px2] = 0xFFFFFFFF;
        }
    }
}

/* Draw a sprite of arbitrary source size (for 32x32 boss sprites etc.) */
static void spr_draw_nf(uint32_t *px, int fb_w, int fb_h,
                        const uint32_t *sprite, int src_w, int src_h,
                        int dx, int dy, float scale) {
    int out_w = (int)(src_w * scale + 0.5f);
    int out_h = (int)(src_h * scale + 0.5f);
    if (out_w < 1 || out_h < 1) return;
    for (int oy = 0; oy < out_h; oy++) {
        int sy = (int)(oy / scale);
        if (sy >= src_h) sy = src_h - 1;
        for (int ox = 0; ox < out_w; ox++) {
            int sx = (int)(ox / scale);
            if (sx >= src_w) sx = src_w - 1;
            uint32_t c = sprite[sy * src_w + sx];
            if (c == SPR_TRANS) continue;
            int px2 = dx + ox, py2 = dy + oy;
            if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                px[py2 * fb_w + px2] = c;
        }
    }
}

static void spr_draw_flash_nf(uint32_t *px, int fb_w, int fb_h,
                              const uint32_t *sprite, int src_w, int src_h,
                              int dx, int dy, float scale) {
    int out_w = (int)(src_w * scale + 0.5f);
    int out_h = (int)(src_h * scale + 0.5f);
    if (out_w < 1 || out_h < 1) return;
    for (int oy = 0; oy < out_h; oy++) {
        int sy = (int)(oy / scale);
        if (sy >= src_h) sy = src_h - 1;
        for (int ox = 0; ox < out_w; ox++) {
            int sx = (int)(ox / scale);
            if (sx >= src_w) sx = src_w - 1;
            uint32_t c = sprite[sy * src_w + sx];
            if (c == SPR_TRANS) continue;
            int px2 = dx + ox, py2 = dy + oy;
            if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                px[py2 * fb_w + px2] = 0xFFFFFFFF;
        }
    }
}

/* Draw a sprite from an sr_texture (PNG-loaded) at framebuffer position, with scaling */
static void spr_draw_tex(uint32_t *px, int fb_w, int fb_h,
                         const sr_texture *tex, int dx, int dy, int scale) {
    for (int sy = 0; sy < tex->height; sy++) {
        for (int sx = 0; sx < tex->width; sx++) {
            uint32_t c = tex->pixels[sy * tex->width + sx];
            uint8_t a = (c >> 24) & 0xFF;
            if (a < 128) continue;  /* skip transparent */
            for (int j = 0; j < scale; j++) {
                for (int i = 0; i < scale; i++) {
                    int px2 = dx + sx * scale + i;
                    int py2 = dy + sy * scale + j;
                    if (px2 >= 0 && px2 < fb_w && py2 >= 0 && py2 < fb_h)
                        px[py2 * fb_w + px2] = c;
                }
            }
        }
    }
}

/* ── Console sprites (per room type) ─────────────────────────────── */
#include "sr_console_sprites.h"

/* ── Card art sprites ───────────────────────────────────────────── */
#include "sr_card_sprites.h"

#endif /* SR_SPRITES_H */
