/*  sr_combat.h — Card-based combat system for Space Hulks.
 *  Single-TU header-only. Depends on sr_app.h, sr_font.h, sr_sprites.h. */
#ifndef SR_COMBAT_H
#define SR_COMBAT_H

#include <stdarg.h>
#include "sr_sprites.h"

/* ── Card types ──────────────────────────────────────────────────── */

enum {
    CARD_SHIELD,     /* +3 shield, cost 1 */
    CARD_SHOOT,      /* 3 dmg single, cost 1 */
    CARD_BURST,      /* 2 dmg all, cost 2 */
    CARD_MOVE,       /* advance 1, cost 1 */
    CARD_MELEE,      /* 6 dmg melee, cost 1 */
    /* Droppable cards */
    CARD_OVERCHARGE, /* +2 energy this turn, cost 0 */
    CARD_REPAIR,     /* heal 4 HP, cost 2 */
    CARD_STUN,       /* skip 1 enemy attack, cost 1 */
    CARD_FORTIFY,    /* +6 shield, cost 2 */
    CARD_DOUBLE_SHOT,/* 5 dmg single, cost 2 */
    CARD_DASH,       /* advance 2 + melee 4, cost 2 */
    /* Elemental cards */
    CARD_ICE,        /* ice: freeze target 3 turns, 1 dmg, cost 1 */
    CARD_ACID,       /* acid: stackable DoT, 1 dmg, cost 1 */
    CARD_FIRE,       /* fire: DoT + spreads, 1 dmg, cost 1 */
    CARD_LIGHTNING,  /* lightning: stun 1-2 turns, 2 dmg, cost 2 */
    /* Class-specific cards */
    CARD_SNIPER,     /* sniper: 5 dmg single, requires dist>=2, cost 1 */
    CARD_SHOTGUN,    /* shotgun: 4 dmg single, requires dist<=1, cost 1 */
    CARD_WELDER,     /* welder: 4 dmg melee, cost 1 */
    CARD_CHAINSAW,   /* chainsaw: 8 dmg melee, cost 2 */
    CARD_LASER,      /* laser: 4 dmg precision single, cost 1 */
    CARD_DEFLECTOR,  /* deflector: +4 shield, reflects enemy damage back, cost 1 */
    CARD_STUN_GUN,   /* stun gun: stun 1 enemy 1 turn, 1 dmg, cost 1 */
    CARD_MICROWAVE,  /* microwave: 5 dmg, if kill -> 3 dmg all, cost 2 */
    CARD_QUICKSTEP,  /* quickstep: add 1 MOVE card to discard, cost 1 */
    CARD_TYPE_COUNT
};

static const char *card_names[] = {
    "SHIELD", "SHOOT", "BURST", "MOVE", "MELEE",
    "OVERCHRG", "REPAIR", "STUN", "FORTIFY", "DBL SHOT", "DASH",
    "ICE", "ACID", "FIRE", "LIGHTNING",
    "SNIPER", "SHOTGUN", "WELDER", "CHAINSAW", "LASER", "DEFLECTR", "STUN GUN", "MCROWAVE",
    "QCKSTEP"
};

/* Card colors — NOT-64 palette only (ABGR: 0xFFBBGGRR from #RRGGBB) */
static const uint32_t card_colors[] = {
    0xFFB4654D,  /* SHIELD    - #4d65b4 pal blue */
    0xFF3138B3,  /* SHOOT     - #b33831 pal red */
    0xFF1D6BFB,  /* BURST     - #fb6b1d pal orange */
    0xFF639023,  /* MOVE      - #239063 pal green */
    0xFF2BC2F9,  /* MELEE     - #f9c22b pal gold */
    0xFF9BAF0E,  /* OVERCHRG  - #0eaf9b pal teal */
    0xFF73BC1E,  /* REPAIR    - #1ebc73 pal emerald */
    0xFF6F4BA2,  /* STUN      - #a24b6f pal mauve */
    0xFFE69B4D,  /* FORTIFY   - #4d9be6 pal bright blue */
    0xFF3B3BE8,  /* DBL SHOT  - #e83b3b pal bright red */
    0xFF4BE0D5,  /* DASH      - #d5e04b pal lime */
    0xFFFDD38F,  /* ICE       - #8fd3ff pal light blue */
    0xFF47A9A2,  /* ACID      - #a2a947 pal olive */
    0xFF364FEA,  /* FIRE      - #ea4f36 pal orange-red */
    0xFF69DB91,  /* LIGHTNING  - #91db69 pal light green */
    0xFF4C5A16,  /* SNIPER    - #165a4c pal dark teal */
    0xFF3D68CD,  /* SHOTGUN   - #cd683d pal copper */
    0xFF1797F7,  /* WELDER    - #f79617 pal amber */
    0xFF3423AE,  /* CHAINSAW  - #ae2334 pal crimson */
    0xFFB9E130,  /* LASER     - #30e1b9 pal bright cyan */
    0xFF8A707F,  /* DEFLECTOR - #7f708a pal steel */
    0xFFF384A8,  /* STUN GUN  - #a884f3 pal lavender */
    0xFF4A7DF5,  /* MCROWAVE  - #f57d4a pal salmon */
    0xFF6CDF34,  /* QCKSTEP   - #34df6c (close to #cddf6c) pal yellow-green */
};

static const int card_energy_cost[] = {
    1, 1, 2, 1, 1,  /* base cards */
    0, 2, 1, 2, 2, 2, /* droppable cards */
    1, 1, 1, 2, /* elemental cards */
    1, 1, 1, 2, 1, 1, 1, 2, /* class-specific: sniper, shotgun, welder, chainsaw, laser, deflector, stun gun, microwave */
    1 /* quickstep */
};

/* Card target types */
enum { TARGET_ENEMY, TARGET_ALL_ENEMIES, TARGET_SELF };

static const int card_targets[] = {
    TARGET_SELF,          /* SHIELD */
    TARGET_ENEMY,         /* SHOOT */
    TARGET_ALL_ENEMIES,   /* BURST */
    TARGET_SELF,          /* MOVE */
    TARGET_ENEMY,         /* MELEE */
    TARGET_SELF,          /* OVERCHARGE */
    TARGET_SELF,          /* REPAIR */
    TARGET_ALL_ENEMIES,   /* STUN */
    TARGET_SELF,          /* FORTIFY */
    TARGET_ENEMY,         /* DOUBLE SHOT */
    TARGET_ENEMY,         /* DASH */
    TARGET_ENEMY,         /* ICE */
    TARGET_ENEMY,         /* ACID */
    TARGET_ENEMY,         /* FIRE */
    TARGET_ENEMY,         /* LIGHTNING */
    TARGET_ENEMY,         /* SNIPER */
    TARGET_ENEMY,         /* SHOTGUN */
    TARGET_ENEMY,         /* WELDER (melee) */
    TARGET_ENEMY,         /* CHAINSAW (melee) */
    TARGET_ENEMY,         /* LASER */
    TARGET_SELF,          /* DEFLECTOR */
    TARGET_ENEMY,         /* STUN GUN */
    TARGET_ENEMY,         /* MICROWAVE */
    TARGET_SELF,          /* QUICKSTEP */
};

/* ── Character classes ───────────────────────────────────────────── */

enum { CLASS_SCOUT, CLASS_MARINE, CLASS_ENGINEER, CLASS_SCIENTIST };

typedef struct {
    int hp_max;
    int deck_composition[CARD_TYPE_COUNT]; /* count of each card type */
    const char *name;
    const uint32_t *sprite;
} char_class;

static const char_class char_classes[] = {
    [CLASS_SCOUT] = {
        .hp_max = 18,
        /* 2 shield, 0 shoot, 1 burst, 3 move, 0 melee, ..., 2 sniper, 2 shotgun, ..., 2 quickstep */
        .deck_composition = { 2, 0, 1, 3, 0, 0,0,0,0,0,0, 0,0,0,0, 2, 2, 0,0,0,0,0,0, 2 },
        .name = "SCOUT",
        .sprite = spr_scout,
    },
    [CLASS_MARINE] = {
        .hp_max = 30,
        .deck_composition = { 4, 3, 1, 1, 1 }, /* 4 shield, 3 shoot, 1 burst, 1 move, 1 melee */
        .name = "MARINE",
        .sprite = spr_marine,
    },
    [CLASS_ENGINEER] = {
        .hp_max = 26,
        /* 2 shield, 1 shoot, 1 burst, 1 move, 0 melee, ..., welder 3, chainsaw 2 */
        .deck_composition = { 2, 1, 1, 1, 0, 0,0,0,0,0,0, 0,0,0,0, 0,0, 3, 2 },
        .name = "ENGINEER",
        .sprite = spr_engineer,
    },
    [CLASS_SCIENTIST] = {
        .hp_max = 22,
        /* 1 shield, 1 shoot, 1 burst, 1 move, 0 melee, ..., laser 2, deflector 2, stun_gun 1, microwave 2 */
        .deck_composition = { 1, 1, 1, 1, 0, 0,0,0,0,0,0, 0,0,0,0, 0,0,0,0, 2, 2, 1, 2 },
        .name = "SCIENTIST",
        .sprite = spr_scientist,
    },
};

/* ── Enemy types ─────────────────────────────────────────────────── */

enum { ENEMY_LURKER, ENEMY_BRUTE, ENEMY_SPITTER, ENEMY_HIVEGUARD, ENEMY_TYPE_COUNT };

/* ── Element types (for weakness system) ─────────────────────────── */

enum { ELEM_ICE, ELEM_ACID, ELEM_FIRE, ELEM_LIGHTNING, ELEM_COUNT };

static const char *elem_names[] = { "ICE", "ACID", "FIRE", "LIGHTNING" };
static const uint32_t elem_colors[] = {
    0xFFFFCC44,   /* ICE: cyan-ish (ABGR) */
    0xFF22CCAA,   /* ACID: green */
    0xFF2244FF,   /* FIRE: red-orange */
    0xFFEEEE44,   /* LIGHTNING: yellow */
};

/* Map card type -> element index (-1 = not elemental) */
static int card_element(int card) {
    switch (card) {
        case CARD_ICE:       return ELEM_ICE;
        case CARD_ACID:      return ELEM_ACID;
        case CARD_FIRE:      return ELEM_FIRE;
        case CARD_LIGHTNING: return ELEM_LIGHTNING;
        default:             return -1;
    }
}

/* ── Enemy weakness system (per run) ────────────────────────────── */

typedef struct {
    int weakness[ENEMY_TYPE_COUNT];        /* element each enemy type is weak to */
    bool discovered[ENEMY_TYPE_COUNT][ELEM_COUNT]; /* which elements tested per type */
    bool weakness_known[ENEMY_TYPE_COUNT]; /* true once we found the weakness */
    bool initialized;
} weakness_table;

static weakness_table g_weakness;

/* Randomize weaknesses for a new game run using the given seed */
static void weakness_init(uint32_t seed) {
    memset(&g_weakness, 0, sizeof(g_weakness));
    uint32_t rng = seed ^ 0xDEADBEEF;
    for (int i = 0; i < ENEMY_TYPE_COUNT; i++) {
        rng = rng * 1103515245u + 12345u;
        g_weakness.weakness[i] = (int)(((rng >> 16) & 0x7FFF) % (uint32_t)ELEM_COUNT);
    }
    g_weakness.initialized = true;
}

/* Call when player attacks enemy_type with an elemental card.
   Returns true if weakness was just discovered for the first time. */
static bool weakness_discover(int enemy_type, int elem) {
    if (enemy_type < 0 || enemy_type >= ENEMY_TYPE_COUNT) return false;
    if (elem < 0 || elem >= ELEM_COUNT) return false;
    g_weakness.discovered[enemy_type][elem] = true;
    if (g_weakness.weakness[enemy_type] == elem && !g_weakness.weakness_known[enemy_type]) {
        g_weakness.weakness_known[enemy_type] = true;
        return true;
    }
    return false;
}

/* Check if enemy_type is weak to elem */
static bool weakness_check(int enemy_type, int elem) {
    if (enemy_type < 0 || enemy_type >= ENEMY_TYPE_COUNT) return false;
    if (elem < 0 || elem >= ELEM_COUNT) return false;
    return g_weakness.weakness[enemy_type] == elem;
}

/* Element icon texture indices (matches STEX_ enum order) */
static const int elem_icon_stex[] = {
    STEX_ICON_ICE, STEX_ICON_ACID, STEX_ICON_FIRE, STEX_ICON_LIGHTNING
};

/* Enemy intent types */
enum { INTENT_ATTACK, INTENT_MOVE };

typedef struct {
    const char *name;
    int hp_max;
    int damage;
    int attack_range;  /* distance at which this enemy attacks */
    int ranged;        /* 1 = ranged attack, 0 = melee */
} enemy_template;

static const enemy_template enemy_templates[] = {
    [ENEMY_LURKER]    = { "LURKER",     8,  2, 2, 1 },  /* ranged: attacks at dist <= 2 */
    [ENEMY_BRUTE]     = { "BRUTE",     18,  5, 1, 0 },  /* melee: attacks at dist <= 1 */
    [ENEMY_SPITTER]   = { "SPITTER",   10,  3, 3, 1 },  /* ranged: attacks at dist <= 3 */
    [ENEMY_HIVEGUARD] = { "HIVEGUARD", 24,  4, 2, 1 },  /* ranged: attacks at dist <= 2 */
};

/* ── Combat state ────────────────────────────────────────────────── */

#define COMBAT_MAX_ENEMIES   4
#define COMBAT_DECK_MAX      20
#define COMBAT_HAND_MAX      5

