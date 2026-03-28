/*  sr_ship.h — FTL/Void Bastards-style ship system for Space Hulks.
 *  Defines ship layouts, room types, officers, missions, and ship status.
 *  Single-TU header-only. Depends on sr_dungeon.h. */
#ifndef SR_SHIP_H
#define SR_SHIP_H

/* ── Room types (subsystems) ────────────────────────────────────── */

enum {
    ROOM_CORRIDOR,   /* empty connecting corridor */
    ROOM_BRIDGE,     /* command center - most heavily defended */
    ROOM_MEDBAY,     /* medical bay */
    ROOM_WEAPONS,    /* weapons array */
    ROOM_ENGINES,    /* propulsion */
    ROOM_REACTOR,    /* power core */
    ROOM_SHIELDS,    /* shield generator */
    ROOM_CARGO,      /* cargo hold / artifact storage */
    ROOM_BARRACKS,   /* crew quarters */
    ROOM_TYPE_COUNT
};

static const char *room_type_names[] = {
    "CORRIDOR", "BRIDGE", "MEDBAY", "WEAPONS", "ENGINES",
    "REACTOR", "SHIELDS", "CARGO", "BARRACKS"
};

static const uint32_t room_type_colors[] = {
    0xFF555555,  /* corridor - gray */
    0xFF22CCEE,  /* bridge - cyan */
    0xFF22CC44,  /* medbay - green */
    0xFF2244FF,  /* weapons - red-orange */
    0xFF44AACC,  /* engines - steel blue */
    0xFF00AAFF,  /* reactor - orange */
    0xFFCCAA22, /* shields - blue-gold */
    0xFF889944,  /* cargo - olive */
    0xFF666688,  /* barracks - slate */
};

/* ── Officer ranks and names ────────────────────────────────────── */

enum {
    RANK_ENSIGN,     /* weakest */
    RANK_LIEUTENANT,
    RANK_COMMANDER,
    RANK_CAPTAIN,    /* strongest - usually on bridge */
    RANK_COUNT
};

static const char *rank_names[] = {
    "ENSIGN", "LIEUTENANT", "COMMANDER", "CAPTAIN"
};

/* Officer first names (procedural pick) */
static const char *officer_first_names[] = {
    "VOSS", "KIRA", "DREN", "MALK", "SIRA",
    "TREN", "JACE", "XYLA", "BRAM", "NOVA",
    "RETH", "ZARA", "GRIM", "TOVA", "KAES",
    "HOLT", "VERA", "DUSK", "LORN", "CYRA",
};
#define NUM_OFFICER_NAMES 20

/* ── Mission types ──────────────────────────────────────────────── */

enum {
    MISSION_DESTROY_SHIP,     /* destroy or capture the ship (take bridge) */
    MISSION_CAPTURE_OFFICER,  /* capture a specific officer alive */
    MISSION_STEAL_ARTIFACT,   /* retrieve artifact from cargo */
    MISSION_DISABLE_ENGINES,  /* disable engines and escape */
    MISSION_TYPE_COUNT
};

static const char *mission_type_names[] = {
    "DESTROY SHIP", "CAPTURE OFFICER", "STEAL ARTIFACT", "DISABLE ENGINES"
};

static const char *mission_descriptions[] = {
    "TAKE THE BRIDGE OR\nDESTROY THE SHIP",
    "CAPTURE THE TARGET\nOFFICER ALIVE",
    "FIND THE ARTIFACT\nIN THE CARGO HOLD",
    "DISABLE THE ENGINES\nAND ESCAPE",
};

/* ── Ship room data ─────────────────────────────────────────────── */

#define SHIP_MAX_ROOMS    12
#define SHIP_MAX_OFFICERS 8
#define SHIP_MAX_DECKS    3

typedef struct {
    int type;            /* ROOM_xxx */
    int deck;            /* which deck (0-based) */
    int room_idx;        /* index on its deck in dng_room */
    int subsystem_hp;    /* 0 = destroyed */
    int subsystem_hp_max;
    bool cleared;        /* all enemies in room defeated */
    bool visited;        /* player has entered this room */
} ship_room;

