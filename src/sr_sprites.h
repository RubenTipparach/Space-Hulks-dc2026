/*  sr_sprites.h — 16x16 pixel art sprites for combat.
 *  Single-TU header-only. Colors are ABGR (0xAABBGGRR). 0 = transparent. */
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

/* ── Color shortcuts ─────────────────────────────────────────────── */

#define _BK 0xFF000000   /* black */
#define _DK 0xFF1A1A1A   /* very dark */
#define _DG 0xFF333333   /* dark gray */
#define _MG 0xFF666666   /* mid gray */
#define _LG 0xFFAAAAAA   /* light gray */

#define _DR 0xFF222299   /* dark red */
#define _RD 0xFF3333CC   /* red */
#define _LR 0xFF5555FF   /* light red */
#define _OR 0xFF4488EE   /* orange */

#define _G1 0xFF116611   /* dark green */
#define _G2 0xFF22AA22   /* green */
#define _G3 0xFF44DD44   /* light green */
#define _G4 0xFF88FF88   /* pale green */

#define _P1 0xFF662244   /* dark purple */
#define _P2 0xFFAA3366   /* purple */
#define _P3 0xFFCC55AA   /* light purple */
#define _P4 0xFFEE88CC   /* pale purple */

#define _B1 0xFF882211   /* dark blue */
#define _B2 0xFFCC4422   /* blue */
#define _B3 0xFFEE7744   /* light blue */

#define _Y1 0xFF22AAAA   /* dark yellow */
#define _Y2 0xFF44DDDD   /* yellow */
#define _Y3 0xFF88FFFF   /* bright yellow */

#define _WH 0xFFFFFFFF   /* white */
#define __ SPR_TRANS      /* transparent */

/* ══════════════════════════════════════════════════════════════════
 *  ALIEN SPRITES
 * ══════════════════════════════════════════════════════════════════ */

/* Lurker — small, fast insectoid alien (green) */
static const uint32_t spr_lurker[SPR_W * SPR_H] = {
    __, __, __, __, __, _G1, __, __, __, __, _G1, __, __, __, __, __,
    __, __, __, __, _G1, _G2, _G1, __, __, _G1, _G2, _G1, __, __, __, __,
    __, __, __, __, __, _G2, _G3, _G1, _G1, _G3, _G2, __, __, __, __, __,
    __, __, __, __, _G1, _G2, _G3, _G3, _G3, _G3, _G2, _G1, __, __, __, __,
    __, __, __, _G1, _G2, _G3, _Y2, _G3, _G3, _Y2, _G3, _G2, _G1, __, __, __,
    __, __, _G1, _G2, _G2, _G3, _G3, _G3, _G3, _G3, _G3, _G2, _G2, _G1, __, __,
    __, __, __, _G1, _G2, _G2, _G3, _G2, _G2, _G3, _G2, _G2, _G1, __, __, __,
    __, _G1, __, __, _G1, _G2, _G2, _G2, _G2, _G2, _G2, _G1, __, __, _G1, __,
    _G1, _G2, _G1, __, __, _G1, _G2, _G2, _G2, _G2, _G1, __, __, _G1, _G2, _G1,
    __, _G1, _G2, _G1, __, _G1, _G1, _G2, _G2, _G1, _G1, __, _G1, _G2, _G1, __,
    __, __, _G1, _G2, _G1, __, _G1, _G1, _G1, _G1, __, _G1, _G2, _G1, __, __,
    __, __, __, _G1, __, __, __, _G1, _G1, __, __, __, _G1, __, __, __,
    __, __, __, __, __, __, _G1, __, __, _G1, __, __, __, __, __, __,
    __, __, __, __, __, _G1, __, __, __, __, _G1, __, __, __, __, __,
    __, __, __, __, _G1, __, __, __, __, __, __, _G1, __, __, __, __,
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
};

