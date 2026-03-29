/*  sr_mobile_input.h — Touch / swipe input for mobile.
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

        /* Check if tap is on the minimap area — open expanded map */
        {
            float fbx, fby;
            screen_to_fb(sx, sy, &fbx, &fby);
            sr_dungeon *md = dng_state.dungeon;
            int mscale = 2;
            int mmx = FB_WIDTH - md->w * mscale - 4;
            int mmy = 4;
            int mmw = md->w * mscale;
            int mmh = md->h * mscale;
            if (fbx >= mmx && fbx <= mmx + mmw && fby >= mmy && fby <= mmy + mmh) {
                dng_expanded_map = true;
                return;
            }
        }

        /* Short tap — strafe based on screen half */
        float mid_x = sapp_widthf() * 0.5f;
        if (sx < mid_x) {
            dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                (dng_state.player.dir + 3) % 4);
        } else {
            dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                (dng_state.player.dir + 1) % 4);
        }
    } else if (dist >= TOUCH_SWIPE_MIN_DIST) {
        /* Swipe — determine cardinal direction */
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

#endif /* SR_MOBILE_INPUT_H */
