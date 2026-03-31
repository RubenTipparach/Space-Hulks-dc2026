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
    CARD_DEFLECTOR,  /* deflector: +4 shield, reflect 1 dmg, cost 1 */
    CARD_STUN_GUN,   /* stun gun: stun 1 enemy 1 turn, 1 dmg, cost 1 */
    CARD_TYPE_COUNT
};

static const char *card_names[] = {
    "SHIELD", "SHOOT", "BURST", "MOVE", "MELEE",
    "OVERCHRG", "REPAIR", "STUN", "FORTIFY", "DBL SHOT", "DASH",
    "ICE", "ACID", "FIRE", "LIGHTNING",
    "SNIPER", "SHOTGUN", "WELDER", "CHAINSAW", "LASER", "DEFLECTR", "STUN GUN"
};

static const uint32_t card_colors[] = {
    0xFFCC8822,  /* shield - blue */
    0xFF2222CC,  /* shoot - red */
    0xFF2266FF,  /* burst - orange */
    0xFF22CC22,  /* move - green */
    0xFF22CCCC,  /* melee - yellow */
    0xFFEECC22,  /* overcharge - cyan */
    0xFF22CC88,  /* repair - teal */
    0xFFCC22CC,  /* stun - magenta */
    0xFFFFAA22,  /* fortify - bright blue */
    0xFF4444FF,  /* double shot - bright red */
    0xFF44CCCC,  /* dash - bright yellow */
    0xFFFFCC44,  /* ice - light blue */
    0xFF22CCAA,  /* acid - green-teal */
    0xFF2244FF,  /* fire - bright orange-red */
    0xFFEEEE44,  /* lightning - bright cyan-yellow */
    0xFF448844,  /* sniper - dark green */
    0xFF2288DD,  /* shotgun - warm orange */
    0xFF44AAFF,  /* welder - bright orange */
    0xFF2244CC,  /* chainsaw - dark red */
    0xFFFFFF44,  /* laser - bright cyan */
    0xFFDDAA44,  /* deflector - steel blue */
    0xFFCC88FF,  /* stun gun - light purple */
};

static const int card_energy_cost[] = {
    1, 1, 2, 1, 1,  /* base cards */
    0, 2, 1, 2, 2, 2, /* droppable cards */
    1, 1, 1, 2, /* elemental cards */
    1, 1, 1, 2, 1, 1, 1 /* class-specific: sniper, shotgun, welder, chainsaw, laser, deflector, stun gun */
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
        /* 2 shield, 0 shoot, 1 burst, 3 move, 0 melee, ..., 2 sniper, 2 shotgun */
        .deck_composition = { 2, 0, 1, 3, 0, 0,0,0,0,0,0, 0,0,0,0, 2, 2 },
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
        /* 1 shield, 1 shoot, 1 burst, 1 move, 0 melee, ..., laser 2, deflector 2, stun_gun 2 */
        .deck_composition = { 1, 1, 1, 1, 0, 0,0,0,0,0,0, 0,0,0,0, 0,0,0,0, 2, 2, 2 },
        .name = "SCIENTIST",
        .sprite = spr_scientist,
    },
};

/* ── Enemy types ─────────────────────────────────────────────────── */