typedef struct {
    char name[32];       /* e.g. "CDR VOSS" */
    int rank;            /* RANK_xxx */
    int room_idx;        /* which ship_room they're in */
    bool alive;
    bool captured;       /* for capture missions */
    int combat_type;     /* enemy_type used in combat */
} ship_officer;

/* ── Mission state ──────────────────────────────────────────────── */

typedef struct {
    int type;            /* MISSION_xxx */
    int target_officer;  /* index into officers[] for CAPTURE missions */
    bool completed;
    bool failed;
    char description[64];
} ship_mission;

/* ── Ship state ─────────────────────────────────────────────────── */

typedef struct {
    /* Ship identity */
    char name[32];       /* e.g. "ISS VALKYRIE" */
    int num_decks;       /* 1-3 */

    /* Rooms */
    ship_room rooms[SHIP_MAX_ROOMS];
    int room_count;

    /* Room->deck mapping: which rooms belong to each deck */
    int deck_room_start[SHIP_MAX_DECKS];  /* first room index on deck */
    int deck_room_count[SHIP_MAX_DECKS];  /* how many rooms on deck */

    /* Officers */
    ship_officer officers[SHIP_MAX_OFFICERS];
    int officer_count;

    /* Mission */
    ship_mission mission;
    ship_mission bonus_missions[2];
    int bonus_count;

    /* Ship health */
    int hull_hp;         /* overall hull, reduced by destroying subsystems */
    int hull_hp_max;

    /* Player's ship */
    int player_hull_hp;
    int player_hull_hp_max;
    int player_hull_timer;  /* frames until next hull damage tick */
    int player_hull_tick;   /* damage per tick from enemy weapons */

    /* Timing */
    int turns_elapsed;   /* combat rounds across the whole boarding */
    int turns_until_destruction; /* 0 = no timer, >0 = ship fires on you */
    bool player_ship_destroyed;
    bool enemy_ship_destroyed;
    bool boarding_active; /* true while player is on enemy ship */

    bool initialized;
} ship_state;

static ship_state current_ship;

/* ── Ship name generation ───────────────────────────────────────── */

static const char *ship_prefixes[] = {
    "ISS", "HMS", "ARC", "TFS", "KSS",
};
#define NUM_SHIP_PREFIXES 5

static const char *ship_suffixes[] = {
    "VALKYRIE", "WRAITH", "DOMINION", "PHANTOM", "SERAPH",
    "HARBINGER", "REVENANT", "ECLIPSE", "LEVIATHAN", "CERBERUS",
    "STORMCROW", "IRONCLAD", "HORIZON", "BASTION", "VANGUARD",
};
#define NUM_SHIP_SUFFIXES 15

/* ── Ship generation ────────────────────────────────────────────── */

