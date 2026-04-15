/*  sr_menu.h - Stats overlay (dungeon-only).
 *  Single-TU header-only. Depends on sr_app.h, sr_font.h. */
#ifndef SR_MENU_H
#define SR_MENU_H

static void draw_stats(sr_framebuffer *fb_ptr, int tris) {
    (void)tris;
    /* Recording indicator */
    if (sr_gif_is_recording()) {
        sr_draw_text_shadow(fb_ptr->color, fb_ptr->width, fb_ptr->height,
                            FB_WIDTH - 30, 3, "REC", 0xFF0000FF, 0xFF000000);
    }
}

#endif /* SR_MENU_H */