enum { ENEMY_LURKER, ENEMY_BRUTE, ENEMY_SPITTER, ENEMY_HIVEGUARD, ENEMY_TYPE_COUNT };

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
    [ENEMY_LURKER]    = { "LURKER",     8,  2, 0, 0 },  /* melee: must be at dist 0 */
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
    int player_distance;  /* steps to enemies (0 = melee range) */
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
    cs->player_distance = 3;
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
        bool in_range = (cs->player_distance <= e->attack_range);
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

        case CARD_MELEE:
            if (cs->player_distance <= 0) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    int dmg = 6 + cs->fire_atk_bonus;
                    combat_deal_damage_enemy(cs, t, dmg);
                    snprintf(buf, sizeof(buf), "MELEE %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", cs->player_distance);
                combat_set_message(cs, buf);
                cs->energy += cost; /* refund */
                return; /* don't consume card */
            }
            break;

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
                combat_deal_damage_enemy(cs, t, 1);
                snprintf(buf, sizeof(buf), "ICE %s! FROZEN 3T", enemy_templates[cs->enemies[t].type].name);
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
                combat_deal_damage_enemy(cs, t, 1);
                snprintf(buf, sizeof(buf), "ACID %s x%d!", enemy_templates[cs->enemies[t].type].name, cs->enemies[t].acid_stacks);
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
                combat_deal_damage_enemy(cs, t, 1);
                snprintf(buf, sizeof(buf), "FIRE %s! BURN 3T", enemy_templates[cs->enemies[t].type].name);
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
                combat_deal_damage_enemy(cs, t, 2);
                snprintf(buf, sizeof(buf), "ZAP %s! STUN %dT", enemy_templates[cs->enemies[t].type].name, stun_turns);
                combat_set_message(cs, buf);
            }
            break;
        }

        /* ── Class-specific cards ───────────────────────────── */
        case CARD_SNIPER:
            if (cs->player_distance >= 2) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    int dmg = 5 + cs->fire_atk_bonus;
                    combat_deal_damage_enemy(cs, t, dmg);
                    snprintf(buf, sizeof(buf), "SNIPE %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "TOO CLOSE! DIST: %d (NEED 2+)", cs->player_distance);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;

        case CARD_SHOTGUN:
            if (cs->player_distance <= 1) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    int dmg = 4 + cs->fire_atk_bonus;
                    combat_deal_damage_enemy(cs, t, dmg);
                    snprintf(buf, sizeof(buf), "SHOTGUN %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d (NEED 1-)", cs->player_distance);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;

        case CARD_WELDER:
            if (cs->player_distance <= 0) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    int dmg = 4 + cs->fire_atk_bonus;
                    combat_deal_damage_enemy(cs, t, dmg);
                    snprintf(buf, sizeof(buf), "WELD %s -%dHP!", enemy_templates[cs->enemies[t].type].name, dmg);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", cs->player_distance);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;

        case CARD_CHAINSAW:
            if (cs->player_distance <= 0) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    int dmg = 8 + cs->fire_atk_bonus;
                    combat_deal_damage_enemy(cs, t, dmg);
                    snprintf(buf, sizeof(buf), "CHAINSAW %s -%dHP!!", enemy_templates[cs->enemies[t].type].name, dmg);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", cs->player_distance);
                combat_set_message(cs, buf);
                cs->energy += cost;
                return;
            }
            break;

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
            /* Reflect 1 damage to all alive enemies */
            for (int i = 0; i < cs->enemy_count; i++) {
                if (cs->enemies[i].alive)
                    combat_deal_damage_enemy(cs, i, 1);
            }
            combat_set_message(cs, "DEFLECTOR +4 SHIELD, 1 REFLECT");
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
    }

    /* Move card to discard */
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

/* Check if an enemy can attack at current distance */
static bool combat_enemy_in_range(combat_state *cs, int idx) {
    return cs->player_distance <= cs->enemies[idx].attack_range;
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

    /* All alive non-stunned enemies try to close distance */
    for (int i = 0; i < cs->enemy_count; i++) {
        combat_enemy *e = &cs->enemies[i];
        if (!e->alive) continue;
        if (e->flash_timer > 10) continue; /* stunned */
        if (e->lightning_stun > 0) continue; /* lightning stun */
        if (e->ice_turns > 0 && (cs->turn % 2) == 0) continue; /* ice: skip move every other turn */
        /* Advance if not yet in attack range */
        if (cs->player_distance > e->attack_range) {
            cs->player_distance--;
            combat_log(cs, "%s advances -> dist %d",
                       enemy_templates[e->type].name, cs->player_distance);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s ADVANCES! DIST:%d",
                     enemy_templates[e->type].name, cs->player_distance);
            combat_set_message(cs, buf);
            break; /* one enemy advances per turn */
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
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].flash_timer > 0) cs->enemies[i].flash_timer--;

    if (cs->phase == CPHASE_DRAW) {
        if (cs->anim_timer > 0) {
            cs->anim_timer--;
        } else {
            cs->player_shield = 0; /* shield expires at start of new turn */
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
            combat_deal_damage_player(cs, dmg);
            snprintf(buf, sizeof(buf), "%s ATTACKS -%dHP",
                     enemy_templates[e->type].name, dmg);
            combat_set_message(cs, buf);
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
    for (int i = 0; i < cs->hand_count; i++)
        cs->discard[cs->discard_count++] = cs->hand[i];
    cs->hand_count = 0;
    /* movement points persist between rounds */
    combat_log(cs, "-- ENEMY TURN --");
    /* Shield persists through enemy turn, cleared at next draw */
    cs->phase = CPHASE_ENEMY_TURN;
    combat_begin_enemy_turn(cs);
}

/* Spend 1 move point to change distance */
static void combat_action_move_forward(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN) return;
    if (cs->player_move_pts <= 0) {
        combat_set_message(cs, "NO MOVE POINTS");
        return;
    }
    if (cs->player_distance <= 0) {
        combat_set_message(cs, "ALREADY AT MELEE RANGE");
        return;
    }
    cs->player_move_pts--;
    cs->player_distance--;
    combat_log(cs, "advance -> dist %d", cs->player_distance);
    combat_roll_intents(cs); /* enemies react to new distance */
    char buf[64];
    snprintf(buf, sizeof(buf), "ADVANCE! DIST:%d MP:%d", cs->player_distance, cs->player_move_pts);
    combat_set_message(cs, buf);
}

