/*  sr_mobile_input.h - Touch / swipe input for mobile.
 *  Single-TU header-only. Depends on sr_app.h, sr_scene_dungeon.h. */
#ifndef SR_MOBILE_INPUT_H
#define SR_MOBILE_INPUT_H

/* ── Touch / swipe state ─────────────────────────────────────────── */

static bool   touch_active = false;
static float  touch_start_sx, touch_start_sy;
static double touch_start_time;
static float  touch_cur_sx, touch_cur_sy;

#define TOUCH_TAP_MAX_TIME   0.25
#define TOUCH_SWIPE_MIN_DIST 30.0f
/* Central horizontal deadzone (fraction of screen width). Taps inside this
   zone do NOT trigger a strafe - prevents accidental side-steps when the
   player taps near the middle of the screen. 0.4 = middle 40%. */
#define TOUCH_STRAFE_DEADZONE 0.4f


static void dng_touch_began(float sx, float sy, double time) {
    touch_active = true;
    touch_start_sx = sx;
    touch_start_sy = sy;
    touch_cur_sx = sx;
    touch_cur_sy = sy;
    touch_start_time = time;
}

static void dng_touch_moved(float sx, float sy) {
    if (touch_active) {
        touch_cur_sx = sx;
        touch_cur_sy = sy;
    }
}

static void dng_touch_ended(float sx, float sy, double time) {
    if (!touch_active) return;
    touch_active = false;

    if (dng_play_state != DNG_STATE_PLAYING) return;

    float dx = sx - touch_start_sx;
    float dy = sy - touch_start_sy;
    float dist = sqrtf(dx * dx + dy * dy);
    double duration = time - touch_start_time;

    if (dist < TOUCH_SWIPE_MIN_DIST && duration < TOUCH_TAP_MAX_TIME) {
        /* If expanded map is open, any tap closes it */
        if (dng_expanded_map) {
            dng_expanded_map = false;
            return;
        }

        /* Check if tap is on the minimap area - open expanded map */
        {
            float fbx, fby;
            screen_to_fb(sx, sy, &fbx, &fby);
            sr_dungeon *md = dng_state.dungeon;
            int mscale = 2;
            int mmx = FB_WIDTH - md->w * mscale - 4;
            int mmy = dng_minimap_y;
            int mmw = md->w * mscale;
            int mmh = md->h * mscale;
            if (fbx >= mmx && fbx <= mmx + mmw && fby >= mmy && fby <= mmy + mmh) {
                dng_expanded_map = true;
                return;
            }
        }

        /* Convert to FB coords to check button areas */
        float fbx2, fby2;
        screen_to_fb(sx, sy, &fbx2, &fby2);

        /* Check if tap is in a button zone (top-right deck button) */
        bool in_button_zone = (fbx2 >= FB_WIDTH - 70 && fby2 <= 28);

        /* Set click state for ui_button detection */
        handle_screen_tap(sx, sy);

        /* Short tap - strafe based on screen half, only if not on a button.
           A central deadzone (middle TOUCH_STRAFE_DEADZONE of the screen)
           ignores the tap to prevent accidental strafes. */
        if (!in_button_zone) {
            float screen_w = sapp_widthf();
            float mid_x = screen_w * 0.5f;
            float half_dead = screen_w * TOUCH_STRAFE_DEADZONE * 0.5f;
            if (sx < mid_x - half_dead) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 3) % 4);
            } else if (sx > mid_x + half_dead) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 1) % 4);
            }
        }
    } else if (dist >= TOUCH_SWIPE_MIN_DIST) {
        /* Swipe - determine cardinal direction */
        float adx = dx < 0 ? -dx : dx;
        float ady = dy < 0 ? -dy : dy;

        if (ady > adx) {
            if (dy < 0) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    dng_state.player.dir);
            } else {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 2) % 4);
            }
        } else {
            if (dx < 0) {
                dng_state.player.dir = (dng_state.player.dir + 3) % 4;
                dng_state.player.target_angle -= 0.25f;
            } else {
                dng_state.player.dir = (dng_state.player.dir + 1) % 4;
                dng_state.player.target_angle += 0.25f;
            }
        }
    }
}

