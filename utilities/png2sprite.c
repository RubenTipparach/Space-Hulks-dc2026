/* png2sprite.c - Convert 16x16 PNGs to ABGR C arrays for sr_sprites.h
   Build: cc -o png2sprite png2sprite.c -I ../third_party -lm
   Run:   ./png2sprite
*/
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void convert(const char *png_path, const char *var_name, FILE *out) {
    int w, h, channels;
    unsigned char *data = stbi_load(png_path, &w, &h, &channels, 4);
    if (!data) { fprintf(stderr, "Failed to load %s\n", png_path); return; }
    if (w != 16 || h != 16) { fprintf(stderr, "%s is %dx%d, need 16x16\n", png_path, w, h); stbi_image_free(data); return; }

    fprintf(out, "static const uint32_t %s[SPR_W * SPR_H] = {\n", var_name);
    for (int y = 0; y < 16; y++) {
        fprintf(out, "    ");
        for (int x = 0; x < 16; x++) {
            int idx = (y * 16 + x) * 4;
            uint8_t r = data[idx+0], g = data[idx+1], b = data[idx+2], a = data[idx+3];
            uint32_t abgr;
            if (a < 128) {
                abgr = 0x00000000; /* transparent */
            } else {
                abgr = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | r;
            }
            fprintf(out, "0x%08X,", abgr);
        }
        fprintf(out, "\n");
    }
    fprintf(out, "};\n\n");
    stbi_image_free(data);
    printf("  converted %s -> %s\n", png_path, var_name);
}

int main(void) {
    FILE *out = fopen("console_sprites_out.h", "w");
    if (!out) { perror("fopen"); return 1; }

    fprintf(out, "/* Auto-generated console sprites (ABGR format) */\n\n");

    convert("../assets/sprites/console_bridge.png",   "spr_console_bridge",   out);
    convert("../assets/sprites/console_medbay.png",   "spr_console_medbay",   out);
    convert("../assets/sprites/console_weapons.png",  "spr_console_weapons",  out);
    convert("../assets/sprites/console_engines.png",  "spr_console_engines",  out);
    convert("../assets/sprites/console_reactor.png",  "spr_console_reactor",  out);
    convert("../assets/sprites/console_shields.png",  "spr_console_shields",  out);
    convert("../assets/sprites/console_cargo.png",    "spr_console_cargo",    out);
    convert("../assets/sprites/console_barracks.png", "spr_console_barracks", out);

    fprintf(out, "/* Lookup table indexed by room type (skip ROOM_CORRIDOR=0) */\n");
    fprintf(out, "static const uint32_t *spr_console_table[] = {\n");
    fprintf(out, "    NULL,                    /* ROOM_CORRIDOR - no console */\n");
    fprintf(out, "    spr_console_bridge,      /* ROOM_BRIDGE */\n");
    fprintf(out, "    spr_console_medbay,      /* ROOM_MEDBAY */\n");
    fprintf(out, "    spr_console_weapons,     /* ROOM_WEAPONS */\n");
    fprintf(out, "    spr_console_engines,     /* ROOM_ENGINES */\n");
    fprintf(out, "    spr_console_reactor,     /* ROOM_REACTOR */\n");
    fprintf(out, "    spr_console_shields,     /* ROOM_SHIELDS */\n");
    fprintf(out, "    spr_console_cargo,       /* ROOM_CARGO */\n");
    fprintf(out, "    spr_console_barracks,    /* ROOM_BARRACKS */\n");
    fprintf(out, "};\n");

    fclose(out);
    printf("Wrote console_sprites_out.h\n");
    return 0;
}