static void combat_action_move_back(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN) return;
    if (cs->player_move_pts <= 0) {
        combat_set_message(cs, "NO MOVE POINTS");
        return;
    }
    cs->player_move_pts--;
    cs->player_distance++;
    combat_log(cs, "retreat -> dist %d", cs->player_distance);
    combat_roll_intents(cs); /* enemies react to new distance */
    char buf[64];
    snprintf(buf, sizeof(buf), "RETREAT! DIST:%d MP:%d", cs->player_distance, cs->player_move_pts);
    combat_set_message(cs, buf);
}

/* ── Button layout constants (used by both render and touch) ─────── */

#define BTN_END_X    (FB_WIDTH - 78)
#define BTN_END_Y    158
#define BTN_END_W    70
#define BTN_END_H    18

/* ── Card layout helpers ─────────────────────────────────────────── */

#define CARD_W  54
#define CARD_H  68
#define CARD_GAP 2
#define CARD_W_SEL 80  /* expanded width when selected */
#define CARD_H_SEL 80  /* expanded height when selected */

static void combat_card_rect(const combat_state *cs, int i, int *ox, int *oy) {
    bool is_sel = (i == cs->cursor && cs->phase == CPHASE_PLAYER_TURN);
    /* Compute total width accounting for selected card expansion */
    int total_w = 0;
    int sel_idx = (cs->phase == CPHASE_PLAYER_TURN) ? cs->cursor : -1;
    for (int j = 0; j < cs->hand_count; j++) {
        int w = (j == sel_idx) ? CARD_W_SEL : CARD_W;
        total_w += w + CARD_GAP;
    }
    total_w -= CARD_GAP;
    int base_x = (FB_WIDTH - total_w) / 2;
    int base_y = FB_HEIGHT - CARD_H - 4;

    int x = base_x;
    for (int j = 0; j < i; j++) {
        int w = (j == sel_idx) ? CARD_W_SEL : CARD_W;
        x += w + CARD_GAP;
    }
    *ox = x;
    *oy = is_sel ? (FB_HEIGHT - CARD_H_SEL - 4) : base_y;
}

/* ── Touch drag input (Slay the Spire style) ─────────────────────── */

