/*  sr_audio.h — Audio system (header-only, single TU)
 *
 *  Uses sokol_audio for output, minimp3 for MP3 decoding.
 *  Provides: footstep SFX, hub ambient drone + random noise.
 */

#ifndef SR_AUDIO_H
#define SR_AUDIO_H

#define MINIMP3_IMPLEMENTATION
#include "../third_party/minimp3/minimp3_ex.h"
#include "../third_party/sokol/sokol_audio.h"

/* ── Data structures ─────────────────────────────────────────────── */

typedef struct {
    float *samples;      /* mono PCM */
    int    num_samples;
    int    sample_rate;
} sr_audio_clip;

#define SR_AUDIO_MAX_VOICES 12

typedef struct {
    const sr_audio_clip *clip;
    int    position;
    float  volume;
    bool   looping;
    bool   active;
} sr_audio_voice;

typedef struct {
    float ambient;
    float footstep;
} sr_audio_volumes;

/* ── Globals ─────────────────────────────────────────────────────── */

static sr_audio_volumes audio_vol = { .ambient = 0.7f, .footstep = 0.4f };
static sr_audio_voice   audio_voices[SR_AUDIO_MAX_VOICES];
static bool             audio_initialized = false;

static sr_audio_clip audio_footsteps[6];
static sr_audio_clip audio_drone;
static sr_audio_clip audio_noise;

/* Card / action SFX */
static sr_audio_clip audio_sfx_teleporter;
static sr_audio_clip audio_sfx_shoot;
static sr_audio_clip audio_sfx_ice;
static sr_audio_clip audio_sfx_acid;
static sr_audio_clip audio_sfx_chainsaw;
static sr_audio_clip audio_sfx_dblshot;
static sr_audio_clip audio_sfx_dealcard;
static sr_audio_clip audio_sfx_welder;
static sr_audio_clip audio_sfx_burst;
static sr_audio_clip audio_sfx_confirm;
static sr_audio_clip audio_sfx_dash;
static sr_audio_clip audio_sfx_deflector;
static sr_audio_clip audio_sfx_error;
static sr_audio_clip audio_sfx_fortify;
static sr_audio_clip audio_sfx_laser;
static sr_audio_clip audio_sfx_microwave;
static sr_audio_clip audio_sfx_stun_gun;
static sr_audio_clip audio_sfx_victory;
static sr_audio_clip audio_sfx_fire;
static sr_audio_clip audio_sfx_lightning;
static sr_audio_clip audio_sfx_melee;
static sr_audio_clip audio_sfx_shieldimpact;
static sr_audio_clip audio_sfx_shotgun;

/* Enemy ship music */
static sr_audio_clip audio_enemyship_music;
static int   audio_enemyship_voice = -1;

static int   audio_drone_voice = -1;
static int   audio_noise_voice = -1;
static float audio_noise_timer = 0.0f;
static bool  audio_hub_ambient_active = false;

/* ── MP3 loading ─────────────────────────────────────────────────── */

static sr_audio_clip sr_audio_load_mp3(const char *path) {
    sr_audio_clip clip = {0};
    mp3dec_t mp3d;
    mp3dec_file_info_t info;
    if (mp3dec_load(&mp3d, path, &info, NULL, NULL)) {
        fprintf(stderr, "[Audio] Failed to load: %s\n", path);
        return clip;
    }
    int num_samples = (int)(info.samples / info.channels);
    clip.samples = (float *)malloc(num_samples * sizeof(float));
    clip.num_samples = num_samples;
    clip.sample_rate = info.hz;
    for (int i = 0; i < num_samples; i++) {
        float sum = 0;
        for (int ch = 0; ch < info.channels; ch++)
            sum += info.buffer[i * info.channels + ch] / 32768.0f;
        clip.samples[i] = sum / info.channels;
    }
    free(info.buffer);
    if (clip.sample_rate != 44100)
        printf("[Audio] Warning: %s is %d Hz (expected 44100)\n", path, clip.sample_rate);
    return clip;
}

static void sr_audio_clip_free(sr_audio_clip *clip) {
    free(clip->samples);
    clip->samples = NULL;
    clip->num_samples = 0;
}

/* ── Voice management ────────────────────────────────────────────── */

static int sr_audio_play(const sr_audio_clip *clip, float volume, bool loop) {
    if (!clip || !clip->samples) return -1;
    /* Apply master volume to all sounds */
    volume *= settings_master_vol;
    for (int i = 0; i < SR_AUDIO_MAX_VOICES; i++) {
        if (!audio_voices[i].active) {
            audio_voices[i].clip     = clip;
            audio_voices[i].position = 0;
            audio_voices[i].volume   = volume;
            audio_voices[i].looping  = loop;
            audio_voices[i].active   = true;
            return i;
        }
    }
    return -1;
}

