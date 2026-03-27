/*  Space Hulks — Dungeon Crawler Engine
 *  Entry point: Sokol app with dungeon scene.
 */

#if defined(__EMSCRIPTEN__)
    #define SOKOL_GLES3
    #include <emscripten.h>
#else
    #define SOKOL_GLCORE
#endif

#define SOKOL_IMPL
#include "../third_party/sokol/sokol_app.h"
#include "../third_party/sokol/sokol_gfx.h"
#include "../third_party/sokol/sokol_glue.h"
#include "../third_party/sokol/sokol_log.h"

#include "sr_math.h"
#include "sr_raster.h"
#include "sr_texture.h"
#include "sr_gif.h"
#include "sr_font.h"
#include "sr_dungeon.h"

#ifdef _WIN32
    #include <direct.h>
    #pragma comment(lib, "winmm.lib")
    #include <mmsystem.h>
#else
    #include <sys/stat.h>
    #ifndef __EMSCRIPTEN__
        #include <unistd.h>
    #endif
#endif

/* ── Module includes (order matters — header-only, single TU) ────── */

#include "sr_app.h"
#include "sr_lighting.h"
#include "sr_scene_dungeon.h"
#include "sr_menu.h"
#include "sr_mobile_input.h"

/* ── Shaders ─────────────────────────────────────────────────────── */

#if defined(SOKOL_GLCORE)

static const char *vs_src =
    "#version 330\n"
    "uniform vec2 u_scale;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos * u_scale, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *fs_src =
    "#version 330\n"
    "uniform sampler2D tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, v_uv);\n"
    "}\n";

#elif defined(SOKOL_GLES3)

static const char *vs_src =
    "#version 300 es\n"
    "uniform vec2 u_scale;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos * u_scale, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

static const char *fs_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = texture(tex, v_uv);\n"
    "}\n";

#endif

/* ── Sokol GPU resources ─────────────────────────────────────────── */

static sg_image    fb_image;
static sg_view     fb_view;
static sg_sampler  fb_sampler;
static sg_pipeline pip;
static sg_bindings bind;
static sg_buffer   vbuf;

/* ── Frame limiter ───────────────────────────────────────────────── */

#if defined(_WIN32)
static void frame_limiter(void) {
    static LARGE_INTEGER freq = {0}, last = {0};
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&last);
    }
    double target = 1.0 / (double)TARGET_FPS;
    for (;;) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        double elapsed = (double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        if (elapsed >= target) { last = now; return; }
        if (target - elapsed > 0.002) Sleep(1);
    }
}
#elif !defined(__EMSCRIPTEN__)
static void frame_limiter(void) {
    static struct timespec last = {0};
    if (last.tv_sec == 0 && last.tv_nsec == 0) clock_gettime(CLOCK_MONOTONIC, &last);
    double target = 1.0 / (double)TARGET_FPS;
    for (;;) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double elapsed = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) * 1e-9;
        if (elapsed >= target) { last = now; return; }
        usleep(500);
    }
}
#else
static void frame_limiter(void) { /* WASM: requestAnimationFrame handles this */ }
#endif

/* ── PNG screenshot ──────────────────────────────────────────────── */

#include "../third_party/stb/stb_image_write.h"

static void save_screenshot(const uint32_t *pixels, int w, int h) {
#ifdef _WIN32
    _mkdir("screenshots");
#else
    mkdir("screenshots", 0755);
#endif

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char filename[256];
    snprintf(filename, sizeof(filename),
             "screenshots/screenshot_%04d%02d%02d_%02d%02d%02d_%d.png",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec, screenshot_counter++);

    uint8_t *rgb = (uint8_t *)malloc(w * h * 3);
    if (!rgb) { fprintf(stderr, "[Screenshot] Out of memory\n"); return; }

    for (int i = 0; i < w * h; i++) {
        uint32_t c = pixels[i];
        rgb[i * 3 + 0] = (uint8_t)((c      ) & 0xFF);
        rgb[i * 3 + 1] = (uint8_t)((c >>  8) & 0xFF);
        rgb[i * 3 + 2] = (uint8_t)((c >> 16) & 0xFF);
    }

    if (!stbi_write_png(filename, w, h, 3, rgb, w * 3))
        fprintf(stderr, "[Screenshot] Failed to save %s\n", filename);
    else
        printf("[Screenshot] Saved %s\n", filename);

    free(rgb);
}