static void combat_touch_began(combat_state *cs, float fx, float fy) {
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
        int lb_x = FB_WIDTH - 32, lb_y = 26;
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

    /* Check if tapping a card */
    for (int i = 0; i < cs->hand_count; i++) {
        int cx, cy;
        combat_card_rect(cs, i, &cx, &cy);
        bool is_sel = (i == cs->cursor);
        int cw = is_sel ? CARD_W_SEL : CARD_W;
        int ch = is_sel ? CARD_H_SEL : CARD_H;
        if (fx >= cx && fx < cx + cw && fy >= cy && fy < cy + ch) {
            cs->dragging = true;
            cs->drag_card = i;
            cs->cursor = i;
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

    /* Tap on enemy to target */
    if (cs->enemy_count > 0) {
        int spacing = FB_WIDTH / (cs->enemy_count + 1);
        for (int i = 0; i < cs->enemy_count; i++) {
            if (!cs->enemies[i].alive) continue;
            int ecx = spacing * (i + 1);
            if (fx >= ecx - 24 && fx <= ecx + 24 && fy >= 10 && fy <= 90) {
                cs->target = i;
                return;
            }
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
            if (fx >= ecx - 30 && fx <= ecx + 30 && fy < 130.0f) {
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
        case CARD_DEFLECTOR:   return "+4 SHIELD\n1 REFLECT";
        case CARD_STUN_GUN:    return "STUN 1T\n1 DMG";
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
        for (int i = 0; i < combat.enemy_count; i++) {
            combat_enemy *e = &combat.enemies[i];
            int cx = spacing * (i + 1);
            int sprite_x = cx - 16;
            int sprite_y = 20;

            if (e->alive) {
                const uint32_t *sprite = spr_enemy_table[e->type];

                /* Wiggle offset during this enemy's attack animation */
                int wiggle_x = 0;
                if (combat.phase == CPHASE_ENEMY_TURN &&
                    combat.enemy_atk_idx == i &&
                    combat.enemy_atk_timer > ENEMY_ATK_HIT_FRAME) {
                    /* Wiggle side-to-side before the hit */
                    wiggle_x = ((combat.enemy_atk_timer % 4) < 2) ? 3 : -3;
                }

                if (e->flash_timer > 0 && (e->flash_timer & 2))
                    spr_draw_flash(px, W, H, sprite, sprite_x + wiggle_x, sprite_y, 2);
                else
                    spr_draw(px, W, H, sprite, sprite_x + wiggle_x, sprite_y, 2);

                /* Target highlight (yellow border around selected enemy) */
                if (i == combat.target && combat.phase == CPHASE_PLAYER_TURN) {
                    combat_draw_rect_outline(px, W, H,
                        sprite_x - 2, sprite_y - 2, 36, 36, yellow);
                }

                /* Intent indicator (above sprite) */
                if (combat.phase == CPHASE_PLAYER_TURN || combat.phase == CPHASE_DRAW) {
                    bool stunned = (e->flash_timer > 10) || (e->lightning_stun > 0);
                    if (stunned) {
                        sr_draw_text_shadow(px, W, H, cx - 10, sprite_y - 10,
                                            "STUN", 0xFFCCCC22, shadow);
                    } else if (e->intent == INTENT_MOVE) {
                        /* Movement only */
                        sr_draw_text_shadow(px, W, H, cx - 6, sprite_y - 10,
                                            ">>", 0xFFCC8844, shadow);
                    } else if (e->intent == INTENT_ATTACK) {
                        /* Attack only: red damage number */
                        int dmg = e->damage / 2; if (dmg < 1) dmg = 1;
                        if (e->ice_turns > 0) { dmg = dmg / 2; if (dmg < 1) dmg = 1; }
                        char intent_buf[16];
                        snprintf(intent_buf, sizeof(intent_buf), "%d", dmg);
                        sr_draw_text_shadow(px, W, H, cx - 4, sprite_y - 10,
                                            intent_buf, 0xFF4444FF, shadow);
                    }
                }
            } else {
                sr_draw_text_shadow(px, W, H, cx - 12, sprite_y + 12, "DEAD", 0xFF444444, shadow);
            }

            /* Enemy name */
            sr_draw_text_shadow(px, W, H, cx - 18, sprite_y + 36,
                                enemy_templates[e->type].name,
                                e->alive ? white : 0xFF444444, shadow);

            /* HP bar */
            if (e->alive) {
                combat_draw_bar(px, W, H, cx - 18, sprite_y + 46, 36, 4,
                                e->hp, e->hp_max, 0xFF2222CC, 0xFF333333);
                char hpbuf[16];
                snprintf(hpbuf, sizeof(hpbuf), "%d/%d", e->hp, e->hp_max);
                sr_draw_text_shadow(px, W, H, cx - 10, sprite_y + 52, hpbuf, gray, shadow);
            }

            /* Attack range indicator */
            if (e->alive) {
                char rngbuf[16];
                snprintf(rngbuf, sizeof(rngbuf), "RNG:%d", e->attack_range);
                sr_draw_text_shadow(px, W, H, cx - 12, sprite_y + 62, rngbuf, dim, shadow);

                /* Status effect indicators */
                int sx = cx - 18;
                int sy = sprite_y + 72;
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

    /* ── Distance indicator ───────────────────────────────────── */
    {
        char distbuf[32];
        snprintf(distbuf, sizeof(distbuf), "DISTANCE: %d", combat.player_distance);
        uint32_t dist_col = combat.player_distance == 0 ? 0xFF00CCCC : 0xFF888888;
        sr_draw_text_shadow(px, W, H, W/2 - 30, 108, distbuf, dist_col, shadow);

        /* Visual distance: draw dots */
        int dot_x = W/2 - combat.player_distance * 8;
        for (int d = 0; d <= combat.player_distance; d++) {
            uint32_t dc = d == 0 ? 0xFF00CC00 : 0xFF555555;
            combat_draw_rect(px, W, H, dot_x + d * 16, 120, 4, 4, dc);
        }
        /* Player marker */
        sr_draw_text_shadow(px, W, H, dot_x - 6, 116, "P", 0xFF44FF44, shadow);
        /* Enemy marker */
        sr_draw_text_shadow(px, W, H, dot_x + combat.player_distance * 16 + 6, 116,
                            "E", 0xFFCC4444, shadow);
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

    /* ── Hand of cards (bottom) ───────────────────────────────── */
    {
        int sel_idx = (combat.phase == CPHASE_PLAYER_TURN) ? combat.cursor : -1;
        /* Compute total width with selected card expanded */
        int total_w = 0;
        for (int i = 0; i < combat.hand_count; i++) {
            int w = (i == sel_idx) ? CARD_W_SEL : CARD_W;
            total_w += w + CARD_GAP;
        }
        if (combat.hand_count > 0) total_w -= CARD_GAP;
        int base_x = (W - total_w) / 2;
        int base_y = H - CARD_H - 4;

        int cx_acc = base_x;
        for (int i = 0; i < combat.hand_count; i++) {
            int card = combat.hand[i];
            bool selected = (i == sel_idx);
            int cw = selected ? CARD_W_SEL : CARD_W;
            int ch = selected ? CARD_H_SEL : CARD_H;
            int cy = selected ? (H - CARD_H_SEL - 4) : base_y;
            combat_draw_card_content(px, W, H, cx_acc, cy, cw, ch,
                                     card, selected, shadow, combat.energy);
            cx_acc += cw + CARD_GAP;
        }
    }

    /* ── Drag visualization ───────────────────────────────────── */
    if (combat.dragging && combat.drag_card < combat.hand_count) {
        int card = combat.hand[combat.drag_card];
        int target_type = card_targets[card];

        /* Draw line from card to drag position */
        int cx, cy;
        combat_card_rect(&combat, combat.drag_card, &cx, &cy);
        bool drag_sel = (combat.drag_card == combat.cursor);
        int line_x0 = cx + (drag_sel ? CARD_W_SEL : CARD_W) / 2;
        int line_y0 = cy;
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
            int spacing = W / (combat.enemy_count + 1);
            for (int i = 0; i < combat.enemy_count; i++) {
                if (!combat.enemies[i].alive) continue;
                int ecx = spacing * (i + 1);
                if (combat.drag_x >= ecx - 30 && combat.drag_x <= ecx + 30 && combat.drag_y < 130.0f) {
                    combat_draw_rect_outline(px, W, H, ecx - 26, 14, 52, 84, yellow);
                }
            }
        }
    }

    /* ── Turn info (top right) ────────────────────────────────── */
    {
        char tbuf[32];
        snprintf(tbuf, sizeof(tbuf), "TURN %d", combat.turn);
        sr_draw_text_shadow(px, W, H, W - 50, 4, tbuf, gray, shadow);

        char dbuf[16];
        snprintf(dbuf, sizeof(dbuf), "DECK:%d", combat.deck_count + combat.discard_count);
        sr_draw_text_shadow(px, W, H, W - 50, 14, dbuf, dim, shadow);
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
        int lb_x = W - 32, lb_y = 26;
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
}

#endif /* SR_COMBAT_H */