typedef struct {
    int type;
    int hp;
    int hp_max;
    int attack_range;  /* distance at which this enemy can attack */
    int distance;      /* this enemy's distance from player (0=melee, max 5) */
    int damage;
    int ranged;
    int intent;        /* INTENT_ATTACK or INTENT_MOVE */
    int flash_timer;   /* > 0 = flashing white */
    bool alive;
    /* Elemental status effects */
    int ice_turns;       /* > 0: frozen, skip move every other turn, reduced dmg */
    int acid_stacks;     /* > 0: take acid_stacks dmg each turn, stackable */
    int fire_turns;      /* > 0: burning, take fire dmg each turn, can spread */
    int lightning_stun;  /* > 0: stunned, can't move or attack */
} combat_enemy;

typedef struct {
    /* Player */
    int player_class;
    int player_hp;
    int player_hp_max;
    int player_shield;
    int player_move_pts;  /* move points available to spend this turn */
    int energy;
    int energy_max;

    /* Deck (persistent across fights, grows with rewards) */
    int deck[COMBAT_DECK_MAX];
    int deck_count;
    int discard[COMBAT_DECK_MAX];
    int discard_count;
    int hand[COMBAT_HAND_MAX];
    int hand_count;

    /* Reward selection */
    int reward_choices[3];
    int reward_cursor;

    /* Enemies */
    combat_enemy enemies[COMBAT_MAX_ENEMIES];
    int enemy_count;

    /* UI state */
    int cursor;           /* selected card in hand */
    int target;           /* targeted enemy (for shoot) */
    int phase;            /* combat phase */
    int turn;
    int anim_timer;       /* for animations */
    int message_timer;
    char message[64];

    /* Sequential enemy attack state */
    int enemy_atk_idx;    /* which enemy is currently attacking (-1 = none) */
    int enemy_atk_timer;  /* countdown for current enemy's wiggle+attack */

    /* Player visual feedback */
    int player_flash_timer;       /* > 0 = player flickers red (took damage) */
    int player_shield_flash_timer;/* > 0 = blue sphere (shield absorbed) */

    /* Combat log */
    #define COMBAT_LOG_MAX  64
    #define COMBAT_LOG_LINE 48
    char log[COMBAT_LOG_MAX][COMBAT_LOG_LINE];
    int log_count;
    int log_scroll;       /* scroll offset when viewing */
    bool log_open;        /* true = log overlay visible */

    /* Drag state */
    bool dragging;
    int drag_card;         /* index in hand being dragged */
    float drag_x, drag_y;  /* current drag position (framebuffer coords) */
    float drag_start_x, drag_start_y;

    /* Elemental state */
    int fire_atk_bonus;  /* +1 per burning enemy, boosts player attack dmg */
    bool player_deflect; /* true = deflector active, reflects damage back */

    /* Enemy info popup */
    int info_popup_enemy;    /* -1 = none, else enemy index to show details for */
    int info_popup_timer;    /* auto-dismiss timer */

    /* Pile viewer overlay */
    bool deck_view_open;
    bool discard_view_open;
    int pile_view_selected;   /* -1 = none */

    /* Result */
    bool combat_over;
    bool player_won;
    bool initialized;
} combat_state;

enum {
    CPHASE_DRAW,         /* drawing cards animation */
    CPHASE_PLAYER_TURN,  /* player selects and plays cards */
    CPHASE_ENEMY_TURN,   /* enemies attack */
    CPHASE_REWARD,       /* pick 1 of 3 card rewards */
    CPHASE_RESULT,       /* win/lose screen */
};

static combat_state combat;

/* ── Persistent player state (survives between combats) ──────────── */

typedef struct {
    int player_class;
    int hp;
    int hp_max;
    int persistent_deck[COMBAT_DECK_MAX];
    int persistent_deck_count;
    bool initialized;
} player_persist;

static player_persist g_player;

static void player_persist_init(int player_class) {
    memset(&g_player, 0, sizeof(g_player));
    g_player.player_class = player_class;
    g_player.hp_max = char_classes[player_class].hp_max;
    g_player.hp = g_player.hp_max;
    g_player.persistent_deck_count = 0;
    const char_class *cc = &char_classes[player_class];
    for (int type = 0; type < CARD_TYPE_COUNT; type++) {
        for (int i = 0; i < cc->deck_composition[type]; i++) {
            if (g_player.persistent_deck_count < COMBAT_DECK_MAX)
                g_player.persistent_deck[g_player.persistent_deck_count++] = type;
        }
    }
    g_player.initialized = true;
}

/* ── Deck management ─────────────────────────────────────────────── */

static void combat_shuffle_deck(combat_state *cs) {
    for (int i = cs->deck_count - 1; i > 0; i--) {
        int j = dng_rng_int(i + 1);
        int tmp = cs->deck[i];
        cs->deck[i] = cs->deck[j];
        cs->deck[j] = tmp;
    }
}

static void combat_build_deck(combat_state *cs) {
    /* Copy from persistent deck */
    cs->deck_count = 0;
    for (int i = 0; i < g_player.persistent_deck_count; i++) {
        if (cs->deck_count < COMBAT_DECK_MAX)
            cs->deck[cs->deck_count++] = g_player.persistent_deck[i];
    }
    cs->discard_count = 0;
    combat_shuffle_deck(cs);
}

static void combat_draw_hand(combat_state *cs) {
    cs->hand_count = 0;
    for (int i = 0; i < COMBAT_HAND_MAX; i++) {
        if (cs->deck_count == 0) {
            /* Reshuffle discard into deck */
            if (cs->discard_count == 0) break;
            for (int j = 0; j < cs->discard_count; j++)
                cs->deck[j] = cs->discard[j];
            cs->deck_count = cs->discard_count;
            cs->discard_count = 0;
            combat_shuffle_deck(cs);
        }
        cs->hand[cs->hand_count++] = cs->deck[--cs->deck_count];
    }
}

/* ── Combat initialization ───────────────────────────────────────── */

static void combat_generate_rewards(combat_state *cs) {
    /* Pick 3 unique droppable card types (CARD_OVERCHARGE through CARD_LIGHTNING) */
    int droppable_start = CARD_OVERCHARGE;
    int droppable_count = CARD_SNIPER - droppable_start; /* exclude class-specific cards */
    for (int i = 0; i < 3; i++) {
        int attempts = 0;
        do {
            cs->reward_choices[i] = droppable_start + dng_rng_int(droppable_count);
            attempts++;
        } while (attempts < 20 && (
            (i > 0 && cs->reward_choices[i] == cs->reward_choices[0]) ||
            (i > 1 && cs->reward_choices[i] == cs->reward_choices[1])
        ));
    }
    cs->reward_cursor = 0;
}

static void combat_log(combat_state *cs, const char *fmt, ...); /* forward decl */

static void combat_init(combat_state *cs, int player_class, int floor, int cell_alien_type) {
    memset(cs, 0, sizeof(*cs));
    cs->player_class = player_class;
    cs->player_hp_max = g_player.hp_max;
    cs->player_hp = g_player.hp;  /* carry over HP */
    cs->player_shield = 0;
    cs->energy = 3;
    cs->energy_max = 3;

    combat_build_deck(cs);

    /* Primary enemy is what was on the cell (1-indexed, so subtract 1) */
    int primary_type = (cell_alien_type > 0) ? (cell_alien_type - 1) : 0;
    if (primary_type >= ENEMY_TYPE_COUNT) primary_type = 0;

    /* Enemy count: 1-3, deeper floors = more */
    cs->enemy_count = 1 + dng_rng_int(2 + (floor > 1 ? 1 : 0));
    if (cs->enemy_count > COMBAT_MAX_ENEMIES)
        cs->enemy_count = COMBAT_MAX_ENEMIES;

    for (int i = 0; i < cs->enemy_count; i++) {
        int type;
        if (i == 0) {
            type = primary_type; /* first enemy matches what you walked into */
        } else {
            /* Additional enemies: weaker or equal to primary */
            type = dng_rng_int(primary_type + 1);
        }

        const enemy_template *tmpl = &enemy_templates[type];
        cs->enemies[i].type = type;
        cs->enemies[i].hp = tmpl->hp_max;
        cs->enemies[i].hp_max = tmpl->hp_max;
        cs->enemies[i].attack_range = tmpl->attack_range;
        cs->enemies[i].damage = tmpl->damage;
        cs->enemies[i].ranged = tmpl->ranged;
        cs->enemies[i].distance = 3 + i; /* stagger: first at 3, rest further */
        if (cs->enemies[i].distance > 5) cs->enemies[i].distance = 5;
        cs->enemies[i].intent = INTENT_MOVE;
        cs->enemies[i].flash_timer = 0;
        cs->enemies[i].alive = true;
    }

    cs->cursor = 0;
    cs->target = 0;
    cs->phase = CPHASE_DRAW;
    cs->turn = 1;
    cs->anim_timer = 30;
    cs->combat_over = false;
    cs->player_won = false;
    cs->initialized = true;
    cs->message_timer = 0;
    cs->log_count = 0;
    cs->log_scroll = 0;
    cs->log_open = false;
    cs->info_popup_enemy = -1;
    cs->info_popup_timer = 0;
    snprintf(cs->message, sizeof(cs->message), "ENEMIES DETECTED!");
    combat_log(cs, "-- COMBAT START --");
}

/* ── Card logic ──────────────────────────────────────────────────── */

static void combat_set_message(combat_state *cs, const char *msg) {
    snprintf(cs->message, sizeof(cs->message), "%s", msg);
    cs->message_timer = 60;
}

/* ── Combat log ─────────────────────────────────────────────────── */

static void combat_log(combat_state *cs, const char *fmt, ...) {
    if (cs->log_count >= COMBAT_LOG_MAX) {
        /* Shift everything up by 1 */
        for (int i = 0; i < COMBAT_LOG_MAX - 1; i++)
            memcpy(cs->log[i], cs->log[i+1], COMBAT_LOG_LINE);
        cs->log_count = COMBAT_LOG_MAX - 1;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cs->log[cs->log_count], COMBAT_LOG_LINE, fmt, ap);
    va_end(ap);
    cs->log_count++;
}

/* ── Distance helpers ───────────────────────────────────────────── */

#define COMBAT_MAX_DISTANCE 5

/* Get distance to the targeted enemy (or first alive) */
static int combat_target_distance(combat_state *cs) {
    int t = cs->target;
    if (t >= 0 && t < cs->enemy_count && cs->enemies[t].alive)
        return cs->enemies[t].distance;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].alive) return cs->enemies[i].distance;
    return 0;
}

/* Get minimum distance across all alive enemies */
static int combat_min_distance(combat_state *cs) {
    int mind = COMBAT_MAX_DISTANCE;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].alive && cs->enemies[i].distance < mind)
            mind = cs->enemies[i].distance;
    return mind;
}

/* ── Enemy intent ───────────────────────────────────────────────── */

static void combat_roll_intents(combat_state *cs) {
    for (int i = 0; i < cs->enemy_count; i++) {
        combat_enemy *e = &cs->enemies[i];
        if (!e->alive) continue;
        bool stunned = (e->flash_timer > 10) || (e->lightning_stun > 0);
        if (stunned) {
            e->intent = INTENT_MOVE;
            continue;
        }
        bool in_range = (e->distance <= e->attack_range);
        e->intent = in_range ? INTENT_ATTACK : INTENT_MOVE;
    }
}

static bool combat_all_enemies_dead(combat_state *cs) {
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].alive) return false;
    return true;
}

static int combat_first_alive_enemy(combat_state *cs) {
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].alive) return i;
    return -1;
}

static void combat_deal_damage_enemy(combat_state *cs, int idx, int dmg) {
    if (idx < 0 || idx >= cs->enemy_count || !cs->enemies[idx].alive) return;
    cs->enemies[idx].hp -= dmg;
    cs->enemies[idx].flash_timer = 10;
    if (cs->enemies[idx].hp <= 0) {
        cs->enemies[idx].hp = 0;
        cs->enemies[idx].alive = false;
    }
}

static void combat_deal_damage_player(combat_state *cs, int dmg) {
    dmg = dmg / 2; /* halve incoming damage, round down */
    if (dmg < 1) dmg = 1;
    int absorbed = dmg < cs->player_shield ? dmg : cs->player_shield;
    cs->player_shield -= absorbed;
    int actual = dmg - absorbed;
    if (absorbed > 0) {
        cs->player_shield_flash_timer = 20;
        combat_log(cs, "  shield absorb %d", absorbed);
    }
    if (actual > 0) {
        cs->player_hp -= actual;
        if (cs->player_hp <= 0) cs->player_hp = 0;
        cs->player_flash_timer = 16;
        combat_log(cs, "  took %d dmg (%d HP)", actual, cs->player_hp);
    } else {
        combat_log(cs, "  fully blocked!");
    }
}

