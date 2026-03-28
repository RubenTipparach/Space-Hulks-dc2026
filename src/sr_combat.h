/*  sr_combat.h — Card-based combat system for Space Hulks.
 *  Single-TU header-only. Depends on sr_app.h, sr_font.h, sr_sprites.h. */
#ifndef SR_COMBAT_H
#define SR_COMBAT_H

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
    CARD_TYPE_COUNT
};

static const char *card_names[] = {
    "SHIELD", "SHOOT", "BURST", "MOVE", "MELEE",
    "OVERCHRG", "REPAIR", "STUN", "FORTIFY", "DBL SHOT", "DASH"
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
};

static const int card_energy_cost[] = {
    1, 1, 2, 1, 1,  /* base cards */
    0, 2, 1, 2, 2, 2 /* droppable cards */
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
};

/* ── Character classes ───────────────────────────────────────────── */

enum { CLASS_SCOUT, CLASS_MARINE };

typedef struct {
    int hp_max;
    int deck_composition[CARD_TYPE_COUNT]; /* count of each card type */
    const char *name;
    const uint32_t *sprite;
} char_class;

static const char_class char_classes[] = {
    [CLASS_SCOUT] = {
        .hp_max = 20,
        .deck_composition = { 3, 2, 2, 2, 1 }, /* 3 shield, 2 shoot, 2 burst, 2 move, 1 melee */
        .name = "SCOUT",
        .sprite = spr_scout,
    },
    [CLASS_MARINE] = {
        .hp_max = 30,
        .deck_composition = { 4, 3, 1, 1, 1 }, /* 4 shield, 3 shoot, 1 burst, 1 move, 1 melee */
        .name = "MARINE",
        .sprite = spr_marine,
    },
};

/* ── Enemy types ─────────────────────────────────────────────────── */

enum { ENEMY_LURKER, ENEMY_BRUTE, ENEMY_SPITTER, ENEMY_HIVEGUARD, ENEMY_TYPE_COUNT };

typedef struct {
    const char *name;
    int hp_max;
    int damage;
    int move_points;  /* how many move cards needed to reach for melee */
    int ranged;       /* 1 = can shoot back */
} enemy_template;

static const enemy_template enemy_templates[] = {
    [ENEMY_LURKER]    = { "LURKER",     8,  2, 1, 0 },
    [ENEMY_BRUTE]     = { "BRUTE",     18,  5, 2, 0 },
    [ENEMY_SPITTER]   = { "SPITTER",   10,  3, 3, 1 },
    [ENEMY_HIVEGUARD] = { "HIVEGUARD", 24,  4, 2, 1 },
};

/* ── Combat state ────────────────────────────────────────────────── */

#define COMBAT_MAX_ENEMIES   4
#define COMBAT_DECK_MAX      20
#define COMBAT_HAND_MAX      5

typedef struct {
    int type;
    int hp;
    int hp_max;
    int move_points;  /* moves needed to melee */
    int damage;
    int ranged;
    int flash_timer;  /* > 0 = flashing white */
    bool alive;
} combat_enemy;

typedef struct {
    /* Player */
    int player_class;
    int player_hp;
    int player_hp_max;
    int player_shield;
    int player_distance;  /* steps to enemies (0 = melee range) */
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

    /* Drag state */
    bool dragging;
    int drag_card;         /* index in hand being dragged */
    float drag_x, drag_y;  /* current drag position (framebuffer coords) */
    float drag_start_x, drag_start_y;

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
    /* Pick 3 droppable card types (CARD_OVERCHARGE through CARD_DASH) */
    int droppable_start = CARD_OVERCHARGE;
    int droppable_count = CARD_TYPE_COUNT - droppable_start;
    for (int i = 0; i < 3; i++) {
        cs->reward_choices[i] = droppable_start + dng_rng_int(droppable_count);
    }
    cs->reward_cursor = 0;
}

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
        cs->enemies[i].move_points = tmpl->move_points;
        cs->enemies[i].damage = tmpl->damage;
        cs->enemies[i].ranged = tmpl->ranged;
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
    snprintf(cs->message, sizeof(cs->message), "ENEMIES DETECTED!");
}