/* Brute — hulking heavy alien (red/brown) */
static const uint32_t spr_brute[SPR_W * SPR_H] = {
    __, __, __, __, __, __, _DR, _DR, _DR, _DR, __, __, __, __, __, __,
    __, __, __, __, __, _DR, _RD, _RD, _RD, _RD, _DR, __, __, __, __, __,
    __, __, __, __, _DR, _RD, _LR, _RD, _RD, _LR, _RD, _DR, __, __, __, __,
    __, __, __, __, _DR, _RD, _Y2, _RD, _RD, _Y2, _RD, _DR, __, __, __, __,
    __, __, __, __, __, _DR, _RD, _RD, _RD, _RD, _DR, __, __, __, __, __,
    __, __, __, __, _DR, _RD, _RD, _LR, _LR, _RD, _RD, _DR, __, __, __, __,
    __, __, __, _DR, _RD, _RD, _LR, _LR, _LR, _LR, _RD, _RD, _DR, __, __, __,
    __, __, _DR, _RD, _RD, _LR, _LR, _RD, _RD, _LR, _LR, _RD, _RD, _DR, __, __,
    __, _DR, _RD, _RD, _LR, _LR, _RD, _RD, _RD, _RD, _LR, _LR, _RD, _RD, _DR, __,
    __, _DR, _RD, _RD, _LR, _RD, _RD, _RD, _RD, _RD, _RD, _LR, _RD, _RD, _DR, __,
    __, __, _DR, _RD, _RD, _RD, _RD, _RD, _RD, _RD, _RD, _RD, _RD, _DR, __, __,
    __, __, __, _DR, _RD, _RD, _RD, _RD, _RD, _RD, _RD, _RD, _DR, __, __, __,
    __, __, __, __, _DR, _RD, _RD, __, __, _RD, _RD, _DR, __, __, __, __,
    __, __, __, __, _DR, _RD, _DR, __, __, _DR, _RD, _DR, __, __, __, __,
    __, __, __, _DR, _RD, _DR, __, __, __, __, _DR, _RD, _DR, __, __, __,
    __, __, __, _DR, _DR, __, __, __, __, __, __, _DR, _DR, __, __, __,
};

/* Spitter — tall ranged alien (purple) */
static const uint32_t spr_spitter[SPR_W * SPR_H] = {
    __, __, __, __, __, __, _P1, _P1, _P1, _P1, __, __, __, __, __, __,
    __, __, __, __, __, _P1, _P2, _P2, _P2, _P2, _P1, __, __, __, __, __,
    __, __, __, __, _P1, _P2, _Y2, _P2, _P2, _Y2, _P2, _P1, __, __, __, __,
    __, __, __, __, _P1, _P2, _P3, _P2, _P2, _P3, _P2, _P1, __, __, __, __,
    __, __, __, __, __, _P1, _P2, _P2, _P2, _P2, _P1, __, __, __, __, __,
    __, __, __, __, __, _P1, _P1, _G3, _G3, _P1, _P1, __, __, __, __, __,
    __, __, __, __, __, __, _P1, _P2, _P2, _P1, __, __, __, __, __, __,
    __, __, __, __, __, _P1, _P2, _P3, _P3, _P2, _P1, __, __, __, __, __,
    __, __, __, _P1, _P1, _P2, _P3, _P3, _P3, _P3, _P2, _P1, _P1, __, __, __,
    __, __, _P1, _P2, __, _P1, _P2, _P3, _P3, _P2, _P1, __, _P2, _P1, __, __,
    __, _P1, _P2, __, __, __, _P1, _P2, _P2, _P1, __, __, __, _P2, _P1, __,
    __, _P1, __, __, __, __, _P1, _P2, _P2, _P1, __, __, __, __, _P1, __,
    __, __, __, __, __, __, _P1, _P2, _P2, _P1, __, __, __, __, __, __,
    __, __, __, __, __, _P1, _P2, __, __, _P2, _P1, __, __, __, __, __,
    __, __, __, __, _P1, _P2, __, __, __, __, _P2, _P1, __, __, __, __,
    __, __, __, __, _P1, _P1, __, __, __, __, _P1, _P1, __, __, __, __,
};

/* Hive Guard — elite armored alien (blue/gray) */
static const uint32_t spr_hiveguard[SPR_W * SPR_H] = {
    __, __, __, __, __, _B1, _B1, _B2, _B2, _B1, _B1, __, __, __, __, __,
    __, __, __, __, _B1, _B2, _B2, _B3, _B3, _B2, _B2, _B1, __, __, __, __,
    __, __, __, _B1, _B2, _B3, _LR, _B3, _B3, _LR, _B3, _B2, _B1, __, __, __,
    __, __, __, _B1, _B2, _B3, _B3, _B3, _B3, _B3, _B3, _B2, _B1, __, __, __,
    __, __, __, __, _B1, _B2, _B2, _B2, _B2, _B2, _B2, _B1, __, __, __, __,
    __, __, __, _DG, _B1, _B2, _B3, _B3, _B3, _B3, _B2, _B1, _DG, __, __, __,
    __, __, _DG, _MG, _B1, _B2, _B3, _B3, _B3, _B3, _B2, _B1, _MG, _DG, __, __,
    __, _DG, _MG, _DG, _B1, _B2, _B3, _B2, _B2, _B3, _B2, _B1, _DG, _MG, _DG, __,
    __, _DG, _MG, __, _B1, _B2, _B2, _B2, _B2, _B2, _B2, _B1, __, _MG, _DG, __,
    __, __, _DG, __, _B1, _B2, _B2, _B2, _B2, _B2, _B2, _B1, __, _DG, __, __,
    __, __, __, __, _B1, _B2, _B2, _B2, _B2, _B2, _B2, _B1, __, __, __, __,
    __, __, __, __, __, _B1, _B2, _B2, _B2, _B2, _B1, __, __, __, __, __,
    __, __, __, __, _B1, _B2, _B1, __, __, _B1, _B2, _B1, __, __, __, __,
    __, __, __, _B1, _B2, _B1, __, __, __, __, _B1, _B2, _B1, __, __, __,
    __, __, __, _B1, _B1, __, __, __, __, __, __, _B1, _B1, __, __, __,
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
};