static void ship_generate(ship_state *ship, int difficulty, uint32_t seed) {
    dng_rng_seed(seed);
    memset(ship, 0, sizeof(*ship));

    /* Ship name */
    int pf = dng_rng_int(NUM_SHIP_PREFIXES);
    int sf = dng_rng_int(NUM_SHIP_SUFFIXES);
    snprintf(ship->name, sizeof(ship->name), "%s %s",
             ship_prefixes[pf], ship_suffixes[sf]);

    /* Decks: 1 at low difficulty, 2-3 at higher */
    ship->num_decks = 1 + (difficulty > 1 ? 1 : 0) + (difficulty > 3 ? 1 : 0);
    if (ship->num_decks > SHIP_MAX_DECKS) ship->num_decks = SHIP_MAX_DECKS;

    /* Generate rooms per deck */
    ship->room_count = 0;

    /* Required rooms: BRIDGE, ENGINES always present */
    /* Layout: each deck gets 2-4 rooms */
    int required_rooms[] = {
        ROOM_BRIDGE, ROOM_ENGINES, ROOM_REACTOR, ROOM_WEAPONS,
        ROOM_SHIELDS, ROOM_MEDBAY, ROOM_CARGO, ROOM_BARRACKS
    };
    int num_required = 8;
    int assigned = 0;

    for (int deck = 0; deck < ship->num_decks; deck++) {
        ship->deck_room_start[deck] = ship->room_count;
        int rooms_on_deck = 2 + dng_rng_int(3); /* 2-4 rooms per deck */
        if (rooms_on_deck + ship->room_count > SHIP_MAX_ROOMS)
            rooms_on_deck = SHIP_MAX_ROOMS - ship->room_count;

        for (int r = 0; r < rooms_on_deck; r++) {
            ship_room *rm = &ship->rooms[ship->room_count];
            rm->deck = deck;
            rm->room_idx = r;
            rm->cleared = false;
            rm->visited = false;

            /* Assign room type: required first, then random */
            if (assigned < num_required) {
                rm->type = required_rooms[assigned++];
            } else {
                /* Random non-bridge room */
                int types[] = { ROOM_CORRIDOR, ROOM_BARRACKS, ROOM_CARGO, ROOM_MEDBAY };
                rm->type = types[dng_rng_int(4)];
            }

            /* Subsystem HP based on importance */
            switch (rm->type) {
                case ROOM_BRIDGE:   rm->subsystem_hp_max = 20; break;
                case ROOM_REACTOR:  rm->subsystem_hp_max = 15; break;
                case ROOM_ENGINES:  rm->subsystem_hp_max = 12; break;
                case ROOM_WEAPONS:  rm->subsystem_hp_max = 10; break;
                case ROOM_SHIELDS:  rm->subsystem_hp_max = 10; break;
                default:            rm->subsystem_hp_max = 0;  break; /* no subsystem */
            }
            rm->subsystem_hp = rm->subsystem_hp_max;

            ship->room_count++;
        }
        ship->deck_room_count[deck] = rooms_on_deck;
    }

    /* Hull HP = sum of all subsystem HP */
    ship->hull_hp_max = 0;
    for (int i = 0; i < ship->room_count; i++)
        ship->hull_hp_max += ship->rooms[i].subsystem_hp_max;
    if (ship->hull_hp_max < 30) ship->hull_hp_max = 30;
    ship->hull_hp = ship->hull_hp_max;

    /* Player ship */
    ship->player_hull_hp_max = 30 + difficulty * 5;
    ship->player_hull_hp = ship->player_hull_hp_max;
    ship->player_hull_timer = 0;
    ship->player_hull_tick = 1 + difficulty / 2;
    /* Enemy ship fires every N turns (fewer turns = harder) */
    ship->turns_until_destruction = 0; /* set when weapons subsystem is active */

    /* Generate officers */
    ship->officer_count = 2 + dng_rng_int(2 + difficulty);
    if (ship->officer_count > SHIP_MAX_OFFICERS)
        ship->officer_count = SHIP_MAX_OFFICERS;

    for (int i = 0; i < ship->officer_count; i++) {
        ship_officer *off = &ship->officers[i];
        int name_idx = dng_rng_int(NUM_OFFICER_NAMES);

        /* Captain always on bridge, higher ranks in important rooms */
        if (i == 0) {
            off->rank = RANK_CAPTAIN;
            /* Find bridge room */
            off->room_idx = 0;
            for (int r = 0; r < ship->room_count; r++) {
                if (ship->rooms[r].type == ROOM_BRIDGE) {
                    off->room_idx = r;
                    break;
                }
            }
        } else {
            off->rank = dng_rng_int(RANK_COUNT - 1); /* Ensign to Commander */
            off->room_idx = dng_rng_int(ship->room_count);
        }

        snprintf(off->name, sizeof(off->name), "%s %s",
                 rank_names[off->rank],
                 officer_first_names[name_idx]);
        off->alive = true;
        off->captured = false;

        /* Higher rank = tougher enemy type */
        switch (off->rank) {
            case RANK_ENSIGN:     off->combat_type = ENEMY_LURKER; break;
            case RANK_LIEUTENANT: off->combat_type = ENEMY_BRUTE; break;
            case RANK_COMMANDER:  off->combat_type = ENEMY_SPITTER; break;
            case RANK_CAPTAIN:    off->combat_type = ENEMY_HIVEGUARD; break;
            default:              off->combat_type = ENEMY_LURKER; break;
        }
    }

    /* Generate mission */
    ship->mission.type = MISSION_DESTROY_SHIP; /* default: take the bridge */
    ship->mission.completed = false;
    ship->mission.failed = false;
    ship->mission.target_officer = 0; /* captain */
    snprintf(ship->mission.description, sizeof(ship->mission.description),
             "%s", mission_descriptions[MISSION_DESTROY_SHIP]);

    /* Bonus missions (randomly pick 1-2) */
    ship->bonus_count = 1 + dng_rng_int(2);
    for (int i = 0; i < ship->bonus_count; i++) {
        int btype = 1 + dng_rng_int(MISSION_TYPE_COUNT - 1); /* not DESTROY_SHIP */
        ship->bonus_missions[i].type = btype;
        ship->bonus_missions[i].completed = false;
        ship->bonus_missions[i].failed = false;
        snprintf(ship->bonus_missions[i].description,
                 sizeof(ship->bonus_missions[i].description),
                 "%s", mission_descriptions[btype]);

        if (btype == MISSION_CAPTURE_OFFICER && ship->officer_count > 1) {
            ship->bonus_missions[i].target_officer = 1 + dng_rng_int(ship->officer_count - 1);
        }
    }

    ship->turns_elapsed = 0;
    ship->player_ship_destroyed = false;
    ship->enemy_ship_destroyed = false;
    ship->boarding_active = true;
    ship->initialized = true;
}