/* ── Card logic ──────────────────────────────────────────────────── */

static void combat_set_message(combat_state *cs, const char *msg) {
    snprintf(cs->message, sizeof(cs->message), "%s", msg);
    cs->message_timer = 60;
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
    int absorbed = dmg < cs->player_shield ? dmg : cs->player_shield;
    cs->player_shield -= absorbed;
    dmg -= absorbed;
    cs->player_hp -= dmg;
    if (cs->player_hp <= 0) cs->player_hp = 0;
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
                combat_deal_damage_enemy(cs, t, 3);
                snprintf(buf, sizeof(buf), "SHOOT %s -3HP", enemy_templates[cs->enemies[t].type].name);
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_BURST:
            for (int i = 0; i < cs->enemy_count; i++) {
                if (cs->enemies[i].alive)
                    combat_deal_damage_enemy(cs, i, 2);
            }
            combat_set_message(cs, "BURST -2HP ALL");
            break;

        case CARD_MOVE:
            if (cs->player_distance > 0) {
                cs->player_distance--;
                snprintf(buf, sizeof(buf), "ADVANCE! DIST: %d", cs->player_distance);
                combat_set_message(cs, buf);
            } else {
                combat_set_message(cs, "ALREADY IN MELEE RANGE");
                cs->energy += cost; /* refund */
            }
            break;

        case CARD_MELEE:
            if (cs->player_distance <= 0) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    combat_deal_damage_enemy(cs, t, 6);
                    snprintf(buf, sizeof(buf), "MELEE %s -6HP!", enemy_templates[cs->enemies[t].type].name);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "TOO FAR! DIST: %d", cs->player_distance);
                combat_set_message(cs, buf);
                cs->energy += cost; /* refund */
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
                combat_deal_damage_enemy(cs, t, 5);
                snprintf(buf, sizeof(buf), "DOUBLE SHOT %s -5HP", enemy_templates[cs->enemies[t].type].name);
                combat_set_message(cs, buf);
            }
            break;
        }

        case CARD_DASH:
            if (cs->player_distance > 0) cs->player_distance--;
            if (cs->player_distance > 0) cs->player_distance--;
            if (cs->player_distance <= 0) {
                int t = cs->target;
                while (t < cs->enemy_count && !cs->enemies[t].alive) t++;
                if (t >= cs->enemy_count) t = combat_first_alive_enemy(cs);
                if (t >= 0) {
                    combat_deal_damage_enemy(cs, t, 4);
                    snprintf(buf, sizeof(buf), "DASH STRIKE %s -4HP!", enemy_templates[cs->enemies[t].type].name);
                    combat_set_message(cs, buf);
                }
            } else {
                snprintf(buf, sizeof(buf), "DASH! DIST: %d", cs->player_distance);
                combat_set_message(cs, buf);
            }
            break;
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

/* ── Enemy turn ──────────────────────────────────────────────────── */

static void combat_enemy_turn(combat_state *cs) {
    for (int i = 0; i < cs->enemy_count; i++) {
        if (!cs->enemies[i].alive) continue;
        combat_enemy *e = &cs->enemies[i];

        /* Stunned enemies skip attack (flash_timer > 10 = stunned) */
        if (e->flash_timer > 10) continue;

        if (e->ranged || cs->player_distance <= 0) {
            combat_deal_damage_player(cs, e->damage);
            char buf[64];
            snprintf(buf, sizeof(buf), "%s ATTACKS -%dHP",
                     enemy_templates[e->type].name, e->damage);
            combat_set_message(cs, buf);
        }
    }
}

/* ── Update ──────────────────────────────────────────────────────── */