static void combat_play_card(combat_state *cs, int hand_idx) {
    if (hand_idx < 0 || hand_idx >= cs->hand_count) return;
    int card = cs->hand[hand_idx];
    int cost = card_energy_cost[card];
    char buf[64];

    /* Check energy */
    if (cs->energy < cost) {
        snprintf(buf, sizeof(buf), "NEED %d ENERGY", cost);
        combat_set_message(cs, buf);
        return;
    }
    cs->energy -= cost;
    combat_log(cs, "played %s (%dE)", card_names[card], cost);

    switch (card) {
        case CARD_SHIELD:
            cs->player_shield += 3;
            combat_set_message(cs, "SHIELD +3");
            break;

        case CARD_SHOOT: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                int dmg = 3 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "SHOOT %s -%dHP", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_BURST: {
            int dmg = 2 + cs->fire_atk_bonus;
            for (int i = 0; i < cs->enemy_count; i++) {
                if (cs->enemies[i].alive)
                    combat_deal_damage_enemy(cs, i, dmg);
            }
            snprintf(buf, sizeof(buf), "BURST -%dHP ALL", dmg);
            combat_set_message(cs, buf);
            break;
        }

        case CARD_MOVE:
            cs->player_move_pts += 2;
            snprintf(buf, sizeof(buf), "+2 MOVE PTS (%d)", cs->player_move_pts);
            combat_set_message(cs, buf);
            break;

        case CARD_MELEE: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0 && cs->enemies[t].distance <= 0) {
                int dmg = 6 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "MELEE %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            } else {
                int d = (t >= 0) ? cs->enemies[t].distance : 0;
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", d);
                combat_set_message(cs, buf);
                cs->energy += cost; /* refund */
                return; /* don't consume card */
            }
            break;
        }

        case CARD_OVERCHARGE:
            cs->energy += 2;
            combat_set_message(cs, "OVERCHARGE! +2 ENERGY");
            break;

        case CARD_REPAIR:
            cs->player_hp += 4;
            if (cs->player_hp > cs->player_hp_max) cs->player_hp = cs->player_hp_max;
            combat_set_message(cs, "REPAIR +4HP");
            break;

        case CARD_STUN:
            for (int i = 0; i < cs->enemy_count; i++)
                if (cs->enemies[i].alive) cs->enemies[i].flash_timer = 20;
            combat_set_message(cs, "STUN! ENEMIES SKIP TURN");
            cs->player_shield += 1; /* minor shield bonus */
            break;

        case CARD_FORTIFY:
            cs->player_shield += 6;
            combat_set_message(cs, "FORTIFY! SHIELD +6");
            break;

        case CARD_DOUBLE_SHOT: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                int dmg = 5 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "DBL SHOT %s -%dHP", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_DASH:
            cs->player_move_pts += 3;
            {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    int dmg = 4 + cs->fire_atk_bonus;
                    combat_deal_damage_enemy(cs, t, dmg);
                    snprintf(buf, sizeof(buf), "DASH +3MP %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                    combat_set_message(cs, buf);
                } else {
                    snprintf(buf, sizeof(buf), "DASH +3 MOVE PTS (%d)", cs->player_move_pts);
                    combat_set_message(cs, buf);
                }
            }
            break;

        /* ── Elemental cards ─────────────────────────────────── */
        case CARD_ICE: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                cs->enemies[t].ice_turns = 3;
                int dmg = 1;
                bool is_weak = weakness_check(cs->enemies[t].type, ELEM_ICE);
                if (is_weak) dmg *= 2;
                bool just_found = weakness_discover(cs->enemies[t].type, ELEM_ICE);
                combat_deal_damage_enemy(cs, t, dmg);
                if (just_found) {
                    snprintf(buf, sizeof(buf), "ICE %s! WEAK! x2!", enemy_templates[cs->enemies[t].type].name);
                    combat_log(cs, ">> %s WEAK TO ICE! <<", enemy_templates[cs->enemies[t].type].name);
                } else if (is_weak) {
                    snprintf(buf, sizeof(buf), "ICE %s! WEAK x2!", enemy_templates[cs->enemies[t].type].name);
                } else {
                    snprintf(buf, sizeof(buf), "ICE %s! FROZEN 3T", enemy_templates[cs->enemies[t].type].name);
                }
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_ACID: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                cs->enemies[t].acid_stacks++;
                int dmg = 1;
                bool is_weak = weakness_check(cs->enemies[t].type, ELEM_ACID);
                if (is_weak) dmg *= 2;
                bool just_found = weakness_discover(cs->enemies[t].type, ELEM_ACID);
                combat_deal_damage_enemy(cs, t, dmg);
                if (just_found) {
                    snprintf(buf, sizeof(buf), "ACID %s! WEAK! x2!", enemy_templates[cs->enemies[t].type].name);
                    combat_log(cs, ">> %s WEAK TO ACID! <<", enemy_templates[cs->enemies[t].type].name);
                } else if (is_weak) {
                    snprintf(buf, sizeof(buf), "ACID %s! WEAK x2!", enemy_templates[cs->enemies[t].type].name);
                } else {
                    snprintf(buf, sizeof(buf), "ACID %s x%d!", enemy_templates[cs->enemies[t].type].name, cs->enemies[t].acid_stacks);
                }
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_FIRE: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                cs->enemies[t].fire_turns = 3;
                int dmg = 1;
                bool is_weak = weakness_check(cs->enemies[t].type, ELEM_FIRE);
                if (is_weak) dmg *= 2;
                bool just_found = weakness_discover(cs->enemies[t].type, ELEM_FIRE);
                combat_deal_damage_enemy(cs, t, dmg);
                if (just_found) {
                    snprintf(buf, sizeof(buf), "FIRE %s! WEAK! x2!", enemy_templates[cs->enemies[t].type].name);
                    combat_log(cs, ">> %s WEAK TO FIRE! <<", enemy_templates[cs->enemies[t].type].name);
                } else if (is_weak) {
                    snprintf(buf, sizeof(buf), "FIRE %s! WEAK x2!", enemy_templates[cs->enemies[t].type].name);
                } else {
                    snprintf(buf, sizeof(buf), "FIRE %s! BURN 3T", enemy_templates[cs->enemies[t].type].name);
                }
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_LIGHTNING: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                int stun_turns = 1 + dng_rng_int(2); /* 1-2 turns */
                cs->enemies[t].lightning_stun = stun_turns;
                int dmg = 2;
                bool is_weak = weakness_check(cs->enemies[t].type, ELEM_LIGHTNING);
                if (is_weak) dmg *= 2;
                bool just_found = weakness_discover(cs->enemies[t].type, ELEM_LIGHTNING);
                combat_deal_damage_enemy(cs, t, dmg);
                if (just_found) {
                    snprintf(buf, sizeof(buf), "ZAP %s! WEAK! x2!", enemy_templates[cs->enemies[t].type].name);
                    combat_log(cs, ">> %s WEAK TO LIGHTNING! <<", enemy_templates[cs->enemies[t].type].name);
                } else if (is_weak) {
                    snprintf(buf, sizeof(buf), "ZAP %s! WEAK x2!", enemy_templates[cs->enemies[t].type].name);
                } else {
                    snprintf(buf, sizeof(buf), "ZAP %s! STUN %dT", enemy_templates[cs->enemies[t].type].name, stun_turns);
                }
                combat_set_message(cs, buf);
            }
            break;
        }

        /* ── Class-specific cards ───────────────────────────── */
        case CARD_SNIPER: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0 && cs->enemies[t].distance >= 2) {
                int dmg = 5 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "SNIPE %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            } else {
                int d = (t >= 0) ? cs->enemies[t].distance : 0;
                snprintf(buf, sizeof(buf), "TOO CLOSE! DIST: %d (NEED 2+)", d);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;
        }

        case CARD_SHOTGUN: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0 && cs->enemies[t].distance <= 1) {
                int dmg = 4 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "SHOTGUN %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            } else {
                int d = (t >= 0) ? cs->enemies[t].distance : 0;
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d (NEED 1-)", d);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;
        }

        case CARD_WELDER: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0 && cs->enemies[t].distance <= 0) {
                int dmg = 4 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "WELD %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            } else {
                int d = (t >= 0) ? cs->enemies[t].distance : 0;
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", d);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;
        }

        case CARD_CHAINSAW: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0 && cs->enemies[t].distance <= 0) {
                int dmg = 8 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "CHAINSAW %s -%dHP!!", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            } else {
                int d = (t >= 0) ? cs->enemies[t].distance : 0;
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", d);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;
        }

        case CARD_LASER: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                int dmg = 4 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                snprintf(buf, sizeof(buf), "LASER %s -%dHP", enemy_templates[cs->enemies[t].type].name, dmg);
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_DEFLECTOR:
            cs->player_shield += 4;
            cs->player_deflect = true;
            combat_set_message(cs, "DEFLECTOR +4 SHIELD, REFLECT ON");
            break;

        case CARD_STUN_GUN: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                cs->enemies[t].lightning_stun = 1;
                combat_deal_damage_enemy(cs, t, 1);
                snprintf(buf, sizeof(buf), "STUN GUN %s! STUN 1T", enemy_templates[cs->enemies[t].type].name);
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_MICROWAVE: {
            int t = cs->target;
            while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
            if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
            if (t >= 0) {
                int dmg = 5 + cs->fire_atk_bonus;
                combat_deal_damage_enemy(cs, t, dmg);
                if (!cs->enemies[t].alive) {
                    /* Kill chain: 3 dmg to all surviving enemies */
                    int chain_kills = 0;
                    for (int i = 0; i < cs->enemy_count; i++) {
                        if (i != t && cs->enemies[i].alive) {
                            combat_deal_damage_enemy(cs, i, 3);
                            chain_kills++;
                        }
                    }
                    snprintf(buf, sizeof(buf), "MCRWAVE %s KILL! 3DMG ALL",
                             enemy_templates[cs->enemies[t].type].name);
                } else {
                    snprintf(buf, sizeof(buf), "MCRWAVE %s -%dHP",
                             enemy_templates[cs->enemies[t].type].name, dmg);
                }
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_QUICKSTEP:
            if (cs->discard_count < COMBAT_DECK_MAX) {
                cs->discard[cs->discard_count++] = CARD_MOVE;
            }
            combat_set_message(cs, "QUICKSTEP! +1 MOVE");
            break;
    }

    /* Move card to discard (movement cards are exhausted instead) */
    if (card != CARD_MOVE && card != CARD_DASH)
        cs->discard[cs->discard_count++] = card;

    /* Remove from hand */
    for (int i = hand_idx; i < cs->hand_count - 1; i++)
        cs->hand[i] = cs->hand[i + 1];
    cs->hand_count--;

    if (cs->cursor >= cs->hand_count && cs->hand_count > 0)
        cs->cursor = cs->hand_count - 1;
}

/* ── Status effect tick (called at start of enemy turn) ──────────── */

static void combat_tick_status_effects(combat_state *cs) {
    cs->fire_atk_bonus = 0;
    for (int i = 0; i < cs->enemy_count; i++) {
        combat_enemy *e = &cs->enemies[i];
        if (!e->alive) continue;

        /* Acid: deal damage per stack each turn */
        if (e->acid_stacks > 0) {
            e->hp -= e->acid_stacks;
            e->flash_timer = 10;
            if (e->hp <= 0) { e->hp = 0; e->alive = false; }
        }

        /* Fire: deal 2 damage per turn, grant player attack bonus */
        if (e->fire_turns > 0) {
            e->hp -= 2;
            e->flash_timer = 10;
            cs->fire_atk_bonus++;
            e->fire_turns--;
            if (e->hp <= 0) { e->hp = 0; e->alive = false; }
        }

        /* Ice: deal 1 damage, decrement */
        if (e->ice_turns > 0) {
            e->hp -= 1;
            e->flash_timer = 10;
            e->ice_turns--;
            if (e->hp <= 0) { e->hp = 0; e->alive = false; }
        }

        /* Lightning: decrement stun counter */
        if (e->lightning_stun > 0) {
            e->lightning_stun--;
        }
    }

    /* Fire spread: if any enemy is burning, adjacent alive enemies catch fire */
    for (int i = 0; i < cs->enemy_count; i++) {
        combat_enemy *e = &cs->enemies[i];
        if (!e->alive || e->fire_turns <= 0) continue;
        /* Spread to neighbors */
        if (i > 0 && cs->enemies[i-1].alive && cs->enemies[i-1].fire_turns == 0)
            cs->enemies[i-1].fire_turns = 2;
        if (i + 1 < cs->enemy_count && cs->enemies[i+1].alive && cs->enemies[i+1].fire_turns == 0)
            cs->enemies[i+1].fire_turns = 2;
    }
}

/* ── Enemy turn (sequential) ─────────────────────────────────────── */

#define ENEMY_ATK_WIGGLE_FRAMES  20  /* wiggle before strike */
#define ENEMY_ATK_HIT_FRAME      10  /* frame at which damage lands */
#define ENEMY_ATK_TOTAL_FRAMES   30  /* total anim per enemy */

/* Check if an enemy can attack at its current distance */
static bool combat_enemy_in_range(combat_state *cs, int idx) {
    return cs->enemies[idx].distance <= cs->enemies[idx].attack_range;
}

/* Find next enemy that will attack (alive, not stunned, in range) */
static int combat_next_attacker(combat_state *cs, int from) {
    for (int i = from; i < cs->enemy_count; i++) {
        if (!cs->enemies[i].alive) continue;
        if (cs->enemies[i].flash_timer > 10) continue; /* stunned */
        if (cs->enemies[i].lightning_stun > 0) continue; /* lightning stun */
        if (combat_enemy_in_range(cs, i))
            return i;
    }
    return -1; /* no more attackers */
}

