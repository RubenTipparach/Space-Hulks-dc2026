/* gen_consoles.c - Generate 16x16 console sprite PNGs for each room type.
   Build: cc -o gen_consoles gen_consoles.c -I ../third_party -lm
   Run:   ./gen_consoles
*/
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define W 16
#define H 16
#define T 0x00000000  /* transparent */

/* Pack RGBA for PNG output (stb wants RGBA byte order) */
static uint32_t rgba(int r, int g, int b, int a) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

/* Color helpers */
#define BLK rgba(0,0,0,255)
#define DRK rgba(30,30,40,255)
#define GR1 rgba(60,60,70,255)
#define GR2 rgba(90,90,100,255)
#define GR3 rgba(120,120,130,255)
#define WHT rgba(200,200,210,255)

/* Base console body (shared shape: monitor on a stand) */
static void draw_base(uint32_t *px) {
    /* Clear */
    for (int i = 0; i < W*H; i++) px[i] = T;

    /* Stand/base */
    for (int x = 5; x <= 10; x++) px[15*W+x] = GR1;  /* bottom bar */
    for (int x = 6; x <= 9; x++)  px[14*W+x] = GR2;  /* stand neck */
    px[13*W+7] = GR2; px[13*W+8] = GR2;               /* narrow neck */

    /* Monitor casing (outer) */
    for (int x = 1; x <= 14; x++) { px[2*W+x] = BLK; px[12*W+x] = BLK; }
    for (int y = 2; y <= 12; y++) { px[y*W+1] = BLK; px[y*W+14] = BLK; }

    /* Monitor casing fill (dark gray body) */
    for (int y = 3; y <= 11; y++)
        for (int x = 2; x <= 13; x++)
            px[y*W+x] = DRK;

    /* Screen bezel (inner border) */
    for (int x = 3; x <= 12; x++) { px[3*W+x] = GR1; px[10*W+x] = GR1; }
    for (int y = 3; y <= 10; y++) { px[y*W+3] = GR1; px[y*W+12] = GR1; }

    /* Screen area (dark bg, will be customized) */
    for (int y = 4; y <= 9; y++)
        for (int x = 4; x <= 11; x++)
            px[y*W+x] = rgba(10,15,20,255);

    /* Small power LED on bezel */
    px[11*W+12] = rgba(0,180,0,255);
}

/* --- Per-room-type screen content --- */

/* BRIDGE: command star/crosshair on screen */
static void screen_bridge(uint32_t *px) {
    uint32_t cy = rgba(34,204,238,255);   /* cyan */
    uint32_t cd = rgba(20,140,170,255);
    /* Crosshair */
    for (int x = 5; x <= 10; x++) px[7*W+x] = cy;
    for (int y = 5; y <= 8; y++)  px[y*W+7] = cy;
    px[5*W+8] = cy; px[8*W+8] = cy;
    /* Corner dots */
    px[5*W+5] = cd; px[5*W+10] = cd;
    px[9*W+5] = cd; px[9*W+10] = cd;
    /* Star center */
    px[7*W+7] = rgba(255,255,255,255);
    px[7*W+8] = rgba(255,255,255,255);
}

/* MEDBAY: heart/cross on screen */
static void screen_medbay(uint32_t *px) {
    uint32_t grn = rgba(34,204,68,255);
    uint32_t gd  = rgba(20,160,40,255);
    /* Medical cross */
    for (int x = 6; x <= 9; x++) { px[5*W+x] = grn; px[9*W+x] = grn; }
    for (int y = 5; y <= 9; y++) { px[y*W+7] = grn; px[y*W+8] = grn; }
    /* Pulse line across bottom */
    px[8*W+4] = gd; px[8*W+5] = gd;
    px[7*W+6] = gd;
    px[6*W+7] = gd;
    px[7*W+8] = grn;
    px[8*W+9] = gd; px[8*W+10] = gd; px[8*W+11] = gd;
}

/* WEAPONS: targeting reticle */
static void screen_weapons(uint32_t *px) {
    uint32_t red = rgba(255,68,34,255);
    uint32_t rd  = rgba(180,40,20,255);
    /* Outer ring (diamond shape) */
    px[4*W+7] = red; px[4*W+8] = red;
    px[5*W+5] = rd;  px[5*W+10] = rd;
    px[7*W+4] = red; px[7*W+11] = red;
    px[8*W+4] = red; px[8*W+11] = red;
    px[9*W+5] = rd;  px[9*W+10] = rd;
    px[9*W+7] = red; px[9*W+8] = red;
    /* Crosshair lines */
    px[6*W+7] = red; px[6*W+8] = red;
    px[7*W+6] = red; px[7*W+9] = red;
    px[8*W+6] = red; px[8*W+9] = red;
    /* Center dot */
    px[7*W+7] = rgba(255,200,200,255);
    px[7*W+8] = rgba(255,200,200,255);
    px[8*W+7] = rgba(255,200,200,255);
    px[8*W+8] = rgba(255,200,200,255);
}