/* ── Player sprites ──────────────────────────────────────────────── */

/* Scout — light armor, visor (cyan tones) */
static const uint32_t spr_scout[SPR_W * SPR_H] = {
    __, __, __, __, __, __, _DG, _DG, _DG, _DG, __, __, __, __, __, __,
    __, __, __, __, __, _DG, _MG, _MG, _MG, _MG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, _B3, _B3, _B3, _B3, _MG, _DG, __, __, __, __,
    __, __, __, __, _DG, _MG, _MG, _MG, _MG, _MG, _MG, _DG, __, __, __, __,
    __, __, __, __, __, _DG, _DG, _MG, _MG, _DG, _DG, __, __, __, __, __,
    __, __, __, __, __, _DG, _MG, _MG, _MG, _MG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, _LG, _MG, _MG, _LG, _MG, _DG, __, __, __, __,
    __, __, __, _DG, _MG, _MG, _LG, _LG, _LG, _LG, _MG, _MG, _DG, __, __, __,
    __, __, _DG, _MG, __, _DG, _MG, _LG, _LG, _MG, _DG, __, _MG, _DG, __, __,
    __, __, _DG, __, __, __, _DG, _MG, _MG, _DG, __, __, __, _DG, __, __,
    __, __, __, __, __, __, _DG, _MG, _MG, _DG, __, __, __, __, __, __,
    __, __, __, __, __, __, _DG, _MG, _MG, _DG, __, __, __, __, __, __,
    __, __, __, __, __, _DG, _MG, __, __, _MG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, __, __, __, __, _MG, _DG, __, __, __, __,
    __, __, __, __, _DG, _DG, __, __, __, __, _DG, _DG, __, __, __, __,
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
};

/* Marine — heavy armor, red visor (darker, bulkier) */
static const uint32_t spr_marine[SPR_W * SPR_H] = {
    __, __, __, __, __, _DG, _DG, _DG, _DG, _DG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, _MG, _MG, _MG, _MG, _MG, _DG, __, __, __, __,
    __, __, __, _DG, _MG, _MG, _LR, _LR, _LR, _LR, _MG, _MG, _DG, __, __, __,
    __, __, __, _DG, _MG, _MG, _MG, _MG, _MG, _MG, _MG, _MG, _DG, __, __, __,
    __, __, __, __, _DG, _DG, _DG, _MG, _MG, _DG, _DG, _DG, __, __, __, __,
    __, __, __, _DG, _MG, _LG, _MG, _MG, _MG, _MG, _LG, _MG, _DG, __, __, __,
    __, __, _DG, _MG, _LG, _LG, _MG, _MG, _MG, _MG, _LG, _LG, _MG, _DG, __, __,
    __, _DG, _MG, _LG, _LG, _MG, _MG, _LG, _LG, _MG, _MG, _LG, _LG, _MG, _DG, __,
    __, _DG, _MG, _LG, __, _DG, _MG, _LG, _LG, _MG, _DG, __, _LG, _MG, _DG, __,
    __, __, _DG, __, __, __, _DG, _MG, _MG, _DG, __, __, __, _DG, __, __,
    __, __, __, __, __, _DG, _MG, _MG, _MG, _MG, _DG, __, __, __, __, __,
    __, __, __, __, __, _DG, _MG, _MG, _MG, _MG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, _MG, __, __, _MG, _MG, _DG, __, __, __, __,
    __, __, __, _DG, _MG, _MG, __, __, __, __, _MG, _MG, _DG, __, __, __,
    __, __, __, _DG, _DG, _DG, __, __, __, __, _DG, _DG, _DG, __, __, __,
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
};