/* ── Sokol callbacks ─────────────────────────────────────────────── */

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });

    fb = sr_framebuffer_create(FB_WIDTH, FB_HEIGHT);
    shadow_fb = sr_framebuffer_create(SHADOW_SIZE, SHADOW_SIZE);

    fb_image = sg_make_image(&(sg_image_desc){
        .width  = FB_WIDTH,
        .height = FB_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .usage.stream_update = true,
    });

    fb_view = sg_make_view(&(sg_view_desc){
        .texture.image = fb_image,
    });

    sg_filter upscale_filter = SG_FILTER_NEAREST;
#if defined(__EMSCRIPTEN__)
    if (emscripten_run_script_int("window.matchMedia('(hover: none) and (pointer: coarse)').matches ? 1 : 0")) {
        upscale_filter = SG_FILTER_LINEAR;
    }
#endif
    fb_sampler = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = upscale_filter,
        .mag_filter = upscale_filter,
    });

    float verts[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
         1,  1,  1, 0,
        -1, -1,  0, 1,
         1,  1,  1, 0,
        -1,  1,  0, 0,
    };
    vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(verts),
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source   = vs_src,
        .fragment_func.source = fs_src,
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(float) * 2,
            .glsl_uniforms[0] = { .glsl_name = "u_scale", .type = SG_UNIFORMTYPE_FLOAT2 },
        },
        .views[0].texture = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .image_type = SG_IMAGETYPE_2D,
            .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
        },
        .samplers[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .sampler_type = SG_SAMPLERTYPE_FILTERING,
        },
        .texture_sampler_pairs[0] = {
            .stage = SG_SHADERSTAGE_FRAGMENT,
            .glsl_name = "tex",
            .view_slot = 0,
            .sampler_slot = 0,
        },
    });

    pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            },
        },
    });

    bind.vertex_buffers[0] = vbuf;
    bind.views[0]     = fb_view;
    bind.samplers[0]  = fb_sampler;

    textures[TEX_BRICK] = sr_texture_load("assets/bricks.png");
    textures[TEX_GRASS] = sr_texture_load("assets/grass.png");
    textures[TEX_ROOF]  = sr_texture_load("assets/roof.png");
    textures[TEX_WOOD]  = sr_texture_load("assets/wood.png");
    textures[TEX_TILE]  = sr_texture_load("assets/tile.png");

    itextures[ITEX_BRICK] = sr_indexed_load("assets/indexed/bricks.idx");
    itextures[ITEX_GRASS] = sr_indexed_load("assets/indexed/grass.idx");
    itextures[ITEX_ROOF]  = sr_indexed_load("assets/indexed/roof.idx");
    itextures[ITEX_WOOD]  = sr_indexed_load("assets/indexed/wood.idx");
    itextures[ITEX_TILE]  = sr_indexed_load("assets/indexed/tile.idx");
    itextures[ITEX_STONE] = sr_indexed_load("assets/indexed/stone.idx");

    sr_fog_set(FOG_COLOR, FOG_NEAR, FOG_FAR);

    dng_load_config();
    dng_game_init(&dng_state);
    dng_initialized = true;

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    printf("Space Hulks — Dungeon Crawler initialized (%dx%d @ %dfps)\n", FB_WIDTH, FB_HEIGHT, TARGET_FPS);
    printf("  WASD/Arrows = move/turn\n");
    printf("  F           = toggle lighting mode\n");
    printf("  I           = toggle info overlay\n");
    printf("  +/-         = adjust torch brightness\n");
    printf("  Ctrl+8      = start GIF recording (24fps)\n");
    printf("  Ctrl+9      = stop & save GIF\n");
    printf("  Ctrl+P      = save screenshot\n");
    printf("  ESC         = quit\n");
}