/* Start the sequential enemy attack phase */
static void combat_begin_enemy_turn(combat_state *cs) {
    /* Tick status effects at start of enemy turn */
    combat_tick_status_effects(cs);

    /* Check if all enemies died from status effects */
    if (combat_all_enemies_dead(cs)) {
        cs->player_won = true;
        g_player.hp = cs->player_hp;
        combat_generate_rewards(cs);
        cs->phase = CPHASE_REWARD;
        combat_set_message(cs, "VICTORY! PICK A CARD");
        return;
    }

    /* Each alive non-stunned enemy tries to close its own distance */
    for (int i = 0; i < cs->enemy_count; i++) {
        combat_enemy *e = &cs->enemies[i];
        if (!e->alive) continue;
        if (e->flash_timer > 10) continue; /* stunned */
        if (e->lightning_stun > 0) continue; /* lightning stun */
        if (e->ice_turns > 0 && (cs->turn % 2) == 0) continue; /* ice: skip move every other turn */
        /* Advance if not yet in attack range */
        if (e->distance > e->attack_range) {
            e->distance--;
            combat_log(cs, "%s advances -> dist %d",
                       enemy_templates[e->type].name, e->distance);
        }
    }

    cs->enemy_atk_idx = combat_next_attacker(cs, 0);
    if (cs->enemy_atk_idx < 0) {
        /* No enemies can attack — skip straight to next draw phase */
        cs->turn++;
        cs->phase = CPHASE_DRAW;
        cs->anim_timer = 20;
        return;
    }
    cs->enemy_atk_timer = ENEMY_ATK_TOTAL_FRAMES;
}

/* ── Update ──────────────────────────────────────────────────────── */

static void combat_update(combat_state *cs) {
    /* Decrement timers */
    if (cs->message_timer > 0) cs->message_timer--;
    if (cs->player_flash_timer > 0) cs->player_flash_timer--;
    if (cs->player_shield_flash_timer > 0) cs->player_shield_flash_timer--;
    if (cs->info_popup_timer > 0) {
        cs->info_popup_timer--;
        if (cs->info_popup_timer == 0) cs->info_popup_enemy = -1;
    }
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].flash_timer > 0) cs->enemies[i].flash_timer--;

    if (cs->phase == CPHASE_DRAW) {
        if (cs->anim_timer > 0) {
            cs->anim_timer--;
        } else {
            cs->player_shield = 0; /* shield expires at start of new turn */
            cs->player_deflect = false; /* deflector expires with shield */
            cs->energy = cs->energy_max; /* refill energy each turn */
            combat_draw_hand(cs);
            cs->cursor = 0;
            cs->target = combat_first_alive_enemy(cs);
            if (cs->target < 0) cs->target = 0;
            combat_roll_intents(cs); /* enemies decide what they'll do */
            combat_log(cs, "-- TURN %d --", cs->turn);
            cs->phase = CPHASE_PLAYER_TURN;
        }
    }

    if (cs->phase == CPHASE_ENEMY_TURN) {
        if (cs->enemy_atk_idx < 0) {
            /* All enemies done — advance to next turn */
            if (cs->player_hp <= 0) {
                g_player.hp = 0;
                cs->phase = CPHASE_RESULT;
                cs->player_won = false;
                cs->combat_over = true;
                combat_set_message(cs, "DEFEATED...");
            } else {
                cs->turn++;
                cs->phase = CPHASE_DRAW;
                cs->anim_timer = 20;
            }
            return;
        }

        cs->enemy_atk_timer--;

        /* Action lands at the hit frame */
        if (cs->enemy_atk_timer == ENEMY_ATK_HIT_FRAME) {
            combat_enemy *e = &cs->enemies[cs->enemy_atk_idx];
            char buf[64];

            int dmg = e->damage;
            if (e->ice_turns > 0) dmg = dmg / 2;
            if (dmg < 1) dmg = 1;
            combat_log(cs, "%s attacks for %d",
                       enemy_templates[e->type].name, dmg);
            if (cs->player_deflect) {
                /* Deflector reflects all damage back to attacker */
                combat_deal_damage_enemy(cs, cs->enemy_atk_idx, dmg);
                combat_log(cs, "  deflector reflects %d back!", dmg);
                snprintf(buf, sizeof(buf), "REFLECT %d -> %s",
                         dmg, enemy_templates[e->type].name);
                combat_set_message(cs, buf);
            } else {
                combat_deal_damage_player(cs, dmg);
                snprintf(buf, sizeof(buf), "%s ATTACKS -%dHP",
                         enemy_templates[e->type].name, dmg);
                combat_set_message(cs, buf);
            }
        }

        /* When this enemy's anim is done, move to next attacker */
        if (cs->enemy_atk_timer <= 0) {
            int next = combat_next_attacker(cs, cs->enemy_atk_idx + 1);
            cs->enemy_atk_idx = next;
            if (next >= 0) {
                cs->enemy_atk_timer = ENEMY_ATK_TOTAL_FRAMES;
            }
            /* if next < 0, the top-of-function check handles end-of-phase next frame */
        }
    }
}

/* ── Shared actions (called by both key and touch) ───────────────── */

static void combat_check_victory(combat_state *cs) {
    if (combat_all_enemies_dead(cs)) {
        cs->player_won = true;
        /* Save HP back to persistent state */
        g_player.hp = cs->player_hp;
        /* Generate rewards */
        combat_generate_rewards(cs);
        cs->phase = CPHASE_REWARD;
        combat_set_message(cs, "VICTORY! PICK A CARD");
    }
}

static void combat_action_end_turn(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN) return;
    for (int i = 0; i < cs->hand_count; i++) {
        int c = cs->hand[i];
        if (c != CARD_MOVE && c != CARD_DASH)
            cs->discard[cs->discard_count++] = c;
    }
    cs->hand_count = 0;
    /* movement points persist between rounds */
    combat_log(cs, "-- ENEMY TURN --");
    /* Shield persists through enemy turn, cleared at next draw */
    cs->phase = CPHASE_ENEMY_TURN;
    combat_begin_enemy_turn(cs);
}

/* Spend 1 move point to change distance (affects all enemies) */
static void combat_action_move_forward(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN) return;
    if (cs->player_move_pts <= 0) {
        combat_set_message(cs, "NO MOVE POINTS");
        return;
    }
    int mind = combat_min_distance(cs);
    if (mind <= 0) {
        combat_set_message(cs, "ALREADY AT MELEE RANGE");
        return;
    }
    cs->player_move_pts--;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].alive && cs->enemies[i].distance > 0)
            cs->enemies[i].distance--;
    combat_log(cs, "advance -> min dist %d", combat_min_distance(cs));
    combat_roll_intents(cs);
    char buf[64];
    snprintf(buf, sizeof(buf), "ADVANCE! MP:%d", cs->player_move_pts);
    combat_set_message(cs, buf);
}

static void combat_action_move_back(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN) return;
    if (cs->player_move_pts <= 0) {
        combat_set_message(cs, "NO MOVE POINTS");
        return;
    }
    cs->player_move_pts--;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].alive && cs->enemies[i].distance < COMBAT_MAX_DISTANCE)
            cs->enemies[i].distance++;
    combat_log(cs, "retreat -> min dist %d", combat_min_distance(cs));
    combat_roll_intents(cs);
    char buf[64];
    snprintf(buf, sizeof(buf), "RETREAT! MP:%d", cs->player_move_pts);
    combat_set_message(cs, buf);
}

/* ── Button layout constants (used by both render and touch) ─────── */

#define BTN_END_X    (FB_WIDTH - 78)
#define BTN_END_Y    158
#define BTN_END_W    70
#define BTN_END_H    18

/* ── Card layout helpers ─────────────────────────────────────────── */

/* ── Fan layout for hand of cards ──────────────────────────────── */

#define CARD_FAN_W  70    /* card width */
#define CARD_FAN_H  96    /* card height */

static uint32_t card_fan_buf[CARD_FAN_W * CARD_FAN_H];

/* Compute fan position for card i. Returns bottom-center (cx, cy) and angle. */
static void combat_card_fan_pos(const combat_state *cs, int i,
                                float *out_cx, float *out_cy, float *out_angle) {
    bool is_sel = (i == cs->cursor && cs->phase == CPHASE_PLAYER_TURN);
    int n = cs->hand_count;
    float mid = (n > 1) ? (n - 1) * 0.5f : 0;
    float t = (n > 1) ? (i - mid) / mid : 0;  /* -1 to 1 */

    /* Horizontal spacing: cards overlap ~40% */
    float spacing = CARD_FAN_W * 0.6f;
    float total = (n - 1) * spacing;
    float cx = (FB_WIDTH - total) * 0.5f + i * spacing;

    /* Vertical: bottom of card near screen bottom, edges dip lower */
    float cy = (float)FB_HEIGHT + 4.0f + t * t * 10.0f;
    if (is_sel) cy -= 16;

    /* Fan angle: edge cards tilt outward */
    float angle = t * 0.10f;

    *out_cx = cx;
    *out_cy = cy;
    *out_angle = angle;
}

/* Blit source buffer rotated around its bottom-center to dst at (bot_cx, bot_cy) */
static void combat_blit_card(uint32_t *dst, int dw, int dh,
                             const uint32_t *src, int sw, int sh,
                             float bot_cx, float bot_cy, float angle) {
    float ca = cosf(angle), sa = sinf(angle);
    float ica = cosf(-angle), isa = sinf(-angle);
    float hw = sw * 0.5f;

    /* Bounding box from rotated corners */
    float corners[4][2] = {
        {-hw, -(float)sh}, {hw, -(float)sh}, {-hw, 0}, {hw, 0}
    };
    int x0 = dw, x1 = 0, y0 = dh, y1 = 0;
    for (int c = 0; c < 4; c++) {
        int rx = (int)(corners[c][0] * ca - corners[c][1] * sa + bot_cx);
        int ry = (int)(corners[c][0] * sa + corners[c][1] * ca + bot_cy);
        if (rx < x0) x0 = rx; if (rx > x1) x1 = rx;
        if (ry < y0) y0 = ry; if (ry > y1) y1 = ry;
    }
    if (--x0 < 0) x0 = 0; if (--y0 < 0) y0 = 0;
    if (++x1 >= dw) x1 = dw - 1; if (++y1 >= dh) y1 = dh - 1;

    for (int dy = y0; dy <= y1; dy++) {
        for (int dx = x0; dx <= x1; dx++) {
            float lx = (dx - bot_cx) * ica - (dy - bot_cy) * isa + hw;
            float ly = (dx - bot_cx) * isa + (dy - bot_cy) * ica + (float)sh;
            int sx = (int)lx, sy = (int)ly;
            if (sx >= 0 && sx < sw && sy >= 0 && sy < sh) {
                uint32_t col = src[sy * sw + sx];
                if (col & 0xFF000000)
                    dst[dy * dw + dx] = col;
            }
        }
    }
}

/* Point-in-rotated-card test (bottom-center anchor) */
static bool combat_point_in_fan_card(float px, float py,
                                     float bot_cx, float bot_cy, float angle) {
    float ica = cosf(-angle), isa = sinf(-angle);
    float lx = (px - bot_cx) * ica - (py - bot_cy) * isa + CARD_FAN_W * 0.5f;
    float ly = (px - bot_cx) * isa + (py - bot_cy) * ica + (float)CARD_FAN_H;
    return lx >= 0 && lx < CARD_FAN_W && ly >= 0 && ly < CARD_FAN_H;
}

/* ── Touch drag input (Slay the Spire style) ─────────────────────── */

static void combat_touch_began(combat_state *cs, float fx, float fy) {
    /* Pile viewer overlay blocks input (handled by draw via ui_button) */
    if (cs->deck_view_open || cs->discard_view_open) return;

    /* Log button / log overlay interaction */
    if (cs->log_open) {
        /* Close button [X] */
        int lx = 20, lw = FB_WIDTH - 40;
        if (fx >= lx + lw - 40 && fx <= lx + lw && fy >= 10 && fy <= 24) {
            cs->log_open = false;
            return;
        }
        /* Tap anywhere else on overlay closes it too */
        cs->log_open = false;
        return;
    }
    /* LOG button */
    {
        int lb_x = FB_WIDTH - 32, lb_y = 38;
        if (fx >= lb_x && fx <= lb_x + 28 && fy >= lb_y && fy <= lb_y + 12) {
            cs->log_open = true;
            cs->log_scroll = 0;
            return;
        }
    }

    if (cs->phase == CPHASE_RESULT) return;

    /* Reward phase — tap a card to pick it */
    if (cs->phase == CPHASE_REWARD) {
        int rw = 72, rh = 80, rgap = 12;
        int rtotal = 3 * (rw + rgap) - rgap;
        int rbase_x = (FB_WIDTH - rtotal) / 2;
        int rbase_y = 75;
        for (int i = 0; i < 3; i++) {
            int rx = rbase_x + i * (rw + rgap);
            if (fx >= rx && fx < rx + rw && fy >= rbase_y && fy < rbase_y + rh) {
                /* Add chosen card to persistent deck */
                if (g_player.persistent_deck_count < COMBAT_DECK_MAX) {
                    g_player.persistent_deck[g_player.persistent_deck_count++] = cs->reward_choices[i];
                }
                cs->phase = CPHASE_RESULT;
                cs->combat_over = true;
                combat_set_message(cs, card_names[cs->reward_choices[i]]);
            }
        }
        return;
    }

    if (cs->phase != CPHASE_PLAYER_TURN) return;

    /* Check if tapping a card (front-to-back for proper overlap) */
    {
        int hit = -1;
        /* Selected card is on top */
        int sel = (cs->phase == CPHASE_PLAYER_TURN) ? cs->cursor : -1;
        if (sel >= 0 && sel < cs->hand_count) {
            float fcx, fcy, fa;
            combat_card_fan_pos(cs, sel, &fcx, &fcy, &fa);
            if (combat_point_in_fan_card(fx, fy, fcx, fcy, fa))
                hit = sel;
        }
        /* Then right-to-left (rightmost drawn last = on top) */
        if (hit < 0) {
            for (int i = cs->hand_count - 1; i >= 0; i--) {
                if (i == sel) continue;
                float fcx, fcy, fa;
                combat_card_fan_pos(cs, i, &fcx, &fcy, &fa);
                if (combat_point_in_fan_card(fx, fy, fcx, fcy, fa)) {
                    hit = i;
                    break;
                }
            }
        }
        if (hit >= 0) {
            cs->dragging = true;
            cs->drag_card = hit;
            cs->cursor = hit;
            cs->drag_x = fx;
            cs->drag_y = fy;
            cs->drag_start_x = fx;
            cs->drag_start_y = fy;
            return;
        }
    }

    /* Check END TURN button */
    if (fx >= BTN_END_X && fx <= BTN_END_X + BTN_END_W &&
        fy >= BTN_END_Y && fy <= BTN_END_Y + BTN_END_H) {
        combat_action_end_turn(cs);
        return;
    }

    /* Check move buttons */
    if (cs->player_move_pts > 0) {
        int mb_y = 238;
        if (fx >= 8 && fx <= 42 && fy >= mb_y && fy <= mb_y + 14) {
            combat_action_move_forward(cs);
            return;
        }
        if (fx >= 46 && fx <= 80 && fy >= mb_y && fy <= mb_y + 14) {
            combat_action_move_back(cs);
            return;
        }
    }

    /* Tap on enemy to target + show info popup */
    if (cs->enemy_count > 0) {
        int spacing = FB_WIDTH / (cs->enemy_count + 1);
        for (int i = 0; i < cs->enemy_count; i++) {
            if (!cs->enemies[i].alive) continue;
            int ecx = spacing * (i + 1);
            if (fx >= ecx - 24 && fx <= ecx + 24 && fy >= 10 && fy <= 90) {
                cs->target = i;
                /* Toggle info popup: tap same enemy again to dismiss */
                if (cs->info_popup_enemy == i) {
                    cs->info_popup_enemy = -1;
                } else {
                    cs->info_popup_enemy = i;
                    cs->info_popup_timer = 180; /* auto-dismiss after ~3 seconds */
                }
                return;
            }
        }
        /* Tapped elsewhere: dismiss popup */
        if (cs->info_popup_enemy >= 0) {
            cs->info_popup_enemy = -1;
            return;
        }
    }
}