/* ENGINES: spinning turbine blades */
static void screen_engines(uint32_t *px) {
    uint32_t stl = rgba(68,170,204,255);
    uint32_t sd  = rgba(40,120,160,255);
    /* Center hub */
    px[7*W+7] = rgba(200,200,220,255);
    px[7*W+8] = rgba(200,200,220,255);
    /* Blades radiating out */
    px[5*W+6] = stl; px[4*W+5] = sd;     /* top-left */
    px[5*W+9] = stl; px[4*W+10] = sd;    /* top-right */
    px[9*W+6] = stl; px[9*W+5] = sd;     /* bottom-left (use 9 not 10) */
    px[9*W+9] = stl; px[9*W+10] = sd;    /* bottom-right */
    /* Exhaust indicators */
    px[6*W+4] = sd; px[6*W+11] = sd;
    px[8*W+4] = sd; px[8*W+11] = sd;
    /* RPM bar at bottom */
    for (int x = 5; x <= 10; x++) px[9*W+x] = stl;
}

/* REACTOR: radiation/atom symbol */
static void screen_reactor(uint32_t *px) {
    uint32_t org = rgba(255,170,0,255);
    uint32_t od  = rgba(200,120,0,255);
    /* Center core (bright) */
    px[6*W+7] = rgba(255,255,100,255);
    px[6*W+8] = rgba(255,255,100,255);
    px[7*W+7] = rgba(255,255,200,255);
    px[7*W+8] = rgba(255,255,200,255);
    /* Radiation rings */
    px[4*W+7] = org; px[4*W+8] = org;
    px[5*W+5] = od;  px[5*W+10] = od;
    px[7*W+4] = org; px[7*W+11] = org;
    px[9*W+5] = od;  px[9*W+10] = od;
    px[9*W+7] = org; px[9*W+8] = org;
    /* Warning bars */
    px[5*W+6] = org; px[5*W+9] = org;
    px[8*W+5] = od;  px[8*W+10] = od;
    px[8*W+7] = org; px[8*W+8] = org;
}

/* SHIELDS: dome/arc */
static void screen_shields(uint32_t *px) {
    uint32_t blu = rgba(204,170,34,255);  /* gold-blue in RGBA */
    uint32_t bd  = rgba(150,120,20,255);
    /* Shield dome arc */
    px[4*W+6] = blu; px[4*W+7] = blu; px[4*W+8] = blu; px[4*W+9] = blu;
    px[5*W+5] = bd;  px[5*W+10] = bd;
    px[6*W+4] = bd;  px[6*W+11] = bd;
    px[7*W+4] = blu; px[7*W+11] = blu;
    px[8*W+5] = bd;  px[8*W+10] = bd;
    px[9*W+6] = blu; px[9*W+7] = blu; px[9*W+8] = blu; px[9*W+9] = blu;
    /* Inner glow */
    px[6*W+7] = rgba(180,160,80,255);
    px[6*W+8] = rgba(180,160,80,255);
    px[7*W+7] = rgba(220,200,100,255);
    px[7*W+8] = rgba(220,200,100,255);
}

/* CARGO: crate/box icon */
static void screen_cargo(uint32_t *px) {
    uint32_t olv = rgba(136,153,68,255);
    uint32_t od  = rgba(100,110,50,255);
    /* Box outline */
    for (int x = 5; x <= 10; x++) { px[5*W+x] = olv; px[9*W+x] = olv; }
    for (int y = 5; y <= 9; y++)  { px[y*W+5] = olv; px[y*W+10] = olv; }
    /* Cross straps */
    for (int i = 5; i <= 10; i++) { px[7*W+i] = od; px[i >= 5 && i <= 9 ? i : 5*W+7] = od; }
    px[5*W+7] = od; px[6*W+7] = od; px[8*W+7] = od; px[9*W+7] = od;
    /* Lock */
    px[7*W+7] = rgba(200,180,80,255);
    px[7*W+8] = rgba(200,180,80,255);
}

/* BARRACKS: bunk/bed icon */
static void screen_barracks(uint32_t *px) {
    uint32_t slt = rgba(102,102,136,255);
    uint32_t sd  = rgba(70,70,100,255);
    /* Bunk bed frame */
    for (int x = 5; x <= 10; x++) { px[5*W+x] = slt; px[7*W+x] = sd; px[9*W+x] = slt; }
    px[5*W+5] = slt; px[6*W+5] = sd; px[7*W+5] = sd; px[8*W+5] = sd; px[9*W+5] = slt;
    px[5*W+10] = slt; px[6*W+10] = sd; px[7*W+10] = sd; px[8*W+10] = sd; px[9*W+10] = slt;
    /* Pillow dots */
    px[6*W+6] = rgba(150,150,170,255);
    px[8*W+6] = rgba(150,150,170,255);
}

typedef struct {
    const char *name;
    void (*draw_screen)(uint32_t *px);
} console_def;

int main(void) {
    console_def consoles[] = {
        { "bridge",   screen_bridge },
        { "medbay",   screen_medbay },
        { "weapons",  screen_weapons },
        { "engines",  screen_engines },
        { "reactor",  screen_reactor },
        { "shields",  screen_shields },
        { "cargo",    screen_cargo },
        { "barracks", screen_barracks },
    };
    int count = sizeof(consoles) / sizeof(consoles[0]);

    for (int i = 0; i < count; i++) {
        uint32_t px[W * H];
        draw_base(px);
        consoles[i].draw_screen(px);

        char path[128];
        snprintf(path, sizeof(path), "../assets/sprites/console_%s.png", consoles[i].name);
        stbi_write_png(path, W, H, 4, px, W * 4);
        printf("wrote %s\n", path);
    }
    return 0;
}