static void dng_touch_cancelled(void) {
    touch_active = false;
}

/* ── Hub touch (same swipe/tap logic, operates on g_hub) ────────── */

static void hub_touch_ended(float sx, float sy, double time) {
    if (!touch_active) return;
    touch_active = false;

    float dx = sx - touch_start_sx;
    float dy = sy - touch_start_sy;
    float dist = sqrtf(dx * dx + dy * dy);
    double duration = time - touch_start_time;

    if (dist < TOUCH_SWIPE_MIN_DIST && duration < TOUCH_TAP_MAX_TIME) {
        /* Check dialog/deck/expanded map first */
        if (g_dialog.active || deck_view_active || dng_expanded_map) {
            handle_screen_tap(sx, sy);
            return;
        }

        /* Check minimap hit */
        {
            float fbx, fby;
            screen_to_fb(sx, sy, &fbx, &fby);
            sr_dungeon *md = &g_hub.dungeon;
            int mscale = 2;
            int mmx = FB_WIDTH - md->w * mscale - 4;
            int mmy = dng_minimap_y;
            int mmw = md->w * mscale;
            int mmh = md->h * mscale;
            if (fbx >= mmx && fbx <= mmx + mmw && fby >= mmy && fby <= mmy + mmh) {
                dng_expanded_map = true;
                return;
            }
        }

        /* Convert to FB coords to check button areas */
        float fbx, fby;
        screen_to_fb(sx, sy, &fbx, &fby);

        /* Check if tap is in a button zone (bottom bar, or the right-side
           column holding SECTOR/SAMPLES text and the DECK button above the
           minimap). The DECK button spans y=26..38. */
        bool in_button_zone = (fby >= FB_HEIGHT - 22) ||
                              (fbx >= FB_WIDTH - 74 && fby <= 40);

        /* Set click state for ui_button detection */
        handle_screen_tap(sx, sy);

        /* Tap strafe - only if not on a button and no dialog active.
           Central deadzone (middle TOUCH_STRAFE_DEADZONE of screen) is
           ignored to prevent accidental strafes. */
        if (!in_button_zone && !g_dialog.active) {
            float screen_w = sapp_widthf();
            float mid_x = screen_w * 0.5f;
            float half_dead = screen_w * TOUCH_STRAFE_DEADZONE * 0.5f;
            if (sx < mid_x - half_dead) {
                dng_player_try_move(&g_hub.player, &g_hub.dungeon,
                                    (g_hub.player.dir + 3) % 4);
            } else if (sx > mid_x + half_dead) {
                dng_player_try_move(&g_hub.player, &g_hub.dungeon,
                                    (g_hub.player.dir + 1) % 4);
            }
        }
    } else if (dist >= TOUCH_SWIPE_MIN_DIST && !g_dialog.active) {
        /* Swipe - move or turn (blocked during dialog) */
        float adx = dx < 0 ? -dx : dx;
        float ady = dy < 0 ? -dy : dy;

        if (ady > adx) {
            if (dy < 0) {
                dng_player_try_move(&g_hub.player, &g_hub.dungeon,
                                    g_hub.player.dir);
            } else {
                dng_player_try_move(&g_hub.player, &g_hub.dungeon,
                                    (g_hub.player.dir + 2) % 4);
            }
        } else {
            if (dx < 0) {
                g_hub.player.dir = (g_hub.player.dir + 3) % 4;
                g_hub.player.target_angle -= 0.25f;
            } else {
                g_hub.player.dir = (g_hub.player.dir + 1) % 4;
                g_hub.player.target_angle += 0.25f;
            }
        }
    }
}

#endif /* SR_MOBILE_INPUT_H */