static void combat_touch_moved(combat_state *cs, float fx, float fy) {
    if (!cs->dragging) return;
    cs->drag_x = fx;
    cs->drag_y = fy;
}

static void combat_touch_ended(combat_state *cs, float fx, float fy) {
    if (cs->phase == CPHASE_RESULT) return;

    if (!cs->dragging) return;
    cs->dragging = false;

    int card = cs->hand[cs->drag_card];
    int target_type = card_targets[card];
    float dy = cs->drag_start_y - fy;  /* positive = dragged upward */

    /* Must drag upward at least 30px to play */
    if (dy < 30.0f) return;

    bool played = false;
    if (target_type == TARGET_SELF) {
        combat_play_card(cs, cs->drag_card);
        played = true;
    } else if (target_type == TARGET_ALL_ENEMIES) {
        if (fy < 130.0f) {
            combat_play_card(cs, cs->drag_card);
            played = true;
        }
    } else {
        int hit_enemy = -1;
        int spacing = FB_WIDTH / (cs->enemy_count + 1);
        for (int i = 0; i < cs->enemy_count; i++) {
            if (!cs->enemies[i].alive) continue;
            int ecx = spacing * (i + 1);
            int escale = 3;
            if (cs->enemies[i].distance >= 4) escale = 1;
            else if (cs->enemies[i].distance >= 2) escale = 2;
            int esz = 16 * escale;
            int esy = 10 + cs->enemies[i].distance * 6;
            if (fx >= ecx - esz && fx <= ecx + esz && fy < esy + esz + 40) {
                hit_enemy = i;
                break;
            }
        }
        if (hit_enemy >= 0) {
            cs->target = hit_enemy;
            combat_play_card(cs, cs->drag_card);
            played = true;
        }
    }
    if (played) {
        combat_check_victory(cs);
        /* Auto-end turn when hand is empty */
        if (cs->phase == CPHASE_PLAYER_TURN && cs->hand_count <= 0)
            combat_action_end_turn(cs);
    }
}

static bool combat_handle_tap(combat_state *cs, float fx, float fy) {
    if (cs->phase == CPHASE_RESULT) return true;
    combat_touch_began(cs, fx, fy);
    return true;
}

/* ── Keyboard input ──────────────────────────────────────────────── */

static void combat_handle_key(combat_state *cs, int key) {
    /* Close pile viewer on Escape */
    if (cs->deck_view_open || cs->discard_view_open) {
        if (key == SAPP_KEYCODE_ESCAPE) {
            cs->deck_view_open = false;
            cs->discard_view_open = false;
            cs->pile_view_selected = -1;
        }
        return;
    }
    /* Log toggle and scroll (works in any phase) */
    if (key == SAPP_KEYCODE_L) {
        cs->log_open = !cs->log_open;
        cs->log_scroll = 0;
        return;
    }
    if (cs->log_open) {
        if (key == SAPP_KEYCODE_W || key == SAPP_KEYCODE_UP) {
            if (cs->log_scroll < cs->log_count - 5)
                cs->log_scroll++;
        }
        if (key == SAPP_KEYCODE_S || key == SAPP_KEYCODE_DOWN) {
            if (cs->log_scroll > 0)
                cs->log_scroll--;
        }
        if (key == SAPP_KEYCODE_ESCAPE) cs->log_open = false;
        return;
    }

    if (cs->phase == CPHASE_RESULT) return;
    if (cs->phase != CPHASE_PLAYER_TURN) return;

    switch (key) {
        case SAPP_KEYCODE_LEFT:
        case SAPP_KEYCODE_Q:
            if (cs->cursor > 0) cs->cursor--;
            break;
        case SAPP_KEYCODE_RIGHT:
        case SAPP_KEYCODE_E:
            if (cs->cursor < cs->hand_count - 1) cs->cursor++;
            break;
        case SAPP_KEYCODE_UP:
        case SAPP_KEYCODE_W: {
            int start = cs->target;
            do {
                cs->target = (cs->target + 1) % cs->enemy_count;
            } while (!cs->enemies[cs->target].alive && cs->target != start);
            break;
        }
        case SAPP_KEYCODE_DOWN:
        case SAPP_KEYCODE_S: {
            int start = cs->target;
            do {
                cs->target = (cs->target + cs->enemy_count - 1) % cs->enemy_count;
            } while (!cs->enemies[cs->target].alive && cs->target != start);
            break;
        }
        case SAPP_KEYCODE_TAB:
        case SAPP_KEYCODE_R:
            combat_action_end_turn(cs);
            break;
        case SAPP_KEYCODE_A:
            combat_action_move_back(cs);
            break;
        case SAPP_KEYCODE_D:
            combat_action_move_forward(cs);
            break;
    }
}

/* ── Rendering helpers ───────────────────────────────────────────── */

static void combat_draw_rect(uint32_t *px, int W, int H,
                             int x0, int y0, int w, int h, uint32_t col) {
    for (int ry = y0; ry < y0 + h && ry < H; ry++)
        for (int rx = x0; rx < x0 + w && rx < W; rx++)
            if (ry >= 0 && rx >= 0) px[ry * W + rx] = col;
}

static void combat_draw_rect_outline(uint32_t *px, int W, int H,
                                     int x0, int y0, int w, int h, uint32_t col) {
    for (int rx = x0; rx < x0 + w && rx < W; rx++) {
        if (rx >= 0) {
            if (y0 >= 0 && y0 < H) px[y0 * W + rx] = col;
            int yb = y0 + h - 1;
            if (yb >= 0 && yb < H) px[yb * W + rx] = col;
        }
    }
    for (int ry = y0; ry < y0 + h && ry < H; ry++) {
        if (ry >= 0) {
            if (x0 >= 0 && x0 < W) px[ry * W + x0] = col;
            int xr = x0 + w - 1;
            if (xr >= 0 && xr < W) px[ry * W + xr] = col;
        }
    }
}

static void combat_draw_bar(uint32_t *px, int W, int H,
                            int x, int y, int w, int h,
                            int val, int max_val, uint32_t fg, uint32_t bg) {
    combat_draw_rect(px, W, H, x, y, w, h, bg);
    if (max_val > 0) {
        int fill = (w * val) / max_val;
        if (fill > w) fill = w;
        if (fill > 0)
            combat_draw_rect(px, W, H, x, y, fill, h, fg);
    }
}

/* ── Card effect text (used by combat and deck viewer) ──────────── */

static const char *card_effect_text(int card_type) {
    switch (card_type) {
        case CARD_SHIELD:      return "+3 SHIELD";
        case CARD_SHOOT:       return "3 DMG";
        case CARD_BURST:       return "2 DMG ALL";
        case CARD_MOVE:        return "+2 MOVE";
        case CARD_MELEE:       return "6 DMG MELEE";
        case CARD_OVERCHARGE:  return "+2 ENERGY";
        case CARD_REPAIR:      return "+4 HP";
        case CARD_STUN:        return "SKIP ENEMY\nATTACKS";
        case CARD_FORTIFY:     return "+6 SHIELD";
        case CARD_DOUBLE_SHOT: return "5 DMG";
        case CARD_DASH:        return "+3 MOVE\n4 DMG";
        case CARD_ICE:         return "FREEZE 3T\nSLOW+DMG";
        case CARD_ACID:        return "STACK DOT\n1/STACK";
        case CARD_FIRE:        return "BURN 3T\nSPREADS";
        case CARD_LIGHTNING:   return "STUN 1-2T\n2 DMG";
        case CARD_SNIPER:      return "5 DMG\nDIST 2+";
        case CARD_SHOTGUN:     return "4 DMG\nDIST 0-1";
        case CARD_WELDER:      return "4 DMG\nMELEE";
        case CARD_CHAINSAW:    return "8 DMG\nMELEE";
        case CARD_LASER:       return "4 DMG\nPRECISION";
        case CARD_DEFLECTOR:   return "+4 SHIELD\nREFLECT DMG";
        case CARD_STUN_GUN:    return "STUN 1T\n1 DMG";
        case CARD_MICROWAVE:   return "5 DMG\n*3 DMG ALL";
        case CARD_QUICKSTEP:   return "+1 MOVE\nTO DISC";
        default:               return "";
    }
}

static const char *card_description_text(int card_type) {
    switch (card_type) {
        case CARD_SHIELD:      return "ADDS SHIELD POINTS\nTHAT ABSORB DAMAGE\nBEFORE HP.";
        case CARD_SHOOT:       return "BASIC RANGED ATTACK.\nWORKS AT ANY DISTANCE.";
        case CARD_BURST:       return "HITS ALL ENEMIES\nFOR REDUCED DAMAGE.";
        case CARD_MOVE:        return "GRANTS MOVEMENT\nPOINTS TO REPOSITION\nIN COMBAT.";
        case CARD_MELEE:       return "POWERFUL CLOSE RANGE\nATTACK. MUST BE\nADJACENT TO TARGET.";
        case CARD_OVERCHARGE:  return "GAIN EXTRA ENERGY\nTHIS TURN. COSTS\nNOTHING TO PLAY.";
        case CARD_REPAIR:      return "RESTORE HIT POINTS.\nHEALING PERSISTS\nBETWEEN COMBATS.";
        case CARD_STUN:        return "ALL ENEMIES SKIP\nTHEIR NEXT ATTACK\nPHASE.";
        case CARD_FORTIFY:     return "HEAVY SHIELD BOOST.\nGREAT FOR BRACING\nAGAINST BIG HITS.";
        case CARD_DOUBLE_SHOT: return "TWO SHOTS AT ONE\nTARGET FOR HIGH\nSINGLE-TARGET DMG.";
        case CARD_DASH:        return "MOVE AND STRIKE.\nGAIN MOVEMENT THEN\nDEAL DAMAGE.";
        case CARD_ICE:         return "FREEZES TARGET FOR\n3 TURNS. SLOWED AND\nTAKES DAMAGE OVER TIME.";
        case CARD_ACID:        return "STACKING POISON.\nEACH STACK ADDS +1\nDMG PER TURN.";
        case CARD_FIRE:        return "BURNING DAMAGE THAT\nSPREADS TO ADJACENT\nENEMIES.";
        case CARD_LIGHTNING:   return "CHANCE TO STUN FOR\n1-2 TURNS PLUS\nDIRECT DAMAGE.";
        case CARD_SNIPER:      return "HIGH DAMAGE BUT\nREQUIRES DISTANCE OF\n2 OR MORE.";
        case CARD_SHOTGUN:     return "CLOSE RANGE BLAST.\nONLY WORKS WITHIN\nDISTANCE 0-1.";
        case CARD_WELDER:      return "MELEE TOOL USED AS\nA WEAPON. MUST BE\nADJACENT.";
        case CARD_CHAINSAW:    return "DEVASTATING MELEE\nDAMAGE BUT COSTS\n2 ENERGY.";
        case CARD_LASER:       return "PRECISION ENERGY\nWEAPON. IGNORES\nSHIELD.";
        case CARD_DEFLECTOR:   return "SHIELD THAT REFLECTS\n1 DAMAGE BACK AT\nATTACKERS.";
        case CARD_STUN_GUN:    return "STUNS ONE ENEMY FOR\n1 TURN AND DEALS\nMINOR DAMAGE.";
        case CARD_MICROWAVE:   return "DEALS 5 DAMAGE. IF\nTHE TARGET DIES, ALL\nOTHER ENEMIES TAKE 3.";
        case CARD_QUICKSTEP:   return "GENERATES A MOVE\nCARD INTO YOUR\nDISCARD PILE.";
        default:               return "";
    }
}