static void frame(void) {
    double dt = sapp_frame_duration();
    time_acc += dt;
    frame_counter++;

    fps_timer += dt;
    fps_frame_count++;
    if (fps_timer >= 1.0) {
        fps_display = fps_frame_count;
        fps_frame_count = 0;
        fps_timer -= 1.0;
    }

    sr_mat4 vp; /* unused — dungeon builds its own MVP */
    (void)vp;

    /* ── CPU rasterize ───────────────────────────────────────── */
    sr_stats_reset();
    sr_framebuffer_clear(&fb, 0xFF000000, 1.0f);
    sr_fog_disable();

    /* Update dungeon game state */
    if (dng_play_state == DNG_STATE_CLIMBING) {
        if (dng_update_climb(&dng_state)) {
            dng_play_state = DNG_STATE_PLAYING;
        }
    } else {
        sr_dungeon *dd = dng_state.dungeon;
        dng_player *pp = &dng_state.player;
        bool on_up = (dd->has_up && pp->gx == dd->stairs_gx && pp->gy == dd->stairs_gy);
        bool on_down = (dd->has_down && pp->gx == dd->down_gx && pp->gy == dd->down_gy);
        if (dng_state.on_stairs) {
            if (!on_up && !on_down) dng_state.on_stairs = false;
        } else {
            if (on_up) {
                dng_start_climb(&dng_state, true);
                dng_play_state = DNG_STATE_CLIMBING;
            } else if (on_down) {
                dng_start_climb(&dng_state, false);
                dng_play_state = DNG_STATE_CLIMBING;
            }
        }
    }
    dng_player_update(&dng_state.player);
    draw_dungeon_scene(&fb, &vp);
    draw_dungeon_minimap(&fb);
    if (dng_show_info) {
        static const char *dir_names[] = {"N","E","S","W"};
        char ibuf[64];
        dng_player *ip = &dng_state.player;
        snprintf(ibuf, sizeof(ibuf), "F%d  %s  (%d,%d)",
                 dng_state.current_floor + 1,
                 dir_names[ip->dir],
                 ip->gx, ip->gy);
        sr_draw_text_shadow(fb.color, fb.width, fb.height,
                            3, 3, ibuf, 0xFFFFFFFF, 0xFF000000);
    }

    int tris = sr_stats_tri_count();
    draw_stats(&fb, tris);

    /* ── GIF capture (time-based, 24fps) ─────────────────────── */
    if (sr_gif_is_recording()) {
        gif_capture_timer += dt;
        double interval = 1.0 / GIF_TARGET_FPS;
        while (gif_capture_timer >= interval) {
            gif_capture_timer -= interval;
            sr_gif_capture_frame(fb.color);
        }
    }

    /* ── Aspect-ratio-preserving scale ───────────────────────── */
    float fb_aspect  = (float)FB_WIDTH / (float)FB_HEIGHT;
    float win_aspect = sapp_widthf() / sapp_heightf();
    float scale[2];
    if (win_aspect > fb_aspect) {
        scale[0] = fb_aspect / win_aspect;
        scale[1] = 1.0f;
    } else {
        scale[0] = 1.0f;
        scale[1] = win_aspect / fb_aspect;
    }

    /* ── Upload and display ──────────────────────────────────── */
    sg_update_image(fb_image, &(sg_image_data){
        .mip_levels[0] = { .ptr = fb.color, .size = FB_WIDTH * FB_HEIGHT * 4 }
    });

    sg_begin_pass(&(sg_pass){
        .action = {
            .colors[0] = {
                .load_action = SG_LOADACTION_CLEAR,
                .clear_value = { 0.05f, 0.05f, 0.08f, 1.0f },
            },
        },
        .swapchain = sglue_swapchain(),
    });
    sg_apply_pipeline(pip);
    sg_apply_bindings(&bind);
    sg_apply_uniforms(0, &(sg_range){ &scale, sizeof(scale) });
    sg_draw(0, 6, 1);
    sg_end_pass();
    sg_commit();

    frame_limiter();
}