/* Engineer — bulky armor, orange visor (welding tones) */
static const uint32_t spr_engineer[SPR_W * SPR_H] = {
    __, __, __, __, __, _DG, _DG, _DG, _DG, _DG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, _MG, _MG, _MG, _MG, _MG, _DG, __, __, __, __,
    __, __, __, _DG, _MG, _MG, _OR, _OR, _OR, _OR, _MG, _MG, _DG, __, __, __,
    __, __, __, _DG, _MG, _MG, _MG, _MG, _MG, _MG, _MG, _MG, _DG, __, __, __,
    __, __, __, __, _DG, _DG, _DG, _MG, _MG, _DG, _DG, _DG, __, __, __, __,
    __, __, __, _DG, _Y1, _LG, _MG, _MG, _MG, _MG, _LG, _Y1, _DG, __, __, __,
    __, __, _DG, _Y1, _LG, _LG, _MG, _MG, _MG, _MG, _LG, _LG, _Y1, _DG, __, __,
    __, _DG, _Y1, _LG, _LG, _MG, _MG, _LG, _LG, _MG, _MG, _LG, _LG, _Y1, _DG, __,
    __, _DG, _Y1, _LG, __, _DG, _MG, _LG, _LG, _MG, _DG, __, _LG, _Y1, _DG, __,
    __, __, _DG, __, __, __, _DG, _MG, _MG, _DG, __, __, __, _DG, __, __,
    __, __, __, __, __, _DG, _Y1, _MG, _MG, _Y1, _DG, __, __, __, __, __,
    __, __, __, __, __, _DG, _MG, _MG, _MG, _MG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _MG, _MG, __, __, _MG, _MG, _DG, __, __, __, __,
    __, __, __, _DG, _Y1, _MG, __, __, __, __, _MG, _Y1, _DG, __, __, __,
    __, __, __, _DG, _DG, _DG, __, __, __, __, _DG, _DG, _DG, __, __, __,
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
};

/* Scientist — sleek suit, cyan visor (tech/lab tones) */
static const uint32_t spr_scientist[SPR_W * SPR_H] = {
    __, __, __, __, __, __, _DG, _DG, _DG, _DG, __, __, __, __, __, __,
    __, __, __, __, __, _DG, _LG, _LG, _LG, _LG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _LG, _B3, _B3, _B3, _B3, _LG, _DG, __, __, __, __,
    __, __, __, __, _DG, _LG, _LG, _LG, _LG, _LG, _LG, _DG, __, __, __, __,
    __, __, __, __, __, _DG, _DG, _LG, _LG, _DG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _WH, _LG, _LG, _LG, _LG, _WH, _DG, __, __, __, __,
    __, __, __, _DG, _WH, _WH, _LG, _LG, _LG, _LG, _WH, _WH, _DG, __, __, __,
    __, __, _DG, _WH, _WH, _LG, _LG, _WH, _WH, _LG, _LG, _WH, _WH, _DG, __, __,
    __, __, _DG, _WH, __, _DG, _LG, _WH, _WH, _LG, _DG, __, _WH, _DG, __, __,
    __, __, __, __, __, __, _DG, _LG, _LG, _DG, __, __, __, __, __, __,
    __, __, __, __, __, __, _DG, _LG, _LG, _DG, __, __, __, __, __, __,
    __, __, __, __, __, __, _DG, _WH, _WH, _DG, __, __, __, __, __, __,
    __, __, __, __, __, _DG, _LG, __, __, _LG, _DG, __, __, __, __, __,
    __, __, __, __, _DG, _LG, __, __, __, __, _LG, _DG, __, __, __, __,
    __, __, __, __, _DG, _DG, __, __, __, __, _DG, _DG, __, __, __, __,
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
};

/* Sprite table for enemy types (indexed by enemy_type enum) */
static const uint32_t *spr_enemy_table[] = {
    spr_lurker,
    spr_brute,
    spr_spitter,
    spr_hiveguard,
};

/* Undefine shortcuts */
#undef _BK
#undef _DK
#undef _DG
#undef _MG
#undef _LG
#undef _DR
#undef _RD
#undef _LR
#undef _OR
#undef _G1
#undef _G2
#undef _G3
#undef _G4
#undef _P1
#undef _P2
#undef _P3
#undef _P4
#undef _B1
#undef _B2
#undef _B3
#undef _Y1
#undef _Y2
#undef _Y3
#undef _WH
#undef __

/* ── Console sprites (per room type) ─────────────────────────────── */
#include "sr_console_sprites.h"

#endif /* SR_SPRITES_H */