/* ── Map room type to dungeon generation ────────────────────────── */

/*  When generating a dungeon floor (deck), we assign room types from
 *  the ship layout and place officers as special aliens. */

static void ship_populate_deck(ship_state *ship, sr_dungeon *d,
                                int deck, int num_rooms, const dng_room *dng_rooms)
{
    int start = ship->deck_room_start[deck];
    int count = ship->deck_room_count[deck];
    if (count > num_rooms) count = num_rooms;

    /* Map each generated dng_room to a ship_room */
    for (int i = 0; i < count && i < num_rooms; i++) {
        ship->rooms[start + i].room_idx = i;
    }

    /* Place officers as aliens in their assigned rooms on this deck */
    for (int o = 0; o < ship->officer_count; o++) {
        ship_officer *off = &ship->officers[o];
        if (!off->alive || off->captured) continue;

        int sr_idx = off->room_idx;
        if (sr_idx < start || sr_idx >= start + count) continue;

        /* Place in the center of the corresponding dng_room */
        int local_room = sr_idx - start;
        if (local_room >= 0 && local_room < num_rooms) {
            int cx = dng_rooms[local_room].cx;
            int cy = dng_rooms[local_room].cy;
            /* Officer as combat type + 1 (1-indexed alien encoding) */
            if (cx >= 1 && cx <= d->w && cy >= 1 && cy <= d->h)
                d->aliens[cy][cx] = (uint8_t)(off->combat_type + 1);
        }
    }

    /* Place additional enemies in rooms based on difficulty */
    for (int i = 0; i < count && i < num_rooms; i++) {
        ship_room *rm = &ship->rooms[start + i];
        if (rm->cleared) continue;

        int extra = 0;
        switch (rm->type) {
            case ROOM_BRIDGE:  extra = 2; break; /* heavily defended */
            case ROOM_REACTOR: extra = 1; break;
            case ROOM_WEAPONS: extra = 1; break;
            case ROOM_ENGINES: extra = 1; break;
            default:           extra = dng_rng_int(2); break;
        }

        for (int e = 0; e < extra; e++) {
            int ax = dng_rooms[i].x + dng_rng_int(dng_rooms[i].w);
            int ay = dng_rooms[i].y + dng_rng_int(dng_rooms[i].h);
            if (ax < 1 || ax > d->w || ay < 1 || ay > d->h) continue;
            if (d->map[ay][ax] != 0) continue;
            if (d->aliens[ay][ax] != 0) continue;
            if (ax == d->spawn_gx && ay == d->spawn_gy) continue;
            int max_type = (rm->type == ROOM_BRIDGE) ? 4 : 3;
            d->aliens[ay][ax] = 1 + (uint8_t)dng_rng_int(max_type);
        }
    }
}

/* ── Ship damage / turn tick ────────────────────────────────────── */