/* ── Shared card rendering ───────────────────────────────────────── */

static void combat_draw_card_content(uint32_t *px, int W, int H,
                                     int cx, int cy, int cw, int ch,
                                     int card_type, bool selected,
                                     uint32_t shadow, int energy_available)
{
    bool affordable = (energy_available < 0 || energy_available >= card_energy_cost[card_type]);
    uint32_t white = affordable ? 0xFFFFFFFF : 0xFF555555;
    uint32_t gray = affordable ? 0xFF888888 : 0xFF444444;
    uint32_t yellow = 0xFF00DDDD;

    /* Background */
    uint32_t bg = selected ? 0xFF222233 : 0xFF111122;
    combat_draw_rect(px, W, H, cx, cy, cw, ch, bg);

    /* Card art sprite as background (centered, drawn dim behind text) */
    if (card_type < (int)(sizeof(spr_card_table)/sizeof(spr_card_table[0]))) {
        int scale = selected ? 3 : 2;
        int art_w = 16 * scale, art_h = 16 * scale;
        int art_x = cx + (cw - art_w) / 2;
        int art_y = cy + (ch - art_h) / 2;
        /* Draw sprite scaled, then dim it to serve as background */
        spr_draw(px, W, H, spr_card_table[card_type], art_x, art_y, scale);
        /* Darken the art so text is readable */
        uint32_t dim = affordable ? 0x44 : 0x22;
        for (int ry = cy + 1; ry < cy + ch - 1 && ry < H; ry++)
            for (int rx = cx + 1; rx < cx + cw - 1 && rx < W; rx++)
                if (rx >= 0 && ry >= 0) {
                    uint32_t p = px[ry * W + rx];
                    int pr = ((p >> 0) & 0xFF) * dim / 255;
                    int pg = ((p >> 8) & 0xFF) * dim / 255;
                    int pb = ((p >> 16) & 0xFF) * dim / 255;
                    px[ry * W + rx] = 0xFF000000 | (pb << 16) | (pg << 8) | pr;
                }
    }

    /* Border */
    uint32_t card_col = affordable ? card_colors[card_type] : 0xFF333333;
    uint32_t border = selected ? yellow : card_col;
    combat_draw_rect_outline(px, W, H, cx, cy, cw, ch, border);
    if (selected)
        combat_draw_rect_outline(px, W, H, cx+1, cy+1, cw-2, ch-2, border);

    /* Color stripe */
    combat_draw_rect(px, W, H, cx + 1, cy + 1, cw - 2, 3, card_col);

    /* Energy cost (top right) */
    {
        int cost = card_energy_cost[card_type];
        char cbuf[8];
        snprintf(cbuf, sizeof(cbuf), "%d", cost);
        uint32_t cost_color = affordable ? 0xFF22CCEE : 0xFF444466;
        sr_draw_text_shadow(px, W, H, cx + cw - 10, cy + 5, cbuf, cost_color, shadow);
    }

    /* Card name (word-wrapped, returns Y after last line) */
    int ty = sr_draw_text_wrap(px, W, H, cx + 3, cy + 5, card_names[card_type],
                               cw - 16, 8, white, shadow);

    /* Card effect text */
    const char *effect = card_effect_text(card_type);
    ty = sr_draw_text_wrap(px, W, H, cx + 3, ty + 2, effect,
                           cw - 6, 8, gray, shadow);

    /* Target type at bottom */
    const char *tgt = "";
    int tt = card_targets[card_type];
    if (tt == TARGET_SELF) tgt = "SELF";
    else if (tt == TARGET_ENEMY) tgt = "1 ENEMY";
    else tgt = "ALL";
    sr_draw_text_shadow(px, W, H, cx + 3, cy + ch - 10, tgt, 0xFF555555, shadow);
}

/* ── Pile viewer overlay (draw deck / discard pile browser) ─────── */

static void combat_draw_pile_viewer(uint32_t *px, int W, int H,
                                    int *pile, int pile_count, const char *title) {
    uint32_t shadow = 0xFF000000;

    /* Dim background */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        int r = ((c >> 0) & 0xFF) / 3;
        int g = ((c >> 8) & 0xFF) / 3;
        int b = ((c >> 16) & 0xFF) / 3;
        px[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }

    /* Title */
    char tbuf[32];
    snprintf(tbuf, sizeof(tbuf), "%s (%d)", title, pile_count);
    sr_draw_text_shadow(px, W, H, W/2 - 40, 8, tbuf, 0xFF00DDDD, shadow);

    if (pile_count == 0) {
        sr_draw_text_shadow(px, W, H, W/2 - 20, H/2, "EMPTY", 0xFF555555, shadow);
    } else {
        /* Grid: 5 columns */
        int cols = 5;
        int cw = 80, ch = 28;
        int pad = 4;
        int gridW = cols * (cw + pad) - pad;
        int startX = (W - gridW) / 2;
        int startY = 24;

        for (int i = 0; i < pile_count; i++) {
            int col = i % cols;
            int row = i / cols;
            int cx = startX + col * (cw + pad);
            int cy = startY + row * (ch + pad);
            if (cy > H - 40) break;  /* don't draw off screen */

            int card_type = pile[i];
            bool sel = (i == combat.pile_view_selected);
            uint32_t ccol = card_colors[card_type];
            uint32_t bg = sel ? 0xFF222244 : 0xFF111122;

            /* Background */
            combat_draw_rect(px, W, H, cx, cy, cw, ch, bg);
            /* Border */
            combat_draw_rect_outline(px, W, H, cx, cy, cw, ch, sel ? 0xFF00DDDD : ccol);
            /* Color stripe */
            combat_draw_rect(px, W, H, cx + 1, cy + 1, cw - 2, 2, ccol);

            /* Card name */
            sr_draw_text_shadow(px, W, H, cx + 3, cy + 4, card_names[card_type],
                                0xFFFFFFFF, shadow);
            /* Effect */
            const char *eff = card_effect_text(card_type);
            sr_draw_text_shadow(px, W, H, cx + 3, cy + 14, eff, 0xFF888888, shadow);
            /* Energy cost */
            char ebuf[4];
            snprintf(ebuf, sizeof(ebuf), "%d", card_energy_cost[card_type]);
            sr_draw_text_shadow(px, W, H, cx + cw - 10, cy + 4, ebuf, 0xFF22CCEE, shadow);

            /* Click detection */
            if (ui_mouse_clicked &&
                ui_click_x >= cx && ui_click_x < cx + cw &&
                ui_click_y >= cy && ui_click_y < cy + ch) {
                combat.pile_view_selected = (sel ? -1 : i);
            }
        }
    }

    /* Detail panel for selected card */
    if (combat.pile_view_selected >= 0 && combat.pile_view_selected < pile_count) {
        int card_type = pile[combat.pile_view_selected];
        uint32_t ccol = card_colors[card_type];

        int pw = 200, ph = 80;
        int px2 = (W - pw) / 2;
        int py = H - 20 - ph;

        combat_draw_rect(px, W, H, px2, py, pw, ph, 0xFF0A0A18);
        combat_draw_rect_outline(px, W, H, px2, py, pw, ph, ccol);
        combat_draw_rect(px, W, H, px2 + 1, py + 1, pw - 2, 2, ccol);

        sr_draw_text_shadow(px, W, H, px2 + 6, py + 5, card_names[card_type],
                            0xFFFFFFFF, shadow);
        char ebuf[8];
        snprintf(ebuf, sizeof(ebuf), "%dE", card_energy_cost[card_type]);
        sr_draw_text_shadow(px, W, H, px2 + pw - 20, py + 5, ebuf, 0xFF22CCEE, shadow);

        const char *tgt = "";
        int tt = card_targets[card_type];
        if (tt == TARGET_SELF) tgt = "TARGET: SELF";
        else if (tt == TARGET_ENEMY) tgt = "TARGET: 1 ENEMY";
        else tgt = "TARGET: ALL";
        sr_draw_text_shadow(px, W, H, px2 + 6, py + 15, tgt, 0xFF888888, shadow);

        const char *effect = card_effect_text(card_type);
        int ey = sr_draw_text_wrap(px, W, H, px2 + 6, py + 27, effect,
                                   pw - 12, 8, ccol, shadow);

        const char *desc = card_description_text(card_type);
        sr_draw_text_wrap(px, W, H, px2 + 6, ey + 4, desc,
                          pw - 12, 8, 0xFF666666, shadow);
    }

    /* Close button */
    if (ui_button(px, W, H, W/2 - 30, H - 16, 60, 14, "CLOSE",
                  0xFF111122, 0xFF222244, 0xFF333366)) {
        combat.deck_view_open = false;
        combat.discard_view_open = false;
        combat.pile_view_selected = -1;
    }
}

/* ── Main combat render ──────────────────────────────────────────── */