static void combat_update(combat_state *cs) {
    /* Decrement timers */
    if (cs->message_timer > 0) cs->message_timer--;
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].flash_timer > 0) cs->enemies[i].flash_timer--;

    if (cs->phase == CPHASE_DRAW) {
        if (cs->anim_timer > 0) {
            cs->anim_timer--;
        } else {
            cs->energy = cs->energy_max; /* refill energy each turn */
            combat_draw_hand(cs);
            cs->cursor = 0;
            cs->target = combat_first_alive_enemy(cs);
            if (cs->target < 0) cs->target = 0;
            cs->phase = CPHASE_PLAYER_TURN;
        }
    }

    if (cs->phase == CPHASE_ENEMY_TURN) {
        if (cs->anim_timer > 0) {
            cs->anim_timer--;
        } else {
            combat_enemy_turn(cs);
            if (cs->player_hp <= 0) {
                cs->phase = CPHASE_RESULT;
                cs->player_won = false;
                cs->combat_over = true;
                combat_set_message(cs, "DEFEATED...");
            } else {
                cs->turn++;
                cs->phase = CPHASE_DRAW;
                cs->anim_timer = 20;
            }
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

static void combat_action_play(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN || cs->hand_count <= 0) return;
    combat_play_card(cs, cs->cursor);
    combat_check_victory(cs);
}

static void combat_action_end_turn(combat_state *cs) {
    if (cs->phase != CPHASE_PLAYER_TURN) return;
    for (int i = 0; i < cs->hand_count; i++)
        cs->discard[cs->discard_count++] = cs->hand[i];
    cs->hand_count = 0;
    cs->player_shield = 0;
    cs->phase = CPHASE_ENEMY_TURN;
    cs->anim_timer = 30;
}

/* ── Button layout constants (used by both render and touch) ─────── */

#define BTN_PLAY_X   (FB_WIDTH - 78)
#define BTN_PLAY_Y   135
#define BTN_PLAY_W   70
#define BTN_PLAY_H   18

#define BTN_END_X    (FB_WIDTH - 78)
#define BTN_END_Y    158
#define BTN_END_W    70
#define BTN_END_H    18

/* ── Card layout helpers ─────────────────────────────────────────── */

#define CARD_W  52
#define CARD_H  44
#define CARD_GAP 4

static void combat_card_rect(const combat_state *cs, int i, int *ox, int *oy) {
    int total_w = cs->hand_count * (CARD_W + CARD_GAP) - CARD_GAP;
    int base_x = (FB_WIDTH - total_w) / 2;
    int base_y = FB_HEIGHT - CARD_H - 8;
    *ox = base_x + i * (CARD_W + CARD_GAP);
    *oy = base_y;
    if (i == cs->cursor && cs->phase == CPHASE_PLAYER_TURN) *oy -= 6;
}

/* ── Touch drag input (Slay the Spire style) ─────────────────────── */

static void combat_touch_began(combat_state *cs, float fx, float fy) {
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
        if (fx >= cx && fx < cx + CARD_W && fy >= cy && fy < cy + CARD_H) {
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

    if (target_type == TARGET_SELF) {
        /* Shield/Move: just drag upward to play on self */
        combat_play_card(cs, cs->drag_card);
        combat_check_victory(cs);
    } else if (target_type == TARGET_ALL_ENEMIES) {
        /* Burst: drag into enemy area (top half) */
        if (fy < 130.0f) {
            combat_play_card(cs, cs->drag_card);
            if (combat_all_enemies_dead(cs)) {
                cs->phase = CPHASE_RESULT;
                cs->player_won = true;
                cs->combat_over = true;
                combat_set_message(cs, "VICTORY!");
            }
        }
    } else {
        /* Shoot/Melee: drag onto a specific enemy */
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
            if (combat_all_enemies_dead(cs)) {
                cs->phase = CPHASE_RESULT;
                cs->player_won = true;
                cs->combat_over = true;
                combat_set_message(cs, "VICTORY!");
            }
        }
    }
}

static bool combat_handle_tap(combat_state *cs, float fx, float fy) {
    if (cs->phase == CPHASE_RESULT) return true;
    combat_touch_began(cs, fx, fy);
    return true;
}

/* ── Keyboard input ──────────────────────────────────────────────── */

static void combat_handle_key(combat_state *cs, int key) {
    if (cs->phase == CPHASE_RESULT) return;
    if (cs->phase != CPHASE_PLAYER_TURN) return;

    switch (key) {
        case SAPP_KEYCODE_LEFT:
        case SAPP_KEYCODE_A:
            if (cs->cursor > 0) cs->cursor--;
            break;
        case SAPP_KEYCODE_RIGHT:
        case SAPP_KEYCODE_D:
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
        case SAPP_KEYCODE_ENTER:
        case SAPP_KEYCODE_KP_ENTER:
        case SAPP_KEYCODE_SPACE:
            combat_action_play(cs);
            break;
        case SAPP_KEYCODE_TAB:
        case SAPP_KEYCODE_E:
            combat_action_end_turn(cs);
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
                if (e->flash_timer > 0 && (e->flash_timer & 2))
                    spr_draw_flash(px, W, H, sprite, sprite_x, sprite_y, 2);
                else
                    spr_draw(px, W, H, sprite, sprite_x, sprite_y, 2);

                /* Target indicator */
                if (i == combat.target && combat.phase == CPHASE_PLAYER_TURN) {
                    sr_draw_text_shadow(px, W, H, cx - 3, sprite_y - 10, "V", yellow, shadow);
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

            /* Move points indicator */
            if (e->alive) {
                char mpbuf[16];
                snprintf(mpbuf, sizeof(mpbuf), "MP:%d", e->move_points);
                sr_draw_text_shadow(px, W, H, cx - 10, sprite_y + 62, mpbuf, dim, shadow);
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

        /* Player sprite */
        spr_draw(px, W, H, cc->sprite, 8, 140, 2);

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
    }

    /* ── Hand of cards (bottom) ───────────────────────────────── */
    {
        int card_w = 52;
        int card_h = 44;
        int card_gap = 4;
        int total_w = combat.hand_count * (card_w + card_gap) - card_gap;
        int base_x = (W - total_w) / 2;
        int base_y = H - card_h - 8;

        for (int i = 0; i < combat.hand_count; i++) {
            int card = combat.hand[i];
            int cx = base_x + i * (card_w + card_gap);
            int cy = base_y;
            bool selected = (i == combat.cursor && combat.phase == CPHASE_PLAYER_TURN);

            if (selected) cy -= 6;

            /* Card background */
            uint32_t bg = selected ? 0xFF222233 : 0xFF111122;
            combat_draw_rect(px, W, H, cx, cy, card_w, card_h, bg);

            /* Card border */
            uint32_t border = selected ? yellow : card_colors[card];
            combat_draw_rect_outline(px, W, H, cx, cy, card_w, card_h, border);
            if (selected)
                combat_draw_rect_outline(px, W, H, cx+1, cy+1, card_w-2, card_h-2, border);

            /* Card type color stripe at top */
            combat_draw_rect(px, W, H, cx + 1, cy + 1, card_w - 2, 3, card_colors[card]);

            /* Card name */
            sr_draw_text_shadow(px, W, H, cx + 4, cy + 8, card_names[card], white, shadow);

            /* Energy cost */
            {
                int cost = card_energy_cost[card];
                char cbuf[8];
                snprintf(cbuf, sizeof(cbuf), "%d", cost);
                uint32_t ccol = combat.energy >= cost ? 0xFF22CCEE : 0xFF4444CC;
                sr_draw_text_shadow(px, W, H, cx + card_w - 10, cy + 5, cbuf, ccol, shadow);
            }

            /* Card effect text */
            const char *effect = "";
            switch (card) {
                case CARD_SHIELD:     effect = "+3 SH"; break;
                case CARD_SHOOT:      effect = "3 DMG"; break;
                case CARD_BURST:      effect = "2 ALL"; break;
                case CARD_MOVE:       effect = "ADV 1"; break;
                case CARD_MELEE:      effect = "6 DMG"; break;
                case CARD_OVERCHARGE: effect = "+2 NRG"; break;
                case CARD_REPAIR:     effect = "+4 HP"; break;
                case CARD_STUN:       effect = "STUN"; break;
                case CARD_FORTIFY:    effect = "+6 SH"; break;
                case CARD_DOUBLE_SHOT:effect = "5 DMG"; break;
                case CARD_DASH:       effect = "RUSH"; break;
            }
            sr_draw_text_shadow(px, W, H, cx + 4, cy + 20, effect, gray, shadow);

            /* Extra info for melee */
            if (card == CARD_MELEE) {
                const char *req = combat.player_distance <= 0 ? "READY" : "NEED MOVE";
                uint32_t rc = combat.player_distance <= 0 ? 0xFF00CC00 : 0xFF4444CC;
                sr_draw_text_shadow(px, W, H, cx + 4, cy + 30, req, rc, shadow);
            }
            if (card == CARD_BURST) {
                sr_draw_text_shadow(px, W, H, cx + 4, cy + 30, "HIT ALL", 0xFF5588FF, shadow);
            }
        }
    }

    /* ── Drag visualization ───────────────────────────────────── */
    if (combat.dragging && combat.drag_card < combat.hand_count) {
        int card = combat.hand[combat.drag_card];
        int target_type = card_targets[card];

        /* Draw line from card to drag position */
        int cx, cy;
        combat_card_rect(&combat, combat.drag_card, &cx, &cy);
        int line_x0 = cx + CARD_W / 2;
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
        /* PLAY button */
        uint32_t play_bg = combat.hand_count > 0 ? 0xFF224422 : 0xFF222222;
        uint32_t play_border = combat.hand_count > 0 ? 0xFF44CC44 : 0xFF444444;
        combat_draw_rect(px, W, H, BTN_PLAY_X, BTN_PLAY_Y, BTN_PLAY_W, BTN_PLAY_H, play_bg);
        combat_draw_rect_outline(px, W, H, BTN_PLAY_X, BTN_PLAY_Y, BTN_PLAY_W, BTN_PLAY_H, play_border);
        sr_draw_text_shadow(px, W, H, BTN_PLAY_X + 10, BTN_PLAY_Y + 5,
                            "PLAY", play_border, shadow);

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

            uint32_t rbg = rsel ? 0xFF222244 : 0xFF111122;
            uint32_t rborder = rsel ? yellow : card_colors[rc];
            combat_draw_rect(px, W, H, rx, ry, rw, rh, rbg);
            combat_draw_rect_outline(px, W, H, rx, ry, rw, rh, rborder);
            if (rsel)
                combat_draw_rect_outline(px, W, H, rx+1, ry+1, rw-2, rh-2, rborder);

            combat_draw_rect(px, W, H, rx+1, ry+1, rw-2, 3, card_colors[rc]);
            sr_draw_text_shadow(px, W, H, rx+4, ry+8, card_names[rc], white, shadow);

            /* Cost */
            char costbuf[8];
            snprintf(costbuf, sizeof(costbuf), "%dE", card_energy_cost[rc]);
            sr_draw_text_shadow(px, W, H, rx+rw-18, ry+8, costbuf, 0xFF22CCEE, shadow);

            /* Effect description */
            const char *desc = "";
            switch (rc) {
                case CARD_OVERCHARGE: desc = "+2 ENERGY\nTHIS TURN"; break;
                case CARD_REPAIR:     desc = "HEAL 4 HP"; break;
                case CARD_STUN:       desc = "SKIP ENEMY\nATTACKS"; break;
                case CARD_FORTIFY:    desc = "+6 SHIELD"; break;
                case CARD_DOUBLE_SHOT:desc = "5 DMG\nSINGLE"; break;
                case CARD_DASH:       desc = "ADV 2 +\n4 DMG"; break;
            }
            sr_draw_text_shadow(px, W, H, rx+4, ry+28, desc, gray, shadow);

            /* Target type label */
            const char *tgt = "";
            int tt = card_targets[rc];
            if (tt == TARGET_SELF) tgt = "SELF";
            else if (tt == TARGET_ENEMY) tgt = "1 ENEMY";
            else tgt = "ALL";
            sr_draw_text_shadow(px, W, H, rx+4, ry+rh-14, tgt, dim, shadow);
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
}

#endif /* SR_COMBAT_H */