static void ship_tick_turn(ship_state *ship) {
    if (!ship->boarding_active) return;
    ship->turns_elapsed++;

    /* Enemy weapons damage player ship if weapons subsystem is still up */
    bool weapons_active = false;
    for (int i = 0; i < ship->room_count; i++) {
        if (ship->rooms[i].type == ROOM_WEAPONS && ship->rooms[i].subsystem_hp > 0) {
            weapons_active = true;
            break;
        }
    }

    if (weapons_active) {
        ship->player_hull_timer++;
        /* Fire every 3 turns */
        if (ship->player_hull_timer >= 3) {
            ship->player_hull_timer = 0;
            ship->player_hull_hp -= ship->player_hull_tick;
            if (ship->player_hull_hp <= 0) {
                ship->player_hull_hp = 0;
                ship->player_ship_destroyed = true;
            }
        }
    }

    /* Recalculate enemy hull from subsystem damage */
    int total_subsystem = 0;
    for (int i = 0; i < ship->room_count; i++)
        total_subsystem += ship->rooms[i].subsystem_hp;
    ship->hull_hp = total_subsystem;
    if (ship->hull_hp <= 0) {
        ship->hull_hp = 0;
        ship->enemy_ship_destroyed = true;
    }
}

/* ── Subsystem damage (called after combat in a room) ───────────── */

static void ship_damage_subsystem(ship_state *ship, int room_idx, int dmg) {
    if (room_idx < 0 || room_idx >= ship->room_count) return;
    ship_room *rm = &ship->rooms[room_idx];
    if (rm->subsystem_hp_max <= 0) return; /* no subsystem to damage */
    rm->subsystem_hp -= dmg;
    if (rm->subsystem_hp < 0) rm->subsystem_hp = 0;
}

/* ── Check mission completion ───────────────────────────────────── */

static void ship_check_missions(ship_state *ship) {
    /* Primary: DESTROY_SHIP = bridge control or hull at 0 */
    if (ship->mission.type == MISSION_DESTROY_SHIP) {
        /* Bridge captured? */
        for (int i = 0; i < ship->room_count; i++) {
            if (ship->rooms[i].type == ROOM_BRIDGE && ship->rooms[i].cleared) {
                ship->mission.completed = true;
            }
        }
        if (ship->enemy_ship_destroyed)
            ship->mission.completed = true;
    }

    /* Bonus missions */
    for (int b = 0; b < ship->bonus_count; b++) {
        ship_mission *bm = &ship->bonus_missions[b];
        if (bm->completed) continue;

        switch (bm->type) {
            case MISSION_CAPTURE_OFFICER:
                if (bm->target_officer >= 0 && bm->target_officer < ship->officer_count) {
                    if (ship->officers[bm->target_officer].captured)
                        bm->completed = true;
                }
                break;
            case MISSION_STEAL_ARTIFACT:
                for (int i = 0; i < ship->room_count; i++) {
                    if (ship->rooms[i].type == ROOM_CARGO && ship->rooms[i].cleared)
                        bm->completed = true;
                }
                break;
            case MISSION_DISABLE_ENGINES:
                for (int i = 0; i < ship->room_count; i++) {
                    if (ship->rooms[i].type == ROOM_ENGINES && ship->rooms[i].subsystem_hp <= 0)
                        bm->completed = true;
                }
                break;
            default: break;
        }
    }
}

/* ── Get ship room for a dungeon grid position ──────────────────── */

/*  Given a deck and a dng_room index, return the ship_room index. */
static int ship_room_for_deck_local(ship_state *ship, int deck, int local_idx) {
    if (deck < 0 || deck >= ship->num_decks) return -1;
    int start = ship->deck_room_start[deck];
    int count = ship->deck_room_count[deck];
    if (local_idx < 0 || local_idx >= count) return -1;
    return start + local_idx;
}

/* ── Ship HUD rendering ─────────────────────────────────────────── */