static void draw_combat_scene(sr_framebuffer *fb_ptr) {
    int W = fb_ptr->width, H = fb_ptr->height;
    uint32_t *px = fb_ptr->color;
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;
    uint32_t dim = 0xFF555555;
    uint32_t yellow = 0xFF00DDDD;

    /* Background - dark industrial */
    for (int i = 0; i < W * H; i++)
        px[i] = 0xFF0D0D11;

    /* Top bar - subtle divider line */
    for (int x = 0; x < W; x++)
        if (x < W) px[130 * W + x] = 0xFF222233;

    /* ── Draw enemies (top half) ──────────────────────────────── */
    if (combat.enemy_count > 0) {
        int spacing = W / (combat.enemy_count + 1);
        /* Draw enemies back-to-front (farthest first) so closer ones overlap */
        int draw_order[COMBAT_MAX_ENEMIES];
        for (int i = 0; i < combat.enemy_count; i++) draw_order[i] = i;
        /* Simple sort by distance descending */
        for (int i = 0; i < combat.enemy_count - 1; i++)
            for (int j = i + 1; j < combat.enemy_count; j++)
                if (combat.enemies[draw_order[i]].distance < combat.enemies[draw_order[j]].distance) {
                    int tmp = draw_order[i]; draw_order[i] = draw_order[j]; draw_order[j] = tmp;
                }

        for (int di = 0; di < combat.enemy_count; di++) {
            int i = draw_order[di];
            combat_enemy *e = &combat.enemies[i];
            int cx = spacing * (i + 1);

            /* Scale sprite by distance: dist 0 = scale 3, dist 5 = scale 1 */
            int scale = 3;
            if (e->distance >= 4) scale = 1;
            else if (e->distance >= 2) scale = 2;
            int spr_sz = 16 * scale;
            /* Position: farther enemies drawn higher (more distant) */
            int sprite_x = cx - spr_sz / 2;
            int sprite_y = 10 + e->distance * 6;

            if (e->alive) {
                const uint32_t *sprite = spr_enemy_table[e->type];

                /* Wiggle offset during this enemy's attack animation */
                int wiggle_x = 0;
                if (combat.phase == CPHASE_ENEMY_TURN &&
                    combat.enemy_atk_idx == i &&
                    combat.enemy_atk_timer > ENEMY_ATK_HIT_FRAME) {
                    wiggle_x = ((combat.enemy_atk_timer % 4) < 2) ? 3 : -3;
                }

                if (e->flash_timer > 0 && (e->flash_timer & 2))
                    spr_draw_flash(px, W, H, sprite, sprite_x + wiggle_x, sprite_y, scale);
                else
                    spr_draw(px, W, H, sprite, sprite_x + wiggle_x, sprite_y, scale);

                /* Target highlight (yellow border around selected enemy) */
                if (i == combat.target && combat.phase == CPHASE_PLAYER_TURN) {
                    combat_draw_rect_outline(px, W, H,
                        sprite_x - 2, sprite_y - 2, spr_sz + 4, spr_sz + 4, yellow);
                }

                /* Intent indicator (above sprite) */
                if (combat.phase == CPHASE_PLAYER_TURN || combat.phase == CPHASE_DRAW) {
                    bool stunned = (e->flash_timer > 10) || (e->lightning_stun > 0);
                    if (stunned) {
                        sr_draw_text_shadow(px, W, H, cx - 10, sprite_y - 10,
                                            "STUN", 0xFFCCCC22, shadow);
                    } else if (e->intent == INTENT_MOVE) {
                        sr_draw_text_shadow(px, W, H, cx - 6, sprite_y - 10,
                                            ">>", 0xFFCC8844, shadow);
                    } else if (e->intent == INTENT_ATTACK) {
                        int dmg = e->damage / 2; if (dmg < 1) dmg = 1;
                        if (e->ice_turns > 0) { dmg = dmg / 2; if (dmg < 1) dmg = 1; }
                        char intent_buf[16];
                        snprintf(intent_buf, sizeof(intent_buf), "%d", dmg);
                        sr_draw_text_shadow(px, W, H, cx - 4, sprite_y - 10,
                                            intent_buf, 0xFF4444FF, shadow);
                    }
                }
            } else {
                int sprite_y_dead = 10 + e->distance * 6;
                sr_draw_text_shadow(px, W, H, cx - 12, sprite_y_dead + 12, "DEAD", 0xFF444444, shadow);
            }

            /* Info below sprite */
            int info_y = sprite_y + spr_sz + 2;

            /* Enemy name + weakness indicator */
            {
                int etype = e->type;
                bool wk_known = g_weakness.initialized && g_weakness.weakness_known[etype];
                const char *ename = enemy_templates[etype].name;
                /* Draw name */
                sr_draw_text_shadow(px, W, H, cx - 18, info_y, ename,
                                    e->alive ? white : 0xFF444444, shadow);
                /* After name: show element icon if known, "?" if not */
                int name_end_x = cx - 18 + (int)strlen(ename) * 6 + 2;
                if (e->alive && wk_known) {
                    int wk_elem = g_weakness.weakness[etype];
                    spr_draw_tex(px, W, H, &stextures[elem_icon_stex[wk_elem]],
                                 name_end_x, info_y - 3, 1);
                } else if (e->alive) {
                    sr_draw_text_shadow(px, W, H, name_end_x, info_y,
                                        "?", 0xFF888888, shadow);
                }
            }

            /* HP bar + distance */
            if (e->alive) {
                combat_draw_bar(px, W, H, cx - 18, info_y + 10, 36, 4,
                                e->hp, e->hp_max, 0xFF2222CC, 0xFF333333);
                char hpbuf[16];
                snprintf(hpbuf, sizeof(hpbuf), "%d/%d", e->hp, e->hp_max);
                sr_draw_text_shadow(px, W, H, cx - 10, info_y + 16, hpbuf, gray, shadow);

                /* Distance + range indicator */
                char distbuf[16];
                snprintf(distbuf, sizeof(distbuf), "D:%d R:%d", e->distance, e->attack_range);
                sr_draw_text_shadow(px, W, H, cx - 16, info_y + 26, distbuf, dim, shadow);

                /* Status effect indicators */
                int sx = cx - 18;
                int sy = info_y + 36;
                if (e->ice_turns > 0) {
                    char ibuf[8]; snprintf(ibuf, sizeof(ibuf), "I%d", e->ice_turns);
                    sr_draw_text_shadow(px, W, H, sx, sy, ibuf, 0xFFFFCC44, shadow);
                    sx += 16;
                }
                if (e->acid_stacks > 0) {
                    char abuf[8]; snprintf(abuf, sizeof(abuf), "A%d", e->acid_stacks);
                    sr_draw_text_shadow(px, W, H, sx, sy, abuf, 0xFF22CCAA, shadow);
                    sx += 16;
                }
                if (e->fire_turns > 0) {
                    char fbuf[8]; snprintf(fbuf, sizeof(fbuf), "F%d", e->fire_turns);
                    sr_draw_text_shadow(px, W, H, sx, sy, fbuf, 0xFF2244FF, shadow);
                    sx += 16;
                }
                if (e->lightning_stun > 0) {
                    char lbuf[8]; snprintf(lbuf, sizeof(lbuf), "L%d", e->lightning_stun);
                    sr_draw_text_shadow(px, W, H, sx, sy, lbuf, 0xFFEEEE44, shadow);
                }
            }
        }
    }

    /* ── Distance dots (player-to-enemy range visualization) ──── */
    if (combat.phase == CPHASE_PLAYER_TURN || combat.phase == CPHASE_DRAW) {
        int spacing = W / (combat.enemy_count + 1);
        /* Player icon X for dot line start */
        int player_x = 24;  /* near player sprite in bottom-left */
        int dot_y = 130;    /* horizontal band in mid-screen */
        for (int i = 0; i < combat.enemy_count; i++) {
            combat_enemy *e = &combat.enemies[i];
            if (!e->alive) continue;
            int ecx = spacing * (i + 1);

            /* Draw dots from player to enemy position */
            int dist = e->distance;
            int dot_total = dist + 1;  /* +1 for the player's position */
            int dot_spacing = 12;
            int dots_w = (dot_total - 1) * dot_spacing;
            int dx0 = ecx - dots_w / 2;

            for (int d = 0; d < dot_total; d++) {
                int dx = dx0 + d * dot_spacing;
                uint32_t dc;
                if (d == 0)
                    dc = 0xFF44FF44;  /* player dot: green */
                else if (d <= e->attack_range)
                    dc = 0xFF4444FF;  /* in attack range: red */
                else
                    dc = 0xFF555555;  /* out of range: gray */
                combat_draw_rect(px, W, H, dx, dot_y, 4, 4, dc);
            }
            /* P and E labels */
            sr_draw_text_shadow(px, W, H, dx0 - 2, dot_y - 8, "P", 0xFF44FF44, shadow);
            if (dist > 0) {
                int ex = dx0 + dist * dot_spacing;
                uint32_t ecol = (i == combat.target) ? yellow : 0xFFCC4444;
                sr_draw_text_shadow(px, W, H, ex - 2, dot_y - 8, "E", ecol, shadow);
            }
        }
    }

    /* ── Player info (bottom left) ────────────────────────────── */
    {
        const char_class *cc = &char_classes[combat.player_class];

        /* Player sprite (flicker when hit) */
        bool show_player = true;
        if (combat.player_flash_timer > 0 && (combat.player_flash_timer & 2))
            show_player = false; /* skip drawing = flicker effect */

        if (show_player)
            spr_draw(px, W, H, cc->sprite, 8, 140, 2);

        /* Red tint on visible flicker frames */
        if (combat.player_flash_timer > 0 && show_player) {
            for (int ry = 140; ry < 140 + 32 && ry < H; ry++)
                for (int rx = 8; rx < 8 + 32 && rx < W; rx++) {
                    uint32_t c = px[ry * W + rx];
                    if ((c & 0x00FFFFFF) != (0xFF0D0D11 & 0x00FFFFFF)) {
                        /* Tint toward red */
                        int r = (c >> 0) & 0xFF;
                        int g = (c >> 8) & 0xFF;
                        int b = (c >> 16) & 0xFF;
                        r = r + (255 - r) / 2;
                        g = g / 2;
                        b = b / 2;
                        px[ry * W + rx] = 0xFF000000 | (b << 16) | (g << 8) | r;
                    }
                }
        }

        /* Blue shield sphere when shield absorbs damage */
        if (combat.player_shield_flash_timer > 0) {
            int scx = 8 + 16, scy = 140 + 16; /* center of player sprite */
            int radius = 18;
            int alpha_fade = combat.player_shield_flash_timer * 6; /* 0..120 */
            if (alpha_fade > 120) alpha_fade = 120;
            for (int ry = scy - radius; ry <= scy + radius; ry++) {
                for (int rx = scx - radius; rx <= scx + radius; rx++) {
                    if (rx < 0 || rx >= W || ry < 0 || ry >= H) continue;
                    int dx = rx - scx, dy = ry - scy;
                    int dist2 = dx * dx + dy * dy;
                    int r2 = radius * radius;
                    if (dist2 > r2) continue;
                    /* Only draw shell (outer 40%) for sphere look */
                    int inner_r2 = (radius * 6 / 10) * (radius * 6 / 10);
                    if (dist2 < inner_r2) continue;
                    uint32_t c = px[ry * W + rx];
                    int cr = (c >> 0) & 0xFF;
                    int cg = (c >> 8) & 0xFF;
                    int cb = (c >> 16) & 0xFF;
                    /* Blend toward blue (ABGR: high B channel = bits 16-23) */
                    int a = alpha_fade;
                    cr = (cr * (255 - a) + 80 * a) / 255;
                    cg = (cg * (255 - a) + 160 * a) / 255;
                    cb = (cb * (255 - a) + 255 * a) / 255;
                    px[ry * W + rx] = 0xFF000000 | (cb << 16) | (cg << 8) | cr;
                }
            }
        }

        /* Class name */
        sr_draw_text_shadow(px, W, H, 8, 175, cc->name, white, shadow);

        /* HP bar */
        sr_draw_text_shadow(px, W, H, 8, 186, "HP", 0xFF4444FF, shadow);
        combat_draw_bar(px, W, H, 24, 186, 50, 6,
                        combat.player_hp, combat.player_hp_max, 0xFF2222CC, 0xFF333333);
        char hpbuf[16];
        snprintf(hpbuf, sizeof(hpbuf), "%d/%d", combat.player_hp, combat.player_hp_max);
        sr_draw_text_shadow(px, W, H, 28, 195, hpbuf, gray, shadow);

        /* Shield bar */
        if (combat.player_shield > 0) {
            sr_draw_text_shadow(px, W, H, 8, 206, "SH", 0xFFCCCC22, shadow);
            char shbuf[16];
            snprintf(shbuf, sizeof(shbuf), "%d", combat.player_shield);
            sr_draw_text_shadow(px, W, H, 28, 206, shbuf, 0xFFCCCC22, shadow);
        }

        /* Energy display */
        {
            char ebuf[16];
            snprintf(ebuf, sizeof(ebuf), "NRG %d/%d", combat.energy, combat.energy_max);
            uint32_t ecol = combat.energy > 0 ? 0xFF22CCEE : 0xFF444444;
            sr_draw_text_shadow(px, W, H, 8, 218, ebuf, ecol, shadow);
        }

        /* Move points display */
        if (combat.player_move_pts > 0) {
            char mpbuf[16];
            snprintf(mpbuf, sizeof(mpbuf), "MP  %d", combat.player_move_pts);
            sr_draw_text_shadow(px, W, H, 8, 228, mpbuf, 0xFF22CC22, shadow);
        }

        /* Fire attack bonus */
        if (combat.fire_atk_bonus > 0) {
            char fbuf[16];
            snprintf(fbuf, sizeof(fbuf), "ATK+%d", combat.fire_atk_bonus);
            sr_draw_text_shadow(px, W, H, 60, 218, fbuf, 0xFF2244FF, shadow);
        }
    }

    /* ── Move buttons (when player has move points) ──────────── */
    if (combat.phase == CPHASE_PLAYER_TURN && combat.player_move_pts > 0) {
        int mb_y = 238;
        /* Forward button */
        combat_draw_rect(px, W, H, 8, mb_y, 34, 14, 0xFF223322);
        combat_draw_rect_outline(px, W, H, 8, mb_y, 34, 14, 0xFF44CC44);
        sr_draw_text_shadow(px, W, H, 11, mb_y + 3, "FWD", 0xFF44CC44, shadow);
        /* Back button */
        combat_draw_rect(px, W, H, 46, mb_y, 34, 14, 0xFF332222);
        combat_draw_rect_outline(px, W, H, 46, mb_y, 34, 14, 0xFFCC6644);
        sr_draw_text_shadow(px, W, H, 50, mb_y + 3, "BCK", 0xFFCC6644, shadow);
    }

    /* ── Hand of cards (bottom, fanned) ──────────────────────── */
    {
        int sel_idx = (combat.phase == CPHASE_PLAYER_TURN) ? combat.cursor : -1;

        /* Draw non-selected cards left-to-right, then selected on top */
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < combat.hand_count; i++) {
                bool selected = (i == sel_idx);
                if (pass == 0 && selected) continue;
                if (pass == 1 && !selected) continue;
                int card = combat.hand[i];
                float fcx, fcy, fangle;
                combat_card_fan_pos(&combat, i, &fcx, &fcy, &fangle);
                /* Render card to temp buffer */
                memset(card_fan_buf, 0, sizeof(card_fan_buf));
                combat_draw_card_content(card_fan_buf, CARD_FAN_W, CARD_FAN_H,
                                         0, 0, CARD_FAN_W, CARD_FAN_H,
                                         card, selected, shadow, combat.energy);
                /* Blit rotated to framebuffer */
                combat_blit_card(px, W, H, card_fan_buf, CARD_FAN_W, CARD_FAN_H,
                                fcx, fcy, fangle);
            }
        }
    }

    /* ── Drag visualization ───────────────────────────────────── */
    if (combat.dragging && combat.drag_card < combat.hand_count) {
        int card = combat.hand[combat.drag_card];
        int target_type = card_targets[card];

        /* Draw line from card to drag position */
        float fcx, fcy, fangle;
        combat_card_fan_pos(&combat, combat.drag_card, &fcx, &fcy, &fangle);
        int line_x0 = (int)fcx;
        int line_y0 = (int)(fcy - CARD_FAN_H);
        int line_x1 = (int)combat.drag_x;
        int line_y1 = (int)combat.drag_y;

        /* Simple line using Bresenham */
        {
            int dx = line_x1 - line_x0; if (dx < 0) dx = -dx;
            int ddy = line_y1 - line_y0; if (ddy < 0) ddy = -ddy;
            int sx2 = line_x0 < line_x1 ? 1 : -1;
            int sy2 = line_y0 < line_y1 ? 1 : -1;
            int err = dx - ddy;
            int lx = line_x0, ly = line_y0;
            uint32_t line_col = card_colors[card] | 0xFF000000;
            for (int step = 0; step < 500; step++) {
                if (lx >= 0 && lx < W && ly >= 0 && ly < H)
                    px[ly * W + lx] = line_col;
                if (lx == line_x1 && ly == line_y1) break;
                int e2 = 2 * err;
                if (e2 > -ddy) { err -= ddy; lx += sx2; }
                if (e2 < dx) { err += dx; ly += sy2; }
            }
        }

        /* Draw targeting reticle at drag position */
        uint32_t ret_col = 0xFFFFFFFF;
        int rx = line_x1, ry = line_y1;
        combat_draw_rect_outline(px, W, H, rx - 6, ry - 6, 12, 12, ret_col);

        /* Highlight valid drop zone */
        if (target_type == TARGET_SELF) {
            /* Highlight player area */
            combat_draw_rect_outline(px, W, H, 4, 136, 80, 80, 0xFF44FF44);
            sr_draw_text_shadow(px, W, H, 12, 220, "DROP ON SELF", 0xFF44FF44, shadow);
        } else if (target_type == TARGET_ALL_ENEMIES) {
            /* Highlight all enemies zone */
            if (combat.drag_y < 130.0f)
                combat_draw_rect_outline(px, W, H, 10, 10, W - 20, 120, 0xFF5588FF);
            sr_draw_text_shadow(px, W, H, W/2 - 35, 95, "HIT ALL", 0xFF5588FF, shadow);
        } else {
            /* Highlight targeted enemy */
            int espacing = W / (combat.enemy_count + 1);
            int elem = card_element(card);
            for (int i = 0; i < combat.enemy_count; i++) {
                if (!combat.enemies[i].alive) continue;
                int ecx = espacing * (i + 1);
                int escale = 3;
                if (combat.enemies[i].distance >= 4) escale = 1;
                else if (combat.enemies[i].distance >= 2) escale = 2;
                int esz = 16 * escale;
                int esy = 10 + combat.enemies[i].distance * 6;
                if (combat.drag_x >= ecx - esz && combat.drag_x <= ecx + esz && combat.drag_y < esy + esz + 40) {
                    /* Check weakness for elemental cards */
                    bool is_weak = (elem >= 0 && g_weakness.initialized &&
                                    g_weakness.weakness_known[combat.enemies[i].type] &&
                                    weakness_check(combat.enemies[i].type, elem));
                    uint32_t highlight_col = is_weak ? 0xFF00FFFF : yellow;
                    combat_draw_rect_outline(px, W, H, ecx - esz/2 - 4, esy - 4, esz + 8, esz + 50, highlight_col);
                    if (is_weak) {
                        /* Show WEAKNESS in bold (double-draw for bold effect) */
                        sr_draw_text_shadow(px, W, H, ecx - 22, esy - 18,
                                            "WEAKNESS!", 0xFF00FFFF, shadow);
                        sr_draw_text_shadow(px, W, H, ecx - 21, esy - 18,
                                            "WEAKNESS!", 0xFF00FFFF, shadow);
                        /* Show 2x damage preview */
                        int base_dmg = (card == CARD_LIGHTNING) ? 2 : 1;
                        char dmgbuf[16];
                        snprintf(dmgbuf, sizeof(dmgbuf), "-%d x2!", base_dmg * 2);
                        sr_draw_text_shadow(px, W, H, ecx - 12, esy + esz + 42,
                                            dmgbuf, 0xFF00FFFF, shadow);
                    }
                }
            }
        }
    }

    /* ── Enemy info popup (weakness details) ──────────────────── */
    if (combat.info_popup_enemy >= 0 && combat.info_popup_enemy < combat.enemy_count
        && combat.enemies[combat.info_popup_enemy].alive) {
        combat_enemy *pe = &combat.enemies[combat.info_popup_enemy];
        int etype = pe->type;

        /* Popup box */
        int pw = 120, ph = 70;
        int ppx = (W - pw) / 2;
        int ppy = 100;

        /* Dark background */
        combat_draw_rect(px, W, H, ppx, ppy, pw, ph, 0xEE111122);
        combat_draw_rect_outline(px, W, H, ppx, ppy, pw, ph, 0xFFCCCCCC);

        /* Enemy name */
        sr_draw_text_shadow(px, W, H, ppx + 4, ppy + 4,
                            enemy_templates[etype].name, white, shadow);

        /* Weakness info */
        if (g_weakness.initialized && g_weakness.weakness_known[etype]) {
            int wk = g_weakness.weakness[etype];
            /* Draw element icon */
            spr_draw_tex(px, W, H, &stextures[elem_icon_stex[wk]],
                         ppx + pw - 20, ppy + 4, 1);
            char wbuf[32];
            snprintf(wbuf, sizeof(wbuf), "WEAK: %s", elem_names[wk]);
            /* Draw twice offset for bold effect */
            sr_draw_text_shadow(px, W, H, ppx + 4, ppy + 16, wbuf, elem_colors[wk], shadow);
            sr_draw_text_shadow(px, W, H, ppx + 5, ppy + 16, wbuf, elem_colors[wk], shadow);
            sr_draw_text_shadow(px, W, H, ppx + 4, ppy + 28,
                                "2x DMG!", 0xFF00FFFF, shadow);
        } else {
            sr_draw_text_shadow(px, W, H, ppx + 4, ppy + 16,
                                "WEAKNESS: ???", 0xFF888888, shadow);
            sr_draw_text_shadow(px, W, H, ppx + 4, ppy + 28,
                                "Use elemental", 0xFF666666, shadow);
            sr_draw_text_shadow(px, W, H, ppx + 4, ppy + 38,
                                "cards to find!", 0xFF666666, shadow);
        }

        /* Show which elements have been tested */
        int tx = ppx + 4;
        int ty = ppy + 52;
        for (int el = 0; el < ELEM_COUNT; el++) {
            const char *eshort[] = { "I", "A", "F", "L" };
            if (g_weakness.discovered[etype][el]) {
                bool is_weak = (g_weakness.weakness[etype] == el);
                uint32_t c = is_weak ? elem_colors[el] : 0xFF555555;
                sr_draw_text_shadow(px, W, H, tx, ty, eshort[el], c, shadow);
            } else {
                sr_draw_text_shadow(px, W, H, tx, ty, "?", 0xFF444444, shadow);
            }
            tx += 14;
        }
    }

    /* ── Turn info (top right) ────────────────────────────────── */
    {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "TURN %d", combat.turn);
        sr_draw_text_shadow(px, W, H, W - 50, 4, tbuf, gray, shadow);

        char dbuf[16];
        snprintf(dbuf, sizeof(dbuf), "DRAW:%d", combat.deck_count);
        sr_draw_text_shadow(px, W, H, W - 50, 14, dbuf, dim, shadow);

        char rbuf[16];
        snprintf(rbuf, sizeof(rbuf), "DISC:%d", combat.discard_count);
        sr_draw_text_shadow(px, W, H, W - 50, 24, rbuf, dim, shadow);

        /* Click DRAW to browse draw pile */
        if (ui_mouse_clicked &&
            ui_click_x >= W - 50 && ui_click_x < W &&
            ui_click_y >= 14 && ui_click_y < 24) {
            combat.deck_view_open = true;
            combat.discard_view_open = false;
            combat.pile_view_selected = -1;
        }
        /* Click DISC to browse discard pile */
        if (ui_mouse_clicked &&
            ui_click_x >= W - 50 && ui_click_x < W &&
            ui_click_y >= 24 && ui_click_y < 34) {
            combat.discard_view_open = true;
            combat.deck_view_open = false;
            combat.pile_view_selected = -1;
        }
    }

    /* ── Action buttons (touch-friendly) ─────────────────────── */
    if (combat.phase == CPHASE_PLAYER_TURN) {
        /* END TURN button */
        combat_draw_rect(px, W, H, BTN_END_X, BTN_END_Y, BTN_END_W, BTN_END_H, 0xFF332222);
        combat_draw_rect_outline(px, W, H, BTN_END_X, BTN_END_Y, BTN_END_W, BTN_END_H, 0xFFCC6644);
        sr_draw_text_shadow(px, W, H, BTN_END_X + 4, BTN_END_Y + 5,
                            "END TURN", 0xFFCC6644, shadow);
    }

    /* ── Message ──────────────────────────────────────────────── */
    if (combat.message_timer > 0) {
        uint32_t mc = white;
        if (combat.message_timer < 15)
            mc = 0xFF888888; /* fade out */
        sr_draw_text_shadow(px, W, H, W/2 - 40, 98, combat.message, mc, shadow);
    }

    /* ── Phase indicators ─────────────────────────────────────── */
    if (combat.phase == CPHASE_DRAW) {
        sr_draw_text_shadow(px, W, H, W/2 - 30, H/2, "DRAWING...", yellow, shadow);
    }
    if (combat.phase == CPHASE_ENEMY_TURN) {
        sr_draw_text_shadow(px, W, H, W/2 - 40, H/2, "ENEMY TURN...", 0xFF4444FF, shadow);
    }

    /* ── Reward screen (pick 1 of 3 cards) ──────────────────── */
    if (combat.phase == CPHASE_REWARD) {
        /* Darken background */
        for (int i = 0; i < W * H; i++) {
            uint32_t c = px[i];
            px[i] = ((c >> 24) << 24) | ((((c>>16)&0xFF)>>1)<<16) |
                    ((((c>>8)&0xFF)>>1)<<8) | (((c)&0xFF)>>1);
        }
        sr_draw_text_shadow(px, W, H, W/2 - 35, 30, "VICTORY!", 0xFF00FF00, shadow);
        sr_draw_text_shadow(px, W, H, W/2 - 55, 50, "PICK A CARD REWARD", yellow, shadow);

        int rw = 72, rh = 80, rgap = 12;
        int rtotal = 3 * (rw + rgap) - rgap;
        int rbase_x = (W - rtotal) / 2;
        int rbase_y = 75;

        for (int i = 0; i < 3; i++) {
            int rc = combat.reward_choices[i];
            int rx = rbase_x + i * (rw + rgap);
            int ry = rbase_y;
            bool rsel = (i == combat.reward_cursor);
            combat_draw_card_content(px, W, H, rx, ry, rw, rh,
                                     rc, rsel, shadow, -1);
        }

        sr_draw_text_shadow(px, W, H, W/2 - 35, rbase_y + rh + 10,
                            "TAP TO PICK", gray, shadow);
    }

    /* ── Result screen (defeat) ──────────────────────────────── */
    if (combat.phase == CPHASE_RESULT) {
        for (int i = 0; i < W * H; i++) {
            uint32_t c = px[i];
            px[i] = ((c >> 24) << 24) | ((((c>>16)&0xFF)>>1)<<16) |
                    ((((c>>8)&0xFF)>>1)<<8) | (((c)&0xFF)>>1);
        }
        sr_draw_text_shadow(px, W, H, W/2 - 30, H/2 - 20, "DEFEATED", 0xFF0000FF, shadow);
        int cb_x = W/2 - 40, cb_y = H/2 + 5, cb_w = 80, cb_h = 20;
        combat_draw_rect(px, W, H, cb_x, cb_y, cb_w, cb_h, 0xFF333333);
        combat_draw_rect_outline(px, W, H, cb_x, cb_y, cb_w, cb_h, white);
        sr_draw_text_shadow(px, W, H, cb_x + 12, cb_y + 6, "CONTINUE", white, shadow);
    }

    /* ── LOG button (always visible) ─────────────────────────── */
    {
        int lb_x = W - 32, lb_y = 38;
        combat_draw_rect(px, W, H, lb_x, lb_y, 28, 12, 0xFF1A1A22);
        combat_draw_rect_outline(px, W, H, lb_x, lb_y, 28, 12, 0xFF666688);
        sr_draw_text_shadow(px, W, H, lb_x + 4, lb_y + 2, "LOG", 0xFF888899, shadow);
    }

    /* ── Log overlay ─────────────────────────────────────────── */
    if (combat.log_open) {
        /* Semi-transparent dark background */
        for (int i = 0; i < W * H; i++) {
            uint32_t c = px[i];
            px[i] = ((c >> 24) << 24) |
                    ((((c>>16)&0xFF)/3)<<16) |
                    ((((c>>8)&0xFF)/3)<<8) |
                    (((c)&0xFF)/3);
        }

        /* Log panel */
        int lx = 20, ly = 10, lw = W - 40, lh = H - 20;
        combat_draw_rect(px, W, H, lx, ly, lw, lh, 0xFF0A0A12);
        combat_draw_rect_outline(px, W, H, lx, ly, lw, lh, 0xFF4444AA);
        combat_draw_rect_outline(px, W, H, lx+1, ly+1, lw-2, lh-2, 0xFF222244);

        sr_draw_text_shadow(px, W, H, lx + 4, ly + 4, "COMBAT LOG", yellow, shadow);
        sr_draw_text_shadow(px, W, H, lx + lw - 40, ly + 4, "[X]", 0xFFCC4444, shadow);

        /* Scroll indicators */
        int visible_lines = (lh - 24) / 10;
        int start = combat.log_count - visible_lines - combat.log_scroll;
        if (start < 0) start = 0;
        int end = start + visible_lines;
        if (end > combat.log_count) end = combat.log_count;

        for (int li = start; li < end; li++) {
            int row_y = ly + 18 + (li - start) * 10;
            uint32_t col = gray;
            /* Color-code: turn headers = yellow, indented = dim, attacks = red */
            if (combat.log[li][0] == '-' && combat.log[li][1] == '-')
                col = yellow;
            else if (combat.log[li][0] == ' ' && combat.log[li][1] == ' ')
                col = dim;
            else if (strstr(combat.log[li], "attack") || strstr(combat.log[li], "SABOTAGE"))
                col = 0xFF6666FF;
            sr_draw_text_shadow(px, W, H, lx + 6, row_y, combat.log[li], col, shadow);
        }

        /* Scroll bar hint */
        if (combat.log_count > visible_lines) {
            char sbuf[16];
            snprintf(sbuf, sizeof(sbuf), "W/S SCROLL");
            sr_draw_text_shadow(px, W, H, lx + lw/2 - 25, ly + lh - 12, sbuf, dim, shadow);
        }
    }

    /* ── Pile viewer overlays ────────────────────────────────── */
    if (combat.deck_view_open) {
        combat_draw_pile_viewer(px, W, H, combat.deck, combat.deck_count, "DRAW PILE");
    }
    if (combat.discard_view_open) {
        combat_draw_pile_viewer(px, W, H, combat.discard, combat.discard_count, "DISCARD PILE");
    }
}

#endif /* SR_COMBAT_H */