static void sr_audio_stop(int voice_idx) {
    if (voice_idx >= 0 && voice_idx < SR_AUDIO_MAX_VOICES)
        audio_voices[voice_idx].active = false;
}

/* ── Stream callback (audio thread) ──────────────────────────────── */

static void sr_audio_callback(float *buffer, int num_frames, int num_channels) {
    memset(buffer, 0, (size_t)(num_frames * num_channels) * sizeof(float));
    for (int v = 0; v < SR_AUDIO_MAX_VOICES; v++) {
        if (!audio_voices[v].active) continue;
        sr_audio_voice *voice = &audio_voices[v];
        const sr_audio_clip *clip = voice->clip;
        if (!clip || !clip->samples) { voice->active = false; continue; }
        for (int i = 0; i < num_frames; i++) {
            if (voice->position >= clip->num_samples) {
                if (voice->looping) {
                    voice->position = 0;
                } else {
                    voice->active = false;
                    break;
                }
            }
            float sample = clip->samples[voice->position] * voice->volume;
            for (int ch = 0; ch < num_channels; ch++)
                buffer[i * num_channels + ch] += sample;
            voice->position++;
        }
    }
    /* Clamp */
    int total = num_frames * num_channels;
    for (int i = 0; i < total; i++) {
        if (buffer[i] > 1.0f) buffer[i] = 1.0f;
        else if (buffer[i] < -1.0f) buffer[i] = -1.0f;
    }
}

/* ── High-level API ──────────────────────────────────────────────── */

static void sr_audio_play_footstep(void) {
    if (!audio_initialized) return;
    int idx = (int)(rng_float() * 6.0f);
    if (idx > 5) idx = 5;
    sr_audio_play(&audio_footsteps[idx], audio_vol.footstep, false);
}

static void sr_audio_play_sfx(const sr_audio_clip *clip) {
    if (!audio_initialized || !clip || !clip->samples) return;
    sr_audio_play(clip, 0.5f, false);
}

static void sr_audio_start_hub_ambient(void) {
    if (!audio_initialized || audio_hub_ambient_active) return;
    audio_drone_voice = sr_audio_play(&audio_drone, audio_vol.ambient, true);
    audio_noise_timer = rng_range(15.0f, 45.0f);
    audio_hub_ambient_active = true;
}

static void sr_audio_stop_hub_ambient(void) {
    if (!audio_hub_ambient_active) return;
    sr_audio_stop(audio_drone_voice);
    sr_audio_stop(audio_noise_voice);
    audio_drone_voice = -1;
    audio_noise_voice = -1;
    audio_hub_ambient_active = false;
}

static void sr_audio_start_enemyship_music(void) {
    if (!audio_initialized) return;
    if (audio_enemyship_voice >= 0 && audio_voices[audio_enemyship_voice].active) return;
    audio_enemyship_voice = sr_audio_play(&audio_enemyship_music, 0.4f, true);
}

static void sr_audio_stop_enemyship_music(void) {
    if (audio_enemyship_voice >= 0) {
        sr_audio_stop(audio_enemyship_voice);
        audio_enemyship_voice = -1;
    }
}

static void sr_audio_play_combat_step(void) {
    if (!audio_initialized) return;
    int idx = (int)(rng_float() * 6.0f);
    if (idx > 5) idx = 5;
    sr_audio_play(&audio_footsteps[idx], 0.25f, false);
}

static void sr_audio_update(float dt) {
    if (!audio_initialized || !audio_hub_ambient_active) return;
    audio_noise_timer -= dt;
    if (audio_noise_timer <= 0.0f) {
        /* Only play if previous noise finished (non-overlapping) */
        if (audio_noise_voice < 0 || !audio_voices[audio_noise_voice].active) {
            audio_noise_voice = sr_audio_play(&audio_noise, audio_vol.ambient, false);
        }
        audio_noise_timer = rng_range(30.0f, 90.0f);
    }
}

/* ── Init / Shutdown ─────────────────────────────────────────────── */