static void draw_ship_hud(uint32_t *px, int W, int H, const ship_state *ship) {
    uint32_t shadow = 0xFF000000;
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;
    uint32_t dim = 0xFF555555;

    int hud_x = 4;
    int hud_y = 30;
    int hud_w = 100;

    /* Semi-transparent background */
    for (int ry = hud_y; ry < hud_y + 160 && ry < H; ry++)
        for (int rx = hud_x; rx < hud_x + hud_w && rx < W; rx++) {
            uint32_t c = px[ry * W + rx];
            int cr = (c >> 0) & 0xFF;
            int cg = (c >> 8) & 0xFF;
            int cb = (c >> 16) & 0xFF;
            px[ry * W + rx] = 0xFF000000 | ((cb/3)<<16) | ((cg/3)<<8) | (cr/3);
        }

    /* Enemy ship name + hull */
    sr_draw_text_shadow(px, W, H, hud_x + 2, hud_y + 2, ship->name, 0xFF4444FF, shadow);

    /* Enemy hull bar */
    {
        char hbuf[32];
        snprintf(hbuf, sizeof(hbuf), "HULL %d/%d", ship->hull_hp, ship->hull_hp_max);
        sr_draw_text_shadow(px, W, H, hud_x + 2, hud_y + 14, hbuf, 0xFF2222CC, shadow);

        /* Bar */
        int bw = hud_w - 8;
        int bx = hud_x + 4, by = hud_y + 24;
        for (int rx = bx; rx < bx + bw && rx < W; rx++)
            if (by >= 0 && by < H) px[by * W + rx] = 0xFF333333;
        int fill = ship->hull_hp_max > 0 ? (bw * ship->hull_hp / ship->hull_hp_max) : 0;
        uint32_t bar_col = ship->hull_hp > ship->hull_hp_max / 3 ? 0xFF2222CC : 0xFF0000CC;
        for (int rx = bx; rx < bx + fill && rx < W; rx++)
            if (by >= 0 && by < H) px[by * W + rx] = bar_col;
    }

    /* Subsystem status (compact) */
    int sy = hud_y + 30;
    for (int i = 0; i < ship->room_count; i++) {
        ship_room *rm = (ship_room *)&ship->rooms[i];
        if (rm->subsystem_hp_max <= 0) continue; /* skip non-subsystem rooms */

        uint32_t col = rm->subsystem_hp > 0 ? room_type_colors[rm->type] : 0xFF444444;
        char sbuf[32];
        const char *short_name;
        switch (rm->type) {
            case ROOM_BRIDGE:  short_name = "BRG"; break;
            case ROOM_WEAPONS: short_name = "WPN"; break;
            case ROOM_ENGINES: short_name = "ENG"; break;
            case ROOM_REACTOR: short_name = "RCT"; break;
            case ROOM_SHIELDS: short_name = "SHL"; break;
            default:           short_name = "???"; break;
        }
        snprintf(sbuf, sizeof(sbuf), "%s %d", short_name, rm->subsystem_hp);
        sr_draw_text_shadow(px, W, H, hud_x + 2, sy, sbuf, col, shadow);
        sy += 10;
    }

    /* Separator */
    sy += 4;
    for (int rx = hud_x + 2; rx < hud_x + hud_w - 2 && rx < W; rx++)
        if (sy >= 0 && sy < H) px[sy * W + rx] = dim;
    sy += 6;

    /* Player ship hull */
    sr_draw_text_shadow(px, W, H, hud_x + 2, sy, "YOUR SHIP", 0xFF44CC44, shadow);
    sy += 10;
    {
        char pbuf[32];
        snprintf(pbuf, sizeof(pbuf), "HULL %d/%d", ship->player_hull_hp, ship->player_hull_hp_max);
        sr_draw_text_shadow(px, W, H, hud_x + 2, sy, pbuf, 0xFF22CC22, shadow);
        sy += 10;

        int bw = hud_w - 8;
        int bx = hud_x + 4;
        for (int rx = bx; rx < bx + bw && rx < W; rx++)
            if (sy >= 0 && sy < H) px[sy * W + rx] = 0xFF333333;
        int fill = ship->player_hull_hp_max > 0 ?
                   (bw * ship->player_hull_hp / ship->player_hull_hp_max) : 0;
        uint32_t bar_col = ship->player_hull_hp > ship->player_hull_hp_max / 3 ?
                           0xFF22CC22 : 0xFF22CC88;
        for (int rx = bx; rx < bx + fill && rx < W; rx++)
            if (sy >= 0 && sy < H) px[sy * W + rx] = bar_col;
    }
    sy += 8;

    /* Mission */
    sy += 4;
    sr_draw_text_shadow(px, W, H, hud_x + 2, sy, "MISSION:", gray, shadow);
    sy += 10;
    uint32_t mcol = ship->mission.completed ? 0xFF22CC22 : 0xFF22CCEE;
    const char *mstatus = ship->mission.completed ? "DONE" : mission_type_names[ship->mission.type];
    sr_draw_text_shadow(px, W, H, hud_x + 2, sy, mstatus, mcol, shadow);
    sy += 10;

    /* Turn counter */
    {
        char tbuf[16];
        snprintf(tbuf, sizeof(tbuf), "TURN %d", ship->turns_elapsed);
        sr_draw_text_shadow(px, W, H, hud_x + 2, sy, tbuf, dim, shadow);
    }
}