static void cleanup(void) {
    if (sr_gif_is_recording())
        sr_gif_stop_and_save();
    for (int i = 0; i < TEX_COUNT; i++)
        sr_texture_free(&textures[i]);
    for (int i = 0; i < ITEX_COUNT; i++)
        sr_indexed_free(&itextures[i]);
    sr_framebuffer_destroy(&fb);
    sr_framebuffer_destroy(&shadow_fb);
    sg_shutdown();
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

/* ── Event handler ───────────────────────────────────────────────── */

static void event(const sapp_event *ev) {
    double now_time = sapp_frame_count() * sapp_frame_duration();

    /* ── Touch / pointer for dungeon ────────────────────────── */
    if (ev->type == SAPP_EVENTTYPE_MOUSE_DOWN && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        dng_touch_began(ev->mouse_x, ev->mouse_y, now_time);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_MOVE) {
        dng_touch_moved(ev->mouse_x, ev->mouse_y);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_MOUSE_UP && ev->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
        dng_touch_ended(ev->mouse_x, ev->mouse_y, now_time);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_BEGAN && ev->num_touches > 0) {
        float sx = ev->touches[0].pos_x, sy = ev->touches[0].pos_y;
        dng_touch_began(sx, sy, now_time);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_MOVED && ev->num_touches > 0) {
        dng_touch_moved(ev->touches[0].pos_x, ev->touches[0].pos_y);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_ENDED && ev->num_touches > 0) {
        dng_touch_ended(ev->touches[0].pos_x, ev->touches[0].pos_y, now_time);
        return;
    }
    if (ev->type == SAPP_EVENTTYPE_TOUCHES_CANCELLED) {
        dng_touch_cancelled();
        return;
    }

    if (ev->type != SAPP_EVENTTYPE_KEY_DOWN) return;

    /* ── Global keys ─────────────────────────────────────────── */
    if (ev->key_code == SAPP_KEYCODE_8 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        if (!sr_gif_is_recording()) {
            gif_capture_timer = 0.0;
            sr_gif_start_recording(FB_WIDTH, FB_HEIGHT);
        }
        return;
    }
    if (ev->key_code == SAPP_KEYCODE_9 && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        if (sr_gif_is_recording()) sr_gif_stop_and_save();
        return;
    }
    if (ev->key_code == SAPP_KEYCODE_P && (ev->modifiers & SAPP_MODIFIER_CTRL)) {
        save_screenshot(fb.color, FB_WIDTH, FB_HEIGHT);
        return;
    }

    /* ── Dungeon keys ────────────────────────────────────────── */
    switch (ev->key_code) {
        case SAPP_KEYCODE_F:
            dng_light_mode = (dng_light_mode + 1) % 2;
            break;
        case SAPP_KEYCODE_I:
            dng_show_info = !dng_show_info;
            break;
        case SAPP_KEYCODE_EQUAL:
        case SAPP_KEYCODE_KP_ADD:
            dng_cfg.light_brightness += 0.1f;
            if (dng_cfg.light_brightness > 5.0f) dng_cfg.light_brightness = 5.0f;
            break;
        case SAPP_KEYCODE_MINUS:
        case SAPP_KEYCODE_KP_SUBTRACT:
            dng_cfg.light_brightness -= 0.1f;
            if (dng_cfg.light_brightness < 0.0f) dng_cfg.light_brightness = 0.0f;
            break;
        /* ── Movement ────────────────────────────────────────── */
        case SAPP_KEYCODE_W:
        case SAPP_KEYCODE_UP:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    dng_state.player.dir);
            }
            break;
        case SAPP_KEYCODE_S:
        case SAPP_KEYCODE_DOWN:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 2) % 4);
            }
            break;
        case SAPP_KEYCODE_A:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 3) % 4);
            }
            break;
        case SAPP_KEYCODE_D:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_player_try_move(&dng_state.player, dng_state.dungeon,
                                    (dng_state.player.dir + 1) % 4);
            }
            break;
        case SAPP_KEYCODE_LEFT:
        case SAPP_KEYCODE_Q:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 3) % 4;
                dng_state.player.target_angle -= 0.25f;
            }
            break;
        case SAPP_KEYCODE_RIGHT:
        case SAPP_KEYCODE_E:
            if (dng_play_state == DNG_STATE_PLAYING) {
                dng_state.player.dir = (dng_state.player.dir + 1) % 4;
                dng_state.player.target_angle += 0.25f;
            }
            break;
        default: break;
    }
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb    = init,
        .frame_cb   = frame,
        .cleanup_cb = cleanup,
        .event_cb   = event,
        .width      = FB_WIDTH * 2,
        .height     = FB_HEIGHT * 2,
        .window_title = "Space Hulks",
        .high_dpi     = true,
        .logger.func  = slog_func,
        .swap_interval = 0,
    };
}
