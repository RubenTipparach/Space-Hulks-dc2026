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
#define DLGD_LINE_LEN         72

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

    /* Kowalski progressive dialog — gets more stressed */
    dlgd_block kowalski_stress[4]; /* 0=early, 1=mid, 2=late, 3=pre-boss freakout */

    /* By-Tor progressive dialog — hopeful alien friend */
    dlgd_block bytor_init;
    dlgd_block bytor_default[3]; /* 0=early, 1=mid, 2=late */
    dlgd_block bytor_pre_boss;

    /* Mission objectives */
    char objectives[DLGD_MAX_OBJECTIVES][DLGD_LINE_LEN];
    int  objective_count;

    /* Epilogues */
    char epilogue_loss[DLGD_MAX_INTRO_LINES][DLGD_LINE_LEN];
    int  epilogue_loss_count;
    char epilogue_win[DLGD_MAX_INTRO_LINES][DLGD_LINE_LEN];
    int  epilogue_win_count;

    /* Captain sample dialogs (between star maps) */
    dlgd_block captain_sample[3]; /* after boss 1, 2, 3 */

    /* Card text (loaded from config/cards.yaml) */
    #define DLGD_CARD_TEXT_LEN 64
    #define DLGD_MAX_CARDS 32
    char card_effect[DLGD_MAX_CARDS][DLGD_CARD_TEXT_LEN];   /* short effect text */
    char card_desc[DLGD_MAX_CARDS][DLGD_CARD_TEXT_LEN];     /* detail description */
    bool cards_loaded;

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

/* Forward declarations */
static void dlgd_load_cards(void);

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

    /* Kowalski progressive stress dialog */
    dlgd_load_block(&cfg, "kowalski.stress0", &g_dlgd.kowalski_stress[0]);
    dlgd_load_block(&cfg, "kowalski.stress1", &g_dlgd.kowalski_stress[1]);
    dlgd_load_block(&cfg, "kowalski.stress2", &g_dlgd.kowalski_stress[2]);
    dlgd_load_block(&cfg, "kowalski.stress3", &g_dlgd.kowalski_stress[3]);

    /* By-Tor dialog */
    dlgd_load_block(&cfg, "bytor.init",      &g_dlgd.bytor_init);
    dlgd_load_block(&cfg, "bytor.default0",  &g_dlgd.bytor_default[0]);
    dlgd_load_block(&cfg, "bytor.default1",  &g_dlgd.bytor_default[1]);
    dlgd_load_block(&cfg, "bytor.default2",  &g_dlgd.bytor_default[2]);
    dlgd_load_block(&cfg, "bytor.pre_boss",  &g_dlgd.bytor_pre_boss);

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

    /* Epilogue: loss */
    g_dlgd.epilogue_loss_count = (int)sr_config_float(&cfg, "epilogue_loss.count", 0);
    if (g_dlgd.epilogue_loss_count > DLGD_MAX_INTRO_LINES) g_dlgd.epilogue_loss_count = DLGD_MAX_INTRO_LINES;
    for (int i = 0; i < g_dlgd.epilogue_loss_count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "epilogue_loss.line%d", i);
        const char *v = sr_config_get(&cfg, key);
        if (v) { strncpy(g_dlgd.epilogue_loss[i], v, DLGD_LINE_LEN - 1); g_dlgd.epilogue_loss[i][DLGD_LINE_LEN - 1] = '\0'; }
    }

    /* Epilogue: win */
    g_dlgd.epilogue_win_count = (int)sr_config_float(&cfg, "epilogue_win.count", 0);
    if (g_dlgd.epilogue_win_count > DLGD_MAX_INTRO_LINES) g_dlgd.epilogue_win_count = DLGD_MAX_INTRO_LINES;
    for (int i = 0; i < g_dlgd.epilogue_win_count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "epilogue_win.line%d", i);
        const char *v = sr_config_get(&cfg, key);
        if (v) { strncpy(g_dlgd.epilogue_win[i], v, DLGD_LINE_LEN - 1); g_dlgd.epilogue_win[i][DLGD_LINE_LEN - 1] = '\0'; }
    }

    /* Captain sample dialogs */
    dlgd_load_block(&cfg, "captain_sample.sample1", &g_dlgd.captain_sample[0]);
    dlgd_load_block(&cfg, "captain_sample.sample2", &g_dlgd.captain_sample[1]);
    dlgd_load_block(&cfg, "captain_sample.sample3", &g_dlgd.captain_sample[2]);

    g_dlgd.loaded = true;
    sr_config_free(&cfg);
    printf("[dlgd] Loaded dialog data: %d intro, %d loss, %d win lines\n",
           g_dlgd.intro_count, g_dlgd.epilogue_loss_count, g_dlgd.epilogue_win_count);

    /* Load card text from cards.yaml */
    dlgd_load_cards();
}

/* Card key names mapping card type index → YAML key prefix */
static const char *dlgd_card_keys[] = {
    "shield", "shoot", "burst", "move", "melee",
    "overcharge", "repair", "stun", "fortify", "double_shot", "dash",
    "ice", "acid", "fire", "lightning",
    "sniper", "shotgun", "welder", "chainsaw", "laser", "deflector", "stun_gun", "microwave",
    "quickstep",
    "sniper_up", "shotgun_up", "welder_up", "chainsaw_up", "laser_up", "deflector_up", "stun_gun_up", "microwave_up"
};

/* Convert escaped \n sequences in YAML strings to actual newlines */
static void dlgd_unescape_newlines(char *str) {
    char *r = str, *w = str;
    while (*r) {
        if (r[0] == '\\' && r[1] == 'n') {
            *w++ = '\n';
            r += 2;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

static void dlgd_load_cards(void) {
    sr_config cfg = sr_config_load("config/cards.yaml");
    if (cfg.count == 0) {
        fprintf(stderr, "[dlgd] Failed to load config/cards.yaml\n");
        return;
    }

    int num_keys = (int)(sizeof(dlgd_card_keys) / sizeof(dlgd_card_keys[0]));
    if (num_keys > DLGD_MAX_CARDS) num_keys = DLGD_MAX_CARDS;

    for (int i = 0; i < num_keys; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%s.effect", dlgd_card_keys[i]);
        const char *v = sr_config_get(&cfg, key);
        if (v) {
            strncpy(g_dlgd.card_effect[i], v, DLGD_CARD_TEXT_LEN - 1);
            g_dlgd.card_effect[i][DLGD_CARD_TEXT_LEN - 1] = '\0';
            dlgd_unescape_newlines(g_dlgd.card_effect[i]);
        }

        snprintf(key, sizeof(key), "%s.description", dlgd_card_keys[i]);
        v = sr_config_get(&cfg, key);
        if (v) {
            strncpy(g_dlgd.card_desc[i], v, DLGD_CARD_TEXT_LEN - 1);
            g_dlgd.card_desc[i][DLGD_CARD_TEXT_LEN - 1] = '\0';
            dlgd_unescape_newlines(g_dlgd.card_desc[i]);
        }
    }

    /* Wire up the pointer arrays used by sr_combat.h */
    for (int i = 0; i < num_keys; i++) {
        if (g_dlgd.card_effect[i][0] != '\0')
            card_yaml_effect[i] = g_dlgd.card_effect[i];
        if (g_dlgd.card_desc[i][0] != '\0')
            card_yaml_desc[i] = g_dlgd.card_desc[i];
    }

    g_dlgd.cards_loaded = true;
    sr_config_free(&cfg);
    printf("[dlgd] Loaded %d card text entries\n", num_keys);
}

#endif /* SR_DIALOG_DATA_H */
