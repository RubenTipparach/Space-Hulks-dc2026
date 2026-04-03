/*  sr_dialog_data.h — Loads dialog text from config/dialog.yaml
 *  Header-only, single TU. Uses sr_config.h for YAML parsing.
 */
#ifndef SR_DIALOG_DATA_H
#define SR_DIALOG_DATA_H

#include "sr_config.h"

/* ── Limits ─────────────────────────────────────────────────────── */

#define DLGD_MAX_INTRO_LINES  32
#define DLGD_MAX_DIALOG_LINES  4
#define DLGD_MAX_PAGES         4
#define DLGD_MAX_OBJECTIVES    4
#define DLGD_LINE_LEN         48

/* ── Data structures ────────────────────────────────────────────── */

typedef struct {
    char lines[DLGD_MAX_DIALOG_LINES][DLGD_LINE_LEN];
    int  count;
} dlgd_block;

typedef struct {
    /* Intro teletype */
    char intro_lines[DLGD_MAX_INTRO_LINES][DLGD_LINE_LEN];
    int  intro_count;

    /* Crew initial dialogs (before captain briefing) */
    dlgd_block crew_init[5];  /* 0=captain(unused), 1=reyes, 2=chen, 3=kowalski, 4=vasquez */

    /* Captain briefing pages */
    dlgd_block captain_briefing[DLGD_MAX_PAGES];
    int        captain_briefing_pages;

    /* Post-briefing crew dialogs */
    dlgd_block post_vasquez;
    dlgd_block post_chen;
    dlgd_block post_reyes_ready;
    dlgd_block post_reyes_blocked;

    /* Captain state dialogs */
    dlgd_block captain_pre_mission;
    dlgd_block captain_post_mission;
    dlgd_block captain_under_attack;
    dlgd_block captain_neutralized;

    /* Default crew dialogs (after initial flow done) */
    dlgd_block crew_default[5]; /* same indexing as crew_init */

    /* Mission objectives */
    char objectives[DLGD_MAX_OBJECTIVES][DLGD_LINE_LEN];
    int  objective_count;

    bool loaded;
} dlgd_data;

static dlgd_data g_dlgd;

/* ── Helpers ────────────────────────────────────────────────────── */

static void dlgd_load_block(const sr_config *cfg, const char *prefix, dlgd_block *blk) {
    char key[128];
    snprintf(key, sizeof(key), "%s.count", prefix);
    blk->count = (int)sr_config_float(cfg, key, 0);
    if (blk->count > DLGD_MAX_DIALOG_LINES) blk->count = DLGD_MAX_DIALOG_LINES;
    for (int i = 0; i < blk->count; i++) {
        snprintf(key, sizeof(key), "%s.line%d", prefix, i);
        const char *v = sr_config_get(cfg, key);
        if (v) {
            strncpy(blk->lines[i], v, DLGD_LINE_LEN - 1);
            blk->lines[i][DLGD_LINE_LEN - 1] = '\0';
        }
    }
}

/* ── Main loader ────────────────────────────────────────────────── */

static void dlgd_load(void) {
    /* Use a larger config for dialog file (more entries than default 64) */
    sr_config cfg = sr_config_load("config/dialog.yaml");
    if (cfg.count == 0) {
        fprintf(stderr, "[dlgd] Failed to load config/dialog.yaml\n");
        return;
    }

    memset(&g_dlgd, 0, sizeof(g_dlgd));

    /* Intro lines */
    g_dlgd.intro_count = (int)sr_config_float(&cfg, "intro.count", 0);
    if (g_dlgd.intro_count > DLGD_MAX_INTRO_LINES) g_dlgd.intro_count = DLGD_MAX_INTRO_LINES;
    for (int i = 0; i < g_dlgd.intro_count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "intro.line%d", i);
        const char *v = sr_config_get(&cfg, key);
        if (v) {
            strncpy(g_dlgd.intro_lines[i], v, DLGD_LINE_LEN - 1);
            g_dlgd.intro_lines[i][DLGD_LINE_LEN - 1] = '\0';
        }
    }

    /* Crew initial dialogs */
    dlgd_load_block(&cfg, "crew_init.kowalski", &g_dlgd.crew_init[3]);
    dlgd_load_block(&cfg, "crew_init.reyes",    &g_dlgd.crew_init[1]);
    dlgd_load_block(&cfg, "crew_init.chen",      &g_dlgd.crew_init[2]);
    dlgd_load_block(&cfg, "crew_init.vasquez",   &g_dlgd.crew_init[4]);

    /* Captain briefing */
    g_dlgd.captain_briefing_pages = (int)sr_config_float(&cfg, "captain_briefing.pages", 0);
    if (g_dlgd.captain_briefing_pages > DLGD_MAX_PAGES) g_dlgd.captain_briefing_pages = DLGD_MAX_PAGES;
    for (int p = 0; p < g_dlgd.captain_briefing_pages; p++) {
        char prefix[64];
        snprintf(prefix, sizeof(prefix), "captain_briefing.page%d", p);
        dlgd_load_block(&cfg, prefix, &g_dlgd.captain_briefing[p]);
    }

    /* Post-briefing crew */
    dlgd_load_block(&cfg, "crew_post_briefing.vasquez",       &g_dlgd.post_vasquez);
    dlgd_load_block(&cfg, "crew_post_briefing.chen",          &g_dlgd.post_chen);
    dlgd_load_block(&cfg, "crew_post_briefing.reyes_ready",   &g_dlgd.post_reyes_ready);
    dlgd_load_block(&cfg, "crew_post_briefing.reyes_blocked", &g_dlgd.post_reyes_blocked);

    /* Captain state dialogs */
    dlgd_load_block(&cfg, "captain.pre_mission",   &g_dlgd.captain_pre_mission);
    dlgd_load_block(&cfg, "captain.post_mission",  &g_dlgd.captain_post_mission);
    dlgd_load_block(&cfg, "captain.under_attack",  &g_dlgd.captain_under_attack);
    dlgd_load_block(&cfg, "captain.neutralized",   &g_dlgd.captain_neutralized);

    /* Default crew dialogs */
    dlgd_load_block(&cfg, "crew_default.reyes",    &g_dlgd.crew_default[1]);
    dlgd_load_block(&cfg, "crew_default.chen",      &g_dlgd.crew_default[2]);
    dlgd_load_block(&cfg, "crew_default.kowalski",  &g_dlgd.crew_default[3]);
    dlgd_load_block(&cfg, "crew_default.vasquez",   &g_dlgd.crew_default[4]);

    /* Mission objectives */
    g_dlgd.objective_count = (int)sr_config_float(&cfg, "objectives.count", 0);
    if (g_dlgd.objective_count > DLGD_MAX_OBJECTIVES) g_dlgd.objective_count = DLGD_MAX_OBJECTIVES;
    for (int i = 0; i < g_dlgd.objective_count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "objectives.obj%d", i);
        const char *v = sr_config_get(&cfg, key);
        if (v) {
            strncpy(g_dlgd.objectives[i], v, DLGD_LINE_LEN - 1);
            g_dlgd.objectives[i][DLGD_LINE_LEN - 1] = '\0';
        }
    }

    g_dlgd.loaded = true;
    sr_config_free(&cfg);
    printf("[dlgd] Loaded dialog data: %d intro lines, %d briefing pages, %d objectives\n",
           g_dlgd.intro_count, g_dlgd.captain_briefing_pages, g_dlgd.objective_count);
}

#endif /* SR_DIALOG_DATA_H */