/* ── Room label overlay (shown when entering a room) ────────────── */

static void draw_room_label(uint32_t *px, int W, int H,
                            const ship_room *room, int room_ship_idx,
                            const ship_state *ship) {
    if (!room) return;
    uint32_t shadow = 0xFF000000;
    uint32_t col = room_type_colors[room->type];
    uint32_t white = 0xFFFFFFFF;
    uint32_t gray = 0xFF888888;

    /* Room name banner at bottom center */
    int bw = 140, bh = 40;
    int bx = W / 2 - bw / 2;
    int by = H - bh - 68; /* above cards */

    /* Semi-transparent background */
    for (int ry = by; ry < by + bh && ry < H; ry++)
        for (int rx = bx; rx < bx + bw && rx < W; rx++) {
            if (rx < 0 || ry < 0) continue;
            uint32_t c = px[ry * W + rx];
            int cr = ((c >> 0) & 0xFF) / 4;
            int cg = ((c >> 8) & 0xFF) / 4;
            int cb = ((c >> 16) & 0xFF) / 4;
            px[ry * W + rx] = 0xFF000000 | (cb << 16) | (cg << 8) | cr;
        }

    /* Room type name */
    sr_draw_text_shadow(px, W, H, bx + 4, by + 4,
                        room_type_names[room->type], col, shadow);

    /* Subsystem HP bar if applicable */
    if (room->subsystem_hp_max > 0) {
        char shpbuf[32];
        snprintf(shpbuf, sizeof(shpbuf), "SYS %d/%d", room->subsystem_hp, room->subsystem_hp_max);
        uint32_t shp_col = room->subsystem_hp > 0 ? col : 0xFF444444;
        sr_draw_text_shadow(px, W, H, bx + 4, by + 14, shpbuf, shp_col, shadow);

        /* HP bar */
        int bar_x = bx + 4, bar_y = by + 24, bar_w = bw - 8, bar_h = 3;
        for (int rx = bar_x; rx < bar_x + bar_w && rx < W; rx++)
            if (bar_y >= 0 && bar_y < H && rx >= 0) px[bar_y * W + rx] = 0xFF333333;
        int fill = room->subsystem_hp_max > 0 ?
                   (bar_w * room->subsystem_hp / room->subsystem_hp_max) : 0;
        for (int rx = bar_x; rx < bar_x + fill && rx < W; rx++)
            if (bar_y >= 0 && bar_y < H && rx >= 0) px[bar_y * W + rx] = col;

        /* Console prompt */
        if (room->subsystem_hp > 0) {
            sr_draw_text_shadow(px, W, H, bx + 4, by + 30,
                                "[SPACE] SABOTAGE", 0xFF22CCEE, shadow);
        } else {
            sr_draw_text_shadow(px, W, H, bx + 4, by + 30,
                                "DESTROYED", 0xFF444444, shadow);
        }
    } else if (room->type == ROOM_CARGO) {
        sr_draw_text_shadow(px, W, H, bx + 4, by + 14, "CARGO BAY", gray, shadow);
        if (!room->cleared) {
            sr_draw_text_shadow(px, W, H, bx + 4, by + 24,
                                "[SPACE] SEARCH", 0xFF22CCEE, shadow);
        } else {
            sr_draw_text_shadow(px, W, H, bx + 4, by + 24,
                                "SEARCHED", 0xFF444444, shadow);
        }
    }

    /* Show officers in this room */
    for (int o = 0; o < ship->officer_count; o++) {
        if (ship->officers[o].room_idx == room_ship_idx && ship->officers[o].alive) {
            sr_draw_text_shadow(px, W, H, bx + 4, by - 12,
                                ship->officers[o].name, 0xFF4444FF, shadow);
            break;
        }
    }
}

#endif /* SR_SHIP_H */