static void sr_audio_init(void) {
    saudio_setup(&(saudio_desc){
        .sample_rate = 44100,
        .num_channels = 1,
        .stream_cb = sr_audio_callback,
        .logger.func = slog_func,
    });

    audio_footsteps[0] = sr_audio_load_mp3("assets/audio/metal_step_1.mp3");
    audio_footsteps[1] = sr_audio_load_mp3("assets/audio/metal_step_2.mp3");
    audio_footsteps[2] = sr_audio_load_mp3("assets/audio/metal_step_3.mp3");
    audio_footsteps[3] = sr_audio_load_mp3("assets/audio/metal_step_4.mp3");
    audio_footsteps[4] = sr_audio_load_mp3("assets/audio/metal_step_5.mp3");
    audio_footsteps[5] = sr_audio_load_mp3("assets/audio/metal_step_6.mp3");
    audio_drone = sr_audio_load_mp3("assets/audio/homeship_drone.mp3");
    audio_noise = sr_audio_load_mp3("assets/audio/homeship_noise.mp3");

    /* Card / action SFX */
    audio_sfx_teleporter = sr_audio_load_mp3("assets/audio/teleporter.mp3");
    audio_sfx_shoot      = sr_audio_load_mp3("assets/audio/shoot.mp3");
    audio_sfx_ice        = sr_audio_load_mp3("assets/audio/ice.mp3");
    audio_sfx_acid       = sr_audio_load_mp3("assets/audio/acid.mp3");
    audio_sfx_chainsaw   = sr_audio_load_mp3("assets/audio/chainsaw.mp3");
    audio_sfx_dblshot    = sr_audio_load_mp3("assets/audio/dblshot.mp3");
    audio_sfx_dealcard   = sr_audio_load_mp3("assets/audio/dealcard.mp3");
    audio_sfx_welder     = sr_audio_load_mp3("assets/audio/welder.mp3");
    audio_sfx_burst      = sr_audio_load_mp3("assets/audio/burst.mp3");
    audio_sfx_confirm    = sr_audio_load_mp3("assets/audio/confirm.mp3");
    audio_sfx_dash       = sr_audio_load_mp3("assets/audio/dash.mp3");
    audio_sfx_deflector  = sr_audio_load_mp3("assets/audio/deflector.mp3");
    audio_sfx_error      = sr_audio_load_mp3("assets/audio/error2.mp3");
    audio_sfx_fortify    = sr_audio_load_mp3("assets/audio/fortify.mp3");
    audio_sfx_laser      = sr_audio_load_mp3("assets/audio/laser.mp3");
    audio_sfx_microwave  = sr_audio_load_mp3("assets/audio/microwave.mp3");
    audio_sfx_stun_gun   = sr_audio_load_mp3("assets/audio/stun gun.mp3");
    audio_sfx_victory    = sr_audio_load_mp3("assets/audio/victory.mp3");
    audio_sfx_fire       = sr_audio_load_mp3("assets/audio/fire.mp3");
    audio_sfx_lightning  = sr_audio_load_mp3("assets/audio/lightning.mp3");
    audio_sfx_melee      = sr_audio_load_mp3("assets/audio/melee.mp3");
    audio_sfx_shieldimpact = sr_audio_load_mp3("assets/audio/shieldimpact.mp3");
    audio_sfx_shotgun    = sr_audio_load_mp3("assets/audio/shotgun.mp3");
    audio_enemyship_music= sr_audio_load_mp3("assets/audio/enemyship_full.mp3");

    dng_on_move_callback = sr_audio_play_footstep;
    audio_initialized = true;
    printf("[Audio] Initialized (%d Hz, %d ch)\n", saudio_sample_rate(), saudio_channels());
}

static void sr_audio_shutdown(void) {
    if (!audio_initialized) return;
    dng_on_move_callback = NULL;
    audio_initialized = false;
    saudio_shutdown();
    for (int i = 0; i < 6; i++) sr_audio_clip_free(&audio_footsteps[i]);
    sr_audio_clip_free(&audio_drone);
    sr_audio_clip_free(&audio_noise);
    sr_audio_clip_free(&audio_sfx_teleporter);
    sr_audio_clip_free(&audio_sfx_shoot);
    sr_audio_clip_free(&audio_sfx_ice);
    sr_audio_clip_free(&audio_sfx_acid);
    sr_audio_clip_free(&audio_sfx_chainsaw);
    sr_audio_clip_free(&audio_sfx_dblshot);
    sr_audio_clip_free(&audio_sfx_dealcard);
    sr_audio_clip_free(&audio_sfx_welder);
    sr_audio_clip_free(&audio_sfx_burst);
    sr_audio_clip_free(&audio_sfx_confirm);
    sr_audio_clip_free(&audio_sfx_dash);
    sr_audio_clip_free(&audio_sfx_deflector);
    sr_audio_clip_free(&audio_sfx_error);
    sr_audio_clip_free(&audio_sfx_fortify);
    sr_audio_clip_free(&audio_sfx_laser);
    sr_audio_clip_free(&audio_sfx_microwave);
    sr_audio_clip_free(&audio_sfx_stun_gun);
    sr_audio_clip_free(&audio_sfx_victory);
    sr_audio_clip_free(&audio_sfx_fire);
    sr_audio_clip_free(&audio_sfx_lightning);
    sr_audio_clip_free(&audio_sfx_melee);
    sr_audio_clip_free(&audio_sfx_shieldimpact);
    sr_audio_clip_free(&audio_sfx_shotgun);
    sr_audio_clip_free(&audio_enemyship_music);
}

#endif /* SR_AUDIO_H */
