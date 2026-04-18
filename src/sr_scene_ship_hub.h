/*  sr_scene_ship_hub.h - Player's own ship hub scene.
 *  Walk around your ship, talk to crew, use teleporter, shop, bridge/starmap.
 *  Single-TU header-only. Depends on sr_dungeon.h, sr_combat.h, sr_app.h. */
#ifndef SR_SCENE_SHIP_HUB_H
#define SR_SCENE_SHIP_HUB_H

/* ── Hub lighting config (loaded from config/hub.yaml) ─────────── */

static struct {
    float ambient_brightness;
    float torch_brightness;
    float fog_base;
    float fog_boost;
    float fog_near;
    float fog_falloff;
    float room_light_color[3];
    float room_light_brightness;
    float room_light_radius;
    int   draw_distance;
} hub_cfg;

static void hub_load_config(void) {
    sr_config cfg = sr_config_load("config/hub.yaml");

    hub_cfg.ambient_brightness = sr_config_float(&cfg, "ambient.brightness", 0.6f);
    hub_cfg.torch_brightness   = sr_config_float(&cfg, "torch.brightness", 0.4f);
    hub_cfg.fog_base           = sr_config_float(&cfg, "fog.base", 0.6f);
    hub_cfg.fog_boost          = sr_config_float(&cfg, "fog.boost", 0.3f);
    hub_cfg.fog_near           = sr_config_float(&cfg, "fog.near", 1.0f);
    hub_cfg.fog_falloff        = sr_config_float(&cfg, "fog.falloff", 8.0f);

    float rl_color[3] = {1.0f, 1.0f, 1.0f};
    sr_config_array(&cfg, "room_light.color", rl_color, 3);
    hub_cfg.room_light_color[0] = rl_color[0];
    hub_cfg.room_light_color[1] = rl_color[1];
    hub_cfg.room_light_color[2] = rl_color[2];
    hub_cfg.room_light_brightness = sr_config_float(&cfg, "room_light.brightness", 1.5f);
    hub_cfg.room_light_radius     = sr_config_float(&cfg, "room_light.radius", 8.0f);
    hub_cfg.draw_distance = (int)sr_config_float(&cfg, "draw_distance", 10.0f);
    if (hub_cfg.draw_distance > DNG_RENDER_R) hub_cfg.draw_distance = DNG_RENDER_R;
    if (hub_cfg.draw_distance < 1) hub_cfg.draw_distance = 1;

    sr_config_free(&cfg);
    printf("[hub] Config loaded: ambient(%.2f) torch(%.2f) fog(%.2f/%.2f/%.1f) room_light(%.1f/%.1f) draw_dist(%d)\n",
           hub_cfg.ambient_brightness, hub_cfg.torch_brightness,
           hub_cfg.fog_base, hub_cfg.fog_boost, hub_cfg.fog_falloff,
           hub_cfg.room_light_brightness, hub_cfg.room_light_radius, hub_cfg.draw_distance);
}

/* ── Hub room types ─────────────────────────────────────────────── */

enum {
    HUB_ROOM_CORRIDOR,
    HUB_ROOM_BRIDGE,
    HUB_ROOM_TELEPORTER,
    HUB_ROOM_SHOP,
    HUB_ROOM_QUARTERS,
    HUB_ROOM_MEDBAY,
    HUB_ROOM_COUNT
};

static const char *hub_room_names[] = {
    "CORRIDOR", "BRIDGE", "TELEPORTER", "ARMORY", "QUARTERS", "MEDBAY"
};

static const uint32_t hub_room_colors[] = {
    0xFF555555, 0xFF22CCEE, 0xFF22CC44, 0xFFCC8822,
    0xFF666688, 0xFF44CC44
};

/* ── NPC crew ───────────────────────────────────────────────────── */

#define HUB_MAX_CREW 6

typedef struct {
    char name[24];
    int room;           /* hub room index */
    int gx, gy;         /* grid position */
    int dialog_id;      /* which dialog set to use */
    bool active;
} hub_npc;

/* ── Star map ───────────────────────────────────────────────────── */

#define STARMAP_MAX_NODES 24
#define STARMAP_MAX_CHOICES 4

/* Node types for the star map */
enum {
    NODE_NORMAL,    /* regular derelict ship */
    NODE_MINIBOSS,  /* derelict with a miniboss fight */
    NODE_EVENT,     /* dilemma / random event (no dungeon) */
    NODE_BOSS,      /* final boss node */
    NODE_JUNKERS    /* junk traders: trade junk cards for scrap */
};

typedef struct {
    char name[24];
    char level_file[64]; /* JSON level to load for this node (empty = procedural) */
    int difficulty;
    int scrap_reward;
    bool visited;
    bool is_boss;       /* true = boss node (rightmost) */
    int boss_room;      /* room index where boss spawns (-1 = none) */
    int node_type;      /* NODE_NORMAL, NODE_MINIBOSS, NODE_EVENT, NODE_BOSS */
    int event_id;       /* index into event table (NODE_EVENT only, -1 = none) */
    int next[STARMAP_MAX_CHOICES]; /* indices of next nodes, -1 = none */
    int next_count;
    int x, y;           /* screen position for drawing */
} starmap_node;

typedef struct {
    starmap_node nodes[STARMAP_MAX_NODES];
    int node_count;
    int current_node;
    int cursor;          /* selected next node */
    bool active;
    bool confirm_active; /* showing jump confirm dialog */
    int confirm_target;  /* node index to confirm */
    int derelicts_visited; /* how many derelicts we've boarded */
    int visited_path[STARMAP_MAX_NODES]; /* ordered list of node indices visited */
    int visited_path_count;
} starmap_state;

static starmap_state g_starmap;

/* ── Space events / dilemmas ───────────────────────────────────── */

#define EVENT_MAX_CHOICES 3
#define EVENT_MAX_EVENTS  8

/* Outcome types */
enum {
    OUTCOME_SCRAP,           /* gain scrap */
    OUTCOME_BIOMASS,         /* gain biomass */
    OUTCOME_CARD,            /* gain a random good card */
    OUTCOME_HEALTH_LOSS,     /* lose HP */
    OUTCOME_JUNK_CARD,       /* add junk card to deck */
    OUTCOME_MAX_HP_LOSS,     /* permanently lose max HP */
    OUTCOME_HEAL,            /* restore HP */
};

typedef struct {
    char text[48];              /* choice button text */
    int outcome_good;           /* OUTCOME_* for success */
    int good_amount;            /* amount of reward */
    int outcome_bad;            /* OUTCOME_* for failure */
    int bad_amount;             /* amount of penalty */
    int success_chance;         /* 0-100 percent chance of good outcome */
    char good_text[64];         /* message on success */
    char bad_text[64];          /* message on failure */
} event_choice;

typedef struct {
    char title[32];
    char description[128];
    event_choice choices[EVENT_MAX_CHOICES];
    int choice_count;
} space_event;

static const space_event g_events[EVENT_MAX_EVENTS] = {
    { /* 0: Distress Signal */
        "DISTRESS SIGNAL",
        "Scanners detect a weak distress\nsignal from a damaged vessel.\nCould be survivors... or a trap.",
        {
            { "INVESTIGATE", OUTCOME_SCRAP, 40, OUTCOME_HEALTH_LOSS, 15, 60,
              "Found survivors! +40 SCRAP", "It was a trap! -15 HP" },
            { "IGNORE", OUTCOME_SCRAP, 0, OUTCOME_SCRAP, 0, 100,
              "You move on.", "" },
        }, 2
    },
    { /* 1: Anomalous Readings */
        "ANOMALOUS READINGS",
        "Strange energy signatures ahead.\nScans reveal alien technology\nof unknown origin.",
        {
            { "SCAN CLOSER", OUTCOME_CARD, 1, OUTCOME_JUNK_CARD, 2, 50,
              "Found alien tech! +1 CARD", "Corrupted data! +2 JUNK CARDS" },
            { "KEEP DISTANCE", OUTCOME_SCRAP, 10, OUTCOME_SCRAP, 10, 100,
              "Cautious scan. +10 SCRAP", "" },
        }, 2
    },
    { /* 2: Ruins on a Planet */
        "PLANETARY RUINS",
        "Ancient ruins on a nearby planet.\nBiomass readings are strong but\nthe structure looks unstable.",
        {
            { "EXPLORE RUINS", OUTCOME_BIOMASS, 25, OUTCOME_MAX_HP_LOSS, 8, 55,
              "Rich biomass deposits! +25 BIO", "Collapse! -8 MAX HP permanently" },
            { "SURFACE SCAN", OUTCOME_BIOMASS, 8, OUTCOME_BIOMASS, 8, 100,
              "Surface samples. +8 BIOMASS", "" },
        }, 2
    },
    { /* 3: Abandoned Cargo Pod */
        "ABANDONED CARGO",
        "A jettisoned cargo pod drifts\nin the void. Salvage markings\nsuggest military equipment.",
        {
            { "SALVAGE IT", OUTCOME_SCRAP, 50, OUTCOME_HEALTH_LOSS, 20, 65,
              "Military supplies! +50 SCRAP", "Booby-trapped! -20 HP" },
            { "LEAVE IT", OUTCOME_SCRAP, 0, OUTCOME_SCRAP, 0, 100,
              "Better safe than sorry.", "" },
        }, 2
    },
    { /* 4: Strange Transmission */
        "STRANGE TRANSMISSION",
        "An encoded signal repeats on\nloop. Your AI can try to decode\nit but risks system damage.",
        {
            { "DECODE IT", OUTCOME_CARD, 2, OUTCOME_JUNK_CARD, 3, 45,
              "Intel decoded! +2 CARDS", "System corrupted! +3 JUNK CARDS" },
            { "BLOCK SIGNAL", OUTCOME_SCRAP, 15, OUTCOME_SCRAP, 15, 100,
              "Signal blocked. +15 SCRAP", "" },
        }, 2
    },
    { /* 5: Alien Artifact */
        "ALIEN ARTIFACT",
        "A pulsing artifact floats in\nspace. It radiates power but\nalso an unsettling presence.",
        {
            { "TOUCH IT", OUTCOME_BIOMASS, 30, OUTCOME_MAX_HP_LOSS, 10, 40,
              "Power absorbed! +30 BIOMASS", "Cursed! -10 MAX HP permanently" },
            { "SELL TO TRADERS", OUTCOME_SCRAP, 35, OUTCOME_SCRAP, 35, 100,
              "Traded for parts. +35 SCRAP", "" },
            { "ABSORB + RISK", OUTCOME_CARD, 1, OUTCOME_HEALTH_LOSS, 25, 50,
              "Gained alien weapon! +1 CARD", "Energy backlash! -25 HP" },
        }, 3
    },
    { /* 6: Derelict Medbay */
        "DERELICT MEDBAY",
        "A destroyed ship's medbay still\nhas power. Medical supplies\nremain but so do parasites.",
        {
            { "ENTER MEDBAY", OUTCOME_HEAL, 30, OUTCOME_HEALTH_LOSS, 10, 60,
              "Medical supplies! +30 HP", "Parasite attack! -10 HP" },
            { "SCAVENGE OUTSIDE", OUTCOME_SCRAP, 20, OUTCOME_SCRAP, 20, 100,
              "Found spare parts. +20 SCRAP", "" },
        }, 2
    },
    { /* 7: Void Storm */
        "VOID STORM",
        "A subspace anomaly creates a\nvoid storm. You can ride it\nfor a boost or wait it out.",
        {
            { "RIDE THE STORM", OUTCOME_SCRAP, 35, OUTCOME_MAX_HP_LOSS, 5, 50,
              "Storm boost! +35 SCRAP", "Hull stress! -5 MAX HP permanently" },
            { "WAIT IT OUT", OUTCOME_BIOMASS, 5, OUTCOME_BIOMASS, 5, 100,
              "Collected ambient spores. +5 BIO", "" },
        }, 2
    },
};

/* Event state machine */
typedef struct {
    bool active;
    int event_id;           /* index into g_events */
    int result_timer;       /* countdown for showing result */
    char result_text[64];   /* outcome message */
    bool showing_result;
} event_state;

static event_state g_event;

/* ── Junkers trade state ───────────────────────────────────────── */

#define JUNKERS_MAX_TRADES 3   /* max junk cards removed per visit */
#define JUNKERS_SCRAP_PER_JUNK 15  /* scrap reward per junk traded */

typedef struct {
    bool active;
    int  junk_traded;      /* how many junk cards traded this visit */
} junkers_state;

static junkers_state g_junkers;

/* Count CARD_JUNK in persistent deck */
static int junkers_count_junk(void) {
    int count = 0;
    for (int i = 0; i < g_player.persistent_deck_count; i++)
        if (g_player.persistent_deck[i] == CARD_JUNK) count++;
    return count;
}

/* Remove one CARD_JUNK from persistent deck, return true if removed */
static bool junkers_remove_one_junk(void) {
    for (int i = 0; i < g_player.persistent_deck_count; i++) {
        if (g_player.persistent_deck[i] == CARD_JUNK) {
            /* Shift remaining cards down */
            for (int j = i; j < g_player.persistent_deck_count - 1; j++)
                g_player.persistent_deck[j] = g_player.persistent_deck[j + 1];
            g_player.persistent_deck_count--;
            return true;
        }
    }
    return false;
}

/* ── Shop state ─────────────────────────────────────────────────── */

#define SHOP_MAX_CARDS 8

typedef struct {
    int cards[SHOP_MAX_CARDS];      /* card types for sale */
    int prices[SHOP_MAX_CARDS];     /* cost (scrap or biomass depending on is_bio) */
    bool is_bio[SHOP_MAX_CARDS];    /* true = costs biomass (elemental), false = costs scrap */
    int count;
    int cursor;
    int mode;            /* 0=buy, 1=trash */
    int trash_cursor;    /* cursor into player deck for trashing */
    int trash_count;     /* number of cards trashed this visit */
    bool active;
    int detail_idx;      /* shop card index shown in detail popup, -1 = none */
    bool detail_open;
} shop_state;

static shop_state g_shop;       /* armory (scrap cards) */
static shop_state g_medbay_shop; /* medbay (elemental/biomass cards) */

/* Persistent medbay health-kit stock (restocks by 1 per jump, max 2). */
#define MEDBAY_KIT_STOCK_MAX 2
static int g_medbay_kit_stock = MEDBAY_KIT_STOCK_MAX;

/* Teleporter elemental gift state - one-time gift before the first
   boss mission. elem_gift_active is the overlay visibility flag,
   g_elem_gift_given persists across jumps/saves so the gift only
   triggers once per run. */
static bool elem_gift_active = false;
static bool g_elem_gift_given = false;
static int  elem_gift_choices[3];

/* Roll 3 unique elemental card choices and show the gift overlay. */
static void teleporter_offer_elem_gift(void) {
    int elems[] = { CARD_ICE, CARD_ACID, CARD_FIRE, CARD_LIGHTNING };
    for (int i = 3; i > 0; i--) {
        int j = dng_rng_int(i + 1);
        int tmp = elems[i]; elems[i] = elems[j]; elems[j] = tmp;
    }
    for (int i = 0; i < 3; i++) elem_gift_choices[i] = elems[i];
    elem_gift_active = true;
}

/* Which shop is active for STATE_SHOP - 0=armory, 1=medbay */
static int active_shop_type = 0;

/* ── Dialog state ───────────────────────────────────────────────── */

#define DIALOG_MAX_LINES 4
#define DIALOG_LINE_LEN 72

enum {
    DIALOG_ACTION_NONE,
    DIALOG_ACTION_STARMAP,
    DIALOG_ACTION_SHOP,
    DIALOG_ACTION_TELEPORT,       /* opens confirm prompt */
    DIALOG_ACTION_TELEPORT_GO,    /* confirmed - actually teleport */
    DIALOG_ACTION_HEAL,
    DIALOG_ACTION_BRIEFING_NEXT,  /* captain briefing: advance to next page */
    DIALOG_ACTION_SHOW_KIT,       /* Chen: show starting deck cards */
    DIALOG_ACTION_MEDBAY_SHOP,    /* Vasquez: open medbay shop (elemental/biomass) */
};

typedef struct {
    char lines[DIALOG_MAX_LINES][DIALOG_LINE_LEN];
    int line_count;
    char speaker[24];
    bool active;
    int pending_action;    /* action to trigger on dismiss */
    bool confirm_mode;     /* true = show YES/NO buttons */
    /* Teletype state */
    int tt_line;           /* current line being typed (0..line_count-1) */
    int tt_timer;          /* frame counter for current line */
    bool tt_all_done;      /* all lines fully revealed */
    /* Button click flags (set by draw, consumed by event loop) */
    bool btn_dismiss;      /* CONTINUE/CLOSE clicked */
    bool btn_yes;          /* YES clicked (confirm mode) */
    bool btn_no;           /* NO clicked (confirm mode) */
} dialog_state;

static dialog_state g_dialog;

/* ── Kit display overlay (Chen first visit) ────────────────────── */

typedef struct {
    bool active;
    int card_count;
    int cards[COMBAT_DECK_MAX]; /* copy of persistent deck */
    int detail_idx;        /* -1 = none, >=0 = showing card detail */
    int scroll;            /* scroll offset for card grid */
} kit_display_state;

static kit_display_state g_kit;

/* ── Hub state ──────────────────────────────────────────────────── */

typedef struct {
    sr_dungeon dungeon;
    dng_player player;
    hub_npc crew[HUB_MAX_CREW];
    int crew_count;
    int room_types[12];  /* hub room type for each dungeon room */
    bool initialized;
    bool mission_available;
    char hud_msg[64];
    int hud_msg_timer;
} hub_state;

static hub_state g_hub;

/* ── Hub dungeon generation (fixed layout) ──────────────────────── */

static void hub_generate(hub_state *hub) {
    memset(hub, 0, sizeof(*hub));
    sr_dungeon *d = &hub->dungeon;

    /* Try to load hub from JSON level file */
    const char *hub_path = "levels/hub.json";
    if (lvl_file_exists(hub_path)) {
        lvl_loaded lvl = lvl_load(hub_path);
        if (lvl.valid && lvl.is_hub && lvl.num_floors > 0) {
            printf("[hub] Loading hub layout from %s\n", hub_path);
            int floor_tok = sr_json_array_get(&lvl.json, lvl.floors_arr, 0);
            lvl_load_floor(d, &lvl.json, floor_tok);

            /* Load NPCs from floor data */
            int npcs_arr = sr_json_find(&lvl.json, floor_tok, "npcs");
            if (npcs_arr >= 0) {
                hub->crew_count = sr_json_array_len(&lvl.json, npcs_arr);
                if (hub->crew_count > HUB_MAX_CREW) hub->crew_count = HUB_MAX_CREW;
                for (int i = 0; i < hub->crew_count; i++) {
                    int npc_tok = sr_json_array_get(&lvl.json, npcs_arr, i);
                    if (npc_tok < 0) continue;
                    hub_npc *npc = &hub->crew[i];
                    sr_json_str(&lvl.json, sr_json_find(&lvl.json, npc_tok, "name"),
                                npc->name, 24);
                    npc->gx = sr_json_int(&lvl.json, sr_json_find(&lvl.json, npc_tok, "gX"), 0);
                    npc->gy = sr_json_int(&lvl.json, sr_json_find(&lvl.json, npc_tok, "gY"), 0);
                    npc->dialog_id = sr_json_int(&lvl.json, sr_json_find(&lvl.json, npc_tok, "dialogId"), i);
                    npc->room = i; /* map NPC index to room index */
                    npc->active = true;
                }
            }

            /* Map room types from JSON room type strings + place consoles */
            {
                /* Map JSON room type name -> hub room type */
                static const struct { const char *name; int hub_type; int ship_type; } hub_type_map[] = {
                    { "Bridge",     HUB_ROOM_BRIDGE,     ROOM_BRIDGE },
                    { "Teleporter", HUB_ROOM_TELEPORTER, ROOM_SHIELDS },
                    { "Cargo",      HUB_ROOM_SHOP,       ROOM_CARGO },
                    { "Barracks",   HUB_ROOM_QUARTERS,   ROOM_BARRACKS },
                    { "Medbay",     HUB_ROOM_MEDBAY,     ROOM_MEDBAY },
                };
                int rooms_arr = sr_json_find(&lvl.json, floor_tok, "rooms");
                for (int i = 0; i < d->room_count; i++) {
                    hub->room_types[i] = HUB_ROOM_CORRIDOR;
                    d->room_light_on[i] = true;

                    /* Check if consoles already placed by JSON */
                    int cx = d->room_cx[i], cy = d->room_cy[i];
                    if (cx >= 1 && cx <= d->w && cy >= 1 && cy <= d->h && d->consoles[cy][cx] > 0) {
                        /* Infer hub type from console's ship room type */
                        for (int m = 0; m < 5; m++)
                            if (d->consoles[cy][cx] == hub_type_map[m].ship_type)
                                hub->room_types[i] = hub_type_map[m].hub_type;
                        continue;
                    }

                    /* Parse room type from JSON and place console */
                    if (rooms_arr >= 0) {
                        int room_tok = sr_json_array_get(&lvl.json, rooms_arr, i);
                        if (room_tok >= 0) {
                            int type_tok = sr_json_find(&lvl.json, room_tok, "type");
                            if (type_tok >= 0) {
                                char tbuf[24] = {0};
                                sr_json_str(&lvl.json, type_tok, tbuf, sizeof(tbuf));
                                for (int m = 0; m < 5; m++) {
                                    if (strcmp(tbuf, hub_type_map[m].name) == 0) {
                                        hub->room_types[i] = hub_type_map[m].hub_type;
                                        if (cx >= 1 && cx <= d->w && cy >= 1 && cy <= d->h)
                                            d->consoles[cy][cx] = (uint8_t)hub_type_map[m].ship_type;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            printf("[hub] Player spawn: (%d,%d) dir=%d\n", d->spawn_gx, d->spawn_gy, d->spawn_dir);
            dng_player_init(&hub->player, d->spawn_gx, d->spawn_gy, d->spawn_dir);

            /* Place NPC sprites in the dungeon grid */
            {
                static const int crew_stex[] = {
                    STEX_CREW_CAPTAIN, STEX_CREW_SERGEANT, STEX_CREW_QUARTERMASTER,
                    STEX_CREW_PRIVATE, STEX_CREW_DOCTOR, STEX_CREW_BYTOR
                };
                for (int i = 0; i < hub->crew_count; i++) {
                    hub_npc *npc = &hub->crew[i];
                    if (!npc->active) continue;
                    int stex = (i < 6) ? crew_stex[i] : STEX_CREW_CAPTAIN;
                    if (npc->gx >= 1 && npc->gx <= d->w && npc->gy >= 1 && npc->gy <= d->h) {
                        d->aliens[npc->gy][npc->gx] = (uint8_t)(stex + 1);
                        snprintf(d->alien_names[npc->gy][npc->gx], 16, "%s", npc->name);
                        printf("[hub] NPC %d '%s' at (%d,%d) stex=%d aliens_val=%d pixels=%p\n",
                               i, npc->name, npc->gx, npc->gy, stex, stex+1,
                               (void*)stextures[stex].pixels);
                    } else {
                        printf("[hub] NPC %d '%s' OUT OF BOUNDS at (%d,%d) w=%d h=%d\n",
                               i, npc->name, npc->gx, npc->gy, d->w, d->h);
                    }
                }
            }

            hub->mission_available = true;
            hub->initialized = true;
            game_pregen_enemy_ship();
            printf("[hub] Loaded: %d rooms, %d NPCs\n", d->room_count, hub->crew_count);
            return;
        }
    }

    /* Fallback: hardcoded hub layout */
    d->w = 20;
    d->h = 20;
    d->has_up = false;
    d->has_down = false;

    /* Fill with walls */
    for (int y = 1; y <= d->h; y++)
        for (int x = 1; x <= d->w; x++)
            d->map[y][x] = 1;

    int mid_y = d->h / 2;

    /* Central corridor (1 tile wide) */
    for (int x = 3; x <= 18; x++) {
        d->map[mid_y][x] = 0;
    }

    /* Room definitions: {x, y, w, h, type}
     * Rooms are set back 1 tile from corridor so there's a wall gap.
     * A single doorway tile connects through the gap. */
    struct { int x, y, w, h, type; } room_defs[] = {
        /* Bridge at the front (right side) - 1 wall gap above corridor */
        { 15, mid_y - 5, 4, 4, HUB_ROOM_BRIDGE },
        /* Teleporter at the back (left side) */
        { 3, mid_y - 5, 4, 4, HUB_ROOM_TELEPORTER },
        /* Shop/Armory upper-middle */
        { 9, mid_y - 5, 4, 4, HUB_ROOM_SHOP },
        /* Quarters below-left - 1 wall gap below corridor */
        { 5, mid_y + 2, 4, 4, HUB_ROOM_QUARTERS },
        /* Medbay below-right */
        { 12, mid_y + 2, 4, 4, HUB_ROOM_MEDBAY },
    };
    int num_rooms = 5;

    d->room_count = num_rooms;
    for (int i = 0; i < num_rooms; i++) {
        int rx = room_defs[i].x;
        int ry = room_defs[i].y;
        int rw = room_defs[i].w;
        int rh = room_defs[i].h;
        hub->room_types[i] = room_defs[i].type;

        /* Carve room */
        for (int py = ry; py < ry + rh; py++)
            for (int px = rx; px < rx + rw; px++)
                if (py >= 1 && py <= d->h && px >= 1 && px <= d->w)
                    d->map[py][px] = 0;

        d->room_x[i] = rx;
        d->room_y[i] = ry;
        d->room_w[i] = rw;
        d->room_h[i] = rh;
        d->room_cx[i] = rx + rw / 2;
        d->room_cy[i] = ry + rh / 2;
        d->room_ship_idx[i] = -1;
        d->room_light_on[i] = true;

        /* Connect room to corridor with 1-wide doorway */
        int conn_x = rx + rw / 2;
        if (ry + rh <= mid_y) {
            for (int y = ry + rh; y <= mid_y; y++)
                if (y >= 1 && y <= d->h && conn_x >= 1 && conn_x <= d->w)
                    d->map[y][conn_x] = 0;
        } else if (ry > mid_y) {
            for (int y = mid_y; y < ry; y++)
                if (y >= 1 && y <= d->h && conn_x >= 1 && conn_x <= d->w)
                    d->map[y][conn_x] = 0;
        }
    }

    /* Spawn in front of the teleporter room, facing north toward it */
    d->spawn_gx = 2;
    d->spawn_gy = mid_y;
    d->spawn_dir = 1; /* 0=N, 1=E, 2=S, 3=W */

    dng_player_init(&hub->player, d->spawn_gx, d->spawn_gy, d->spawn_dir);

    /* Place crew NPCs */
    hub->crew_count = 5;

    /* Captain on bridge */
    hub->crew[0] = (hub_npc){
        .name = "CPT HARDEN", .room = 0, .dialog_id = 0, .active = true,
        .gx = 16, .gy = mid_y - 3
    };
    /* Teleporter tech */
    hub->crew[1] = (hub_npc){
        .name = "SGT REYES", .room = 1, .dialog_id = 1, .active = true,
        .gx = 4, .gy = mid_y - 3
    };
    /* Shopkeeper */
    hub->crew[2] = (hub_npc){
        .name = "QM CHEN", .room = 2, .dialog_id = 2, .active = true,
        .gx = 10, .gy = mid_y - 3
    };
    /* Crew in quarters */
    hub->crew[3] = (hub_npc){
        .name = "PVT KOWALSKI", .room = 3, .dialog_id = 3, .active = true,
        .gx = 6, .gy = mid_y + 3
    };
    /* Medic */
    hub->crew[4] = (hub_npc){
        .name = "DR VASQUEZ", .room = 4, .dialog_id = 4, .active = true,
        .gx = 13, .gy = mid_y + 3
    };

    /* Place NPC sprites using crew-specific textures */
    {
        static const int crew_stex[] = {
            STEX_CREW_CAPTAIN, STEX_CREW_SERGEANT, STEX_CREW_QUARTERMASTER,
            STEX_CREW_PRIVATE, STEX_CREW_DOCTOR
        };
        for (int i = 0; i < hub->crew_count; i++) {
            hub_npc *npc = &hub->crew[i];
            if (!npc->active) continue;
            if (npc->gx >= 1 && npc->gx <= d->w && npc->gy >= 1 && npc->gy <= d->h) {
                d->aliens[npc->gy][npc->gx] = (uint8_t)(crew_stex[i] + 1);
                snprintf(d->alien_names[npc->gy][npc->gx], 16, "%s", npc->name);
            }
        }
    }

    /* Place consoles at room centers for minimap room type detection */
    {
        /* Map hub room types to ship ROOM_* values for expanded map coloring */
        static const int hub_to_ship_room[] = {
            0,              /* HUB_ROOM_CORRIDOR -> ROOM_CORRIDOR */
            ROOM_BRIDGE,    /* HUB_ROOM_BRIDGE */
            ROOM_TELEPORTER,/* HUB_ROOM_TELEPORTER */
            ROOM_CARGO,     /* HUB_ROOM_SHOP (armory) -> CARGO color */
            ROOM_BARRACKS,  /* HUB_ROOM_QUARTERS -> BARRACKS color */
            ROOM_MEDBAY,    /* HUB_ROOM_MEDBAY */
        };
        for (int i = 0; i < num_rooms; i++) {
            int rt = room_defs[i].type;
            if (rt > 0 && rt < HUB_ROOM_COUNT) {
                int cx = d->room_cx[i], cy = d->room_cy[i];
                if (cx >= 1 && cx <= d->w && cy >= 1 && cy <= d->h)
                    d->consoles[cy][cx] = (uint8_t)hub_to_ship_room[rt];
            }
        }
    }

    hub->mission_available = true;
    hub->initialized = true;
    game_pregen_enemy_ship();
}

/* ── Hub lighting (60% ambient, depth-based, no point lights) ───── */

static float hub_fog_vertex_intensity(float wx, float wy, float wz) {
    dng_player *p = &g_hub.player;
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz) / DNG_CELL_SIZE;

    /* ambient + fog_base + fog_boost = max brightness (near player) */
    /* ambient + fog_base             = min brightness (far away)   */
    float base = hub_cfg.ambient_brightness + hub_cfg.fog_base;
    if (dist > hub_cfg.fog_near) {
        float fade = 1.0f - (dist - hub_cfg.fog_near) / hub_cfg.fog_falloff;
        if (fade < 0.0f) fade = 0.0f;
        base += hub_cfg.fog_boost * fade;
    } else {
        base += hub_cfg.fog_boost;
    }

    /* Room ceiling light */
    if (dng_cur_room_light >= 0) {
        float rl_radius = hub_cfg.room_light_radius;
        float ldx = dng_cur_rl_x - wx;
        float ldy = dng_cur_rl_y - wy;
        float ldz = dng_cur_rl_z - wz;
        float ldist = sqrtf(ldx*ldx + ldy*ldy + ldz*ldz);

        if (ldist < rl_radius) {
            float la;
            if (ldist <= 0.5f) {
                la = 1.0f;
            } else {
                la = 1.0f - (ldist - 0.5f) / (rl_radius - 0.5f);
                la *= la;
            }

            float rl_lum = hub_cfg.room_light_brightness *
                (hub_cfg.room_light_color[0] + hub_cfg.room_light_color[1] + hub_cfg.room_light_color[2]) / 3.0f;
            base += rl_lum * la;
        }
    }

    if (base > 1.0f) base = 1.0f;
    return base;
}

/* ── Hub NPC interaction ────────────────────────────────────────── */

static int hub_npc_at(int gx, int gy) {
    for (int i = 0; i < g_hub.crew_count; i++) {
        if (g_hub.crew[i].active && g_hub.crew[i].gx == gx && g_hub.crew[i].gy == gy)
            return i;
    }
    return -1;
}

static int hub_room_at_pos(int gx, int gy) {
    sr_dungeon *d = &g_hub.dungeon;
    for (int i = 0; i < d->room_count; i++) {
        if (gx >= d->room_x[i] && gx < d->room_x[i] + d->room_w[i] &&
            gy >= d->room_y[i] && gy < d->room_y[i] + d->room_h[i])
            return i;
    }
    return -1;
}

/* ── Dialog system ──────────────────────────────────────────────── */

/* Helper: fill dialog from a dlgd_block */
static void dialog_from_block(const dlgd_block *blk) {
    g_dialog.line_count = blk->count;
    for (int i = 0; i < blk->count && i < DIALOG_MAX_LINES; i++)
        snprintf(g_dialog.lines[i], DIALOG_LINE_LEN, "%s", blk->lines[i]);
}

static void hub_start_dialog(int npc_idx, int action) {
    if (npc_idx < 0 || npc_idx >= g_hub.crew_count) return;
    hub_npc *npc = &g_hub.crew[npc_idx];
    memset(&g_dialog, 0, sizeof(g_dialog));
    snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "%s", npc->name);

    int did = npc->dialog_id;
    if (did < 0 || did > 5) did = 0;

    /* Captain (dialog_id 0) - complex state machine */
    if (did == 0) {
        if (!mission_briefed) {
            /* First time: start multi-page briefing */
            captain_briefing_page = 0;
            if (g_dlgd.loaded && g_dlgd.captain_briefing_pages > 0) {
                dialog_from_block(&g_dlgd.captain_briefing[0]);
                if (g_dlgd.captain_briefing_pages > 1)
                    action = DIALOG_ACTION_BRIEFING_NEXT;
                else {
                    mission_briefed = true;
                    action = DIALOG_ACTION_NONE;
                }
            }
        } else if (current_map_boss_done && !g_hub.mission_available) {
            /* Just beat a boss - show sample dialog, then open star map for next run */
            int si = player_samples - 1;
            if (si < 0) si = 0; if (si > 2) si = 2;
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_sample[si]);
            action = DIALOG_ACTION_STARMAP;
        } else if (g_hub.mission_available) {
            /* Mission available - board the derelict */
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_under_attack);
        } else {
            /* Neutralized - ready to jump */
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_post_mission);
            action = DIALOG_ACTION_STARMAP;
        }
    }
    /* SGT REYES (dialog_id 1) - teleporter */
    else if (did == 1) {
        if (!mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[1]);
            action = DIALOG_ACTION_NONE;
        } else if (!mission_medbay_done || !mission_armory_done) {
            /* Block teleport until prep is done */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_reyes_blocked);
            action = DIALOG_ACTION_NONE;
        } else {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[1]);
        }
    }
    /* QM CHEN (dialog_id 2) - armory */
    else if (did == 2) {
        if (!mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[2]);
            action = DIALOG_ACTION_NONE;
        } else if (!mission_armory_done) {
            /* First visit after briefing: show starting deck cards */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_chen);
            action = DIALOG_ACTION_SHOW_KIT;
        } else {
            /* Subsequent visits: open armory shop (scrap cards) */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[2]);
            action = DIALOG_ACTION_SHOP;
        }
    }
    /* PVT KOWALSKI (dialog_id 3) - quarters, gets progressively stressed */
    else if (did == 3) {
        if (!mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[3]);
        } else if (g_dlgd.loaded) {
            int stress;
            if (current_mission_is_boss)     stress = 3;
            else if (player_sector >= 3)     stress = 2;
            else if (player_sector >= 1)     stress = 1;
            else                             stress = 0;
            if (g_dlgd.kowalski_stress[stress].count > 0)
                dialog_from_block(&g_dlgd.kowalski_stress[stress]);
            else
                dialog_from_block(&g_dlgd.crew_default[3]);
        }
        action = DIALOG_ACTION_NONE;
    }
    /* DR VASQUEZ (dialog_id 4) - medbay */
    else if (did == 4) {
        if (!mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[4]);
            action = DIALOG_ACTION_NONE;
        } else if (!mission_medbay_done) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_vasquez);
            action = DIALOG_ACTION_HEAL;
        } else {
            /* Subsequent visits: open medbay shop (elemental/biomass cards) */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[4]);
            action = DIALOG_ACTION_MEDBAY_SHOP;
        }
    }
    /* PVT FIREMANN (dialog_id 5) - friendly alien, Kowalski's friend */
    else if (did == 5) {
        if (!mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.bytor_init);
        } else {
            /* Progressive dialog: hopeful early, reflective late, emotional pre-boss */
            bool near_boss = (g_starmap.active && g_starmap.node_count > 0 &&
                g_starmap.nodes[g_starmap.node_count - 1].is_boss &&
                player_sector >= g_starmap.nodes[g_starmap.node_count - 1].difficulty - 1);
            if (near_boss && g_dlgd.bytor_pre_boss.count > 0) {
                dialog_from_block(&g_dlgd.bytor_pre_boss);
            } else {
                int stage = g_starmap.derelicts_visited;
                if (stage > 2) stage = 2;
                if (g_dlgd.bytor_default[stage].count > 0)
                    dialog_from_block(&g_dlgd.bytor_default[stage]);
                else
                    dialog_from_block(&g_dlgd.bytor_init);
            }
        }
        action = DIALOG_ACTION_NONE;
    }

    g_dialog.pending_action = action;
    g_dialog.active = true;
}

/* Start dialog for a room interaction (F key with no NPC, or NPC with room action) */
static int hub_room_action_for_type(int room_type) {
    switch (room_type) {
        case HUB_ROOM_BRIDGE:     return DIALOG_ACTION_STARMAP;
        case HUB_ROOM_TELEPORTER: return DIALOG_ACTION_TELEPORT;
        case HUB_ROOM_SHOP:       return DIALOG_ACTION_SHOP;
        case HUB_ROOM_MEDBAY:     return DIALOG_ACTION_HEAL;
        default:                  return DIALOG_ACTION_NONE;
    }
}

static void hub_show_teleport_confirm(void) {
    memset(&g_dialog, 0, sizeof(g_dialog));
    snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "TELEPORTER");
    snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "ARE YOU READY TO TELEPORT TO THE ENEMY SHIP?");
    g_dialog.line_count = 1;
    g_dialog.pending_action = DIALOG_ACTION_TELEPORT_GO;
    g_dialog.confirm_mode = true;
    g_dialog.active = true;
}

/* Count total chars across all dialog lines */
static int dialog_total_chars(void) {
    int total = 0;
    for (int i = 0; i < g_dialog.line_count; i++)
        total += (int)strlen(g_dialog.lines[i]);
    return total;
}

/* Advance teletype by one frame. */
static void dialog_teletype_tick(void) {
    if (!g_dialog.active || g_dialog.tt_all_done) return;
    g_dialog.tt_timer++;
    if (g_dialog.tt_timer * 2 >= dialog_total_chars())
        g_dialog.tt_all_done = true;
}

/* Handle CONTINUE click - instantly reveal all, or dismiss if already done. */
static bool dialog_teletype_advance(void) {
    if (g_dialog.tt_all_done) return true;
    g_dialog.tt_all_done = true;
    return false;
}

static void draw_dialog(uint32_t *px, int W, int H) {
    if (!g_dialog.active) return;
    uint32_t shadow = 0xFF000000;

    dialog_teletype_tick();

    /* Dialog box - spans most of screen width */
    int bx = 20, by = H - 66, bw = W - 40, bh = 52;
    for (int ry = by; ry < by + bh && ry < H; ry++)
        for (int rx = bx; rx < bx + bw && rx < W; rx++) {
            if (rx < 0 || ry < 0) continue;
            px[ry * W + rx] = 0xFF111122;
        }
    /* Border */
    for (int rx = bx; rx < bx + bw && rx < W; rx++) {
        if (by >= 0 && by < H) px[by * W + rx] = 0xFF4444AA;
        if (by + bh - 1 >= 0 && by + bh - 1 < H) px[(by + bh - 1) * W + rx] = 0xFF4444AA;
    }
    for (int ry = by; ry < by + bh && ry < H; ry++) {
        if (bx >= 0 && bx < W) px[ry * W + bx] = 0xFF4444AA;
        if (bx + bw - 1 >= 0 && bx + bw - 1 < W) px[ry * W + bx + bw - 1] = 0xFF4444AA;
    }

    /* Speaker name */
    sr_draw_text_shadow(px, W, H, bx + 4, by + 4, g_dialog.speaker, 0xFF00DDDD, shadow);

    /* Dialog lines with teletype effect - flows across all lines */
    {
        int chars_left = g_dialog.tt_all_done ? 99999 : g_dialog.tt_timer * 2;
        int cursor_x = bx + 4, cursor_y = by + 16;
        for (int i = 0; i < g_dialog.line_count && chars_left > 0; i++) {
            int llen = (int)strlen(g_dialog.lines[i]);
            int show = (chars_left >= llen) ? llen : chars_left;
            char partial[DIALOG_LINE_LEN];
            memcpy(partial, g_dialog.lines[i], show);
            partial[show] = '\0';
            sr_draw_text_shadow(px, W, H, bx + 4, by + 16 + i * 10,
                                partial, 0xFFCCCCCC, shadow);
            cursor_x = bx + 4 + show * 6;
            cursor_y = by + 16 + i * 10;
            chars_left -= llen;
        }
        /* Blinking cursor while typing */
        if (!g_dialog.tt_all_done && (g_dialog.tt_timer / 8) % 2 == 0)
            sr_draw_text_shadow(px, W, H, cursor_x, cursor_y, "_", 0xFFCCCCCC, shadow);
    }

    /* Buttons - attached tab below the box, right side */
    int tab_y = by + bh;
    if (g_dialog.confirm_mode && g_dialog.tt_all_done) {
        /* YES / NO buttons only after all text revealed */
        if (ui_button(px, W, H, bx + bw - 140, tab_y, 60, 14,
                      "YES", 0xFF112211, 0xFF223322, 0xFF44CC44)) {
            g_dialog.btn_yes = true;
        }
        if (ui_button(px, W, H, bx + bw - 70, tab_y, 60, 14,
                      "NO", 0xFF221111, 0xFF332222, 0xFF882222)) {
            g_dialog.btn_no = true;
        }
        /* Connect tabs to box - draw border continuation */
        for (int rx = bx + bw - 140; rx < bx + bw - 140 + 130 && rx < W; rx++)
            if (tab_y - 1 >= 0 && tab_y - 1 < H && rx >= 0)
                px[(tab_y - 1) * W + rx] = 0xFF111122;
    } else {
        const char *dismiss_label;
        if (!g_dialog.tt_all_done)
            dismiss_label = "CONTINUE";
        else if (g_dialog.pending_action != DIALOG_ACTION_NONE)
            dismiss_label = "CONTINUE";
        else
            dismiss_label = "CLOSE";
        int btn_w = 70;
        int btn_x = bx + bw - btn_w - 5;
        if (ui_button(px, W, H, btn_x, tab_y, btn_w, 14,
                      dismiss_label, 0xFF1A1A33, 0xFF222255, 0xFF44CC44)) {
            g_dialog.btn_dismiss = true;
        }
        /* Erase border where tab attaches to box */
        for (int rx = btn_x; rx < btn_x + btn_w && rx < W; rx++)
            if (tab_y - 1 >= 0 && tab_y - 1 < H && rx >= 0)
                px[(tab_y - 1) * W + rx] = 0xFF111122;
    }
}

/* ── Kit display overlay (Chen gives starting deck) ────────────── */

static void draw_kit_display(uint32_t *px, int W, int H) {
    if (!g_kit.active) return;
    uint32_t shadow = 0xFF000000;

    /* Dark overlay */
    for (int i = 0; i < W * H; i++) {
        uint32_t p = px[i];
        int r = ((p >> 0) & 0xFF) / 3;
        int g = ((p >> 8) & 0xFF) / 3;
        int b = ((p >> 16) & 0xFF) / 3;
        px[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }

    /* Title */
    sr_draw_text_shadow(px, W, H, 10, 4, "YOUR STARTING KIT", 0xFF00DDDD, shadow);

    /* Card grid */
    int cw = 50, ch = 66, pad = 6;
    int cols = 7;
    for (int i = 0; i < g_kit.card_count; i++) {
        int col = i % cols, row = i / cols;
        int cx = 10 + col * (cw + pad);
        int cy = 14 + row * (ch + pad);
        bool sel = false;
        combat_draw_card_content(px, W, H, cx, cy, cw, ch,
                                 g_kit.cards[i], sel, shadow, -1);
    }

    /* CLOSE button */
    if (ui_button(px, W, H, W - 60, H - 18, 50, 14,
                  "CLOSE", 0xFF1A1A33, 0xFF222255, 0xFF44CC44)) {
        g_kit.active = false;
    }

    /* Detail overlay if a card is selected */
    if (g_kit.detail_idx >= 0 && g_kit.detail_idx < g_kit.card_count) {
        /* Centered large card */
        int dw = 100, dh = 140;
        int dx = (W - dw) / 2, dy = (H - dh) / 2 - 10;
        /* Dark backdrop behind detail */
        for (int ry = dy - 4; ry < dy + dh + 4 && ry < H; ry++)
            for (int rx = dx - 4; rx < dx + dw + 4 && rx < W; rx++)
                if (rx >= 0 && ry >= 0)
                    px[ry * W + rx] = 0xFF000000;
        combat_draw_card_content(px, W, H, dx, dy, dw, dh,
                                 g_kit.cards[g_kit.detail_idx], true, shadow, -1);
        /* Card long description */
        const char *desc = card_description_text(g_kit.cards[g_kit.detail_idx]);
        sr_draw_text_wrap(px, W, H, dx, dy + dh + 4, desc,
                          dw, 8, 0xFFAAAAAA, shadow);
    }
}

/* ── Shop system ────────────────────────────────────────────────── */

static bool card_is_elemental(int card_type) {
    return card_type == CARD_ICE || card_type == CARD_ACID ||
           card_type == CARD_FIRE || card_type == CARD_LIGHTNING;
}

/* Consumable item IDs stored in shop cards[] with flag to distinguish from card types */
#define SHOP_CONSUMABLE_FLAG 0x1000
#define SHOP_IS_CONSUMABLE(c) ((c) & SHOP_CONSUMABLE_FLAG)
#define SHOP_CONSUMABLE_TYPE(c) ((c) & ~SHOP_CONSUMABLE_FLAG)

/* Generate armory shop: non-elemental cards + grenades, costs scrap */
static void shop_generate(shop_state *shop) {
    memset(shop, 0, sizeof(*shop));

    int non_elem[] = { CARD_REPAIR, CARD_STUN, CARD_FORTIFY,
                       CARD_DOUBLE_SHOT, CARD_DASH };
    int non_elem_count = 5;

    int idx = 0;
    /* 3-5 normal cards (3% chance each slot is overcharge) */
    int card_count = 3 + dng_rng_int(3);
    for (int i = 0; i < card_count && idx < SHOP_MAX_CARDS; i++, idx++) {
        shop->cards[idx] = (dng_rng_int(100) < 3) ? CARD_OVERCHARGE : non_elem[dng_rng_int(non_elem_count)];
        shop->prices[idx] = 15 + card_energy_cost[shop->cards[idx]] * 10 + dng_rng_int(6);
        shop->is_bio[idx] = false;
    }
    /* 1-2 grenades */
    int grenade_count = 1 + dng_rng_int(2);
    for (int i = 0; i < grenade_count && idx < SHOP_MAX_CARDS; i++, idx++) {
        shop->cards[idx] = SHOP_CONSUMABLE_FLAG | CONSUMABLE_GRENADE;
        shop->prices[idx] = consumable_prices[CONSUMABLE_GRENADE];
        shop->is_bio[idx] = false;
    }

    shop->count = idx;
    shop->cursor = 0;
    shop->mode = 0;
    shop->trash_cursor = 0;
    shop->detail_idx = -1;
    shop->detail_open = false;
    shop->active = true;
}

/* Generate medbay shop: mostly elemental cards (biomass) + health consumables (scrap) */
static void medbay_shop_generate(shop_state *shop) {
    memset(shop, 0, sizeof(*shop));

    /* Shuffle elemental pool for variety */
    int elem[] = { CARD_ICE, CARD_ACID, CARD_FIRE, CARD_LIGHTNING };
    for (int i = 3; i > 0; i--) {
        int j = dng_rng_int(i + 1);
        int tmp = elem[i]; elem[i] = elem[j]; elem[j] = tmp;
    }

    int idx = 0;
    /* 4 elemental cards (one of each, shuffled order) - costs biomass */
    for (int i = 0; i < 4 && idx < SHOP_MAX_CARDS; i++, idx++) {
        shop->cards[idx] = elem[i];
        shop->prices[idx] = 135;
        shop->is_bio[idx] = true;
    }
    /* Health kits - persistent stock, restocks by 1 per jump (max 2).
       Hidden during the very first mission so the tutorial can guarantee
       full HP and focus the player on elemental cards. */
    int kit_count = g_medbay_kit_stock;
    if (g_run_stats.sectors_visited == 0) kit_count = 0;
    if (kit_count < 0) kit_count = 0;
    if (kit_count > MEDBAY_KIT_STOCK_MAX) kit_count = MEDBAY_KIT_STOCK_MAX;
    for (int i = 0; i < kit_count && idx < SHOP_MAX_CARDS; i++, idx++) {
        shop->cards[idx] = SHOP_CONSUMABLE_FLAG | CONSUMABLE_HEALTH_KIT;
        shop->prices[idx] = consumable_prices[CONSUMABLE_HEALTH_KIT];
        shop->is_bio[idx] = false;
    }

    shop->count = idx;
    shop->cursor = 0;
    shop->mode = 0;
    shop->trash_cursor = 0;
    shop->detail_idx = -1;
    shop->detail_open = false;
    shop->active = true;
}

/* Draw a consumable item in a card-sized slot */
static void draw_consumable_slot(uint32_t *px, int W, int H,
                                 int cx, int cy, int cw, int ch,
                                 int cons_type, bool selected, uint32_t shadow) {
    uint32_t bg = selected ? 0xFF222233 : 0xFF111122;
    combat_draw_rect(px, W, H, cx, cy, cw, ch, bg);
    uint32_t border = selected ? 0xFF00DDDD :
        (cons_type == CONSUMABLE_HEALTH_KIT ? 0xFF22CC44 : 0xFF4488EE);
    combat_draw_rect_outline(px, W, H, cx, cy, cw, ch, border);
    if (selected)
        combat_draw_rect_outline(px, W, H, cx+1, cy+1, cw-2, ch-2, border);
    /* Color stripe */
    combat_draw_rect(px, W, H, cx+1, cy+1, cw-2, 3, border);
    /* Name */
    const char *name = (cons_type > 0 && cons_type < CONSUMABLE_TYPE_COUNT)
        ? consumable_names[cons_type] : "???";
    sr_draw_text_wrap(px, W, H, cx+3, cy+6, name, cw-6, 8, 0xFFFFFFFF, shadow);
    /* Effect text */
    const char *effect = "";
    if (cons_type == CONSUMABLE_HEALTH_KIT) effect = "+10 HP\nUSE ANY\nTIME";
    else if (cons_type == CONSUMABLE_GRENADE) effect = "4 DMG\nALL\nENEMIES";
    sr_draw_text_wrap(px, W, H, cx+3, cy+24, effect, cw-6, 8, 0xFF888888, shadow);
    /* "ITEM" label */
    sr_draw_text_shadow(px, W, H, cx+3, cy+ch-10, "ITEM", 0xFF555555, shadow);
}

/* Get pointer to whichever shop is currently active */
static shop_state *active_shop(void) {
    return (active_shop_type == 1) ? &g_medbay_shop : &g_shop;
}

static void draw_shop(uint32_t *px, int W, int H) {
    shop_state *shop = active_shop();
    if (!shop->active) return;
    uint32_t shadow = 0xFF000000;

    /* Background */
    for (int i = 0; i < W * H; i++) px[i] = 0xFF0D0D15;

    const char *title = (active_shop_type == 1) ? "MEDBAY" : "ARMORY";
    uint32_t title_col = (active_shop_type == 1) ? 0xFF44CC88 : 0xFFCC8822;
    sr_draw_text_shadow(px, W, H, W/2 - 30, 10, title, title_col, shadow);

    char scrap_buf[32];
    snprintf(scrap_buf, sizeof(scrap_buf), "SCRAP:%d", player_scrap);
    sr_draw_text_shadow(px, W, H, W - 150, 10, scrap_buf, 0xFFEECC44, shadow);
    char bio_buf[32];
    snprintf(bio_buf, sizeof(bio_buf), "BIO:%d", player_biomass);
    sr_draw_text_shadow(px, W, H, W - 60, 10, bio_buf, 0xFF44CC88, shadow);

    /* Mode tab buttons */
    if (ui_button(px, W, H, 30, 24, 90, 14, "BUY CARDS",
                  shop->mode == 0 ? 0xFF1A1A33 : 0xFF111122,
                  0xFF222255, 0xFF333366))
        shop->mode = 0;
    if (ui_button(px, W, H, 130, 24, 100, 14, "TRASH CARDS",
                  shop->mode == 1 ? 0xFF1A1A33 : 0xFF111122,
                  0xFF222255, 0xFF333366))
        shop->mode = 1;

    /* Treat Wounds button - medbay only, once per mission */
    if (active_shop_type == 1) {
        int tw_x = W - 170, tw_y = 24;
        if (medbay_used) {
            sr_draw_text_shadow(px, W, H, tw_x, tw_y + 2, "ALREADY TREATED", 0xFF555555, shadow);
        } else if (g_player.hp >= g_player.hp_max) {
            sr_draw_text_shadow(px, W, H, tw_x, tw_y + 2, "FULL HP", 0xFF555555, shadow);
        } else {
            char tw_buf[48];
            int heal_amt = (g_player.hp_max * 3) / 10;
            snprintf(tw_buf, sizeof(tw_buf), "TREAT WOUNDS +%d HP", heal_amt);
            if (ui_button(px, W, H, tw_x, tw_y, 160, 14, tw_buf,
                          0xFF113322, 0xFF225533, 0xFF44CC88)) {
                if (heal_amt < 1) heal_amt = 1;
                g_player.hp += heal_amt;
                if (g_player.hp > g_player.hp_max) g_player.hp = g_player.hp_max;
                medbay_used = true;
            }
        }
    }

    if (shop->mode == 0) {
        /* Buy mode - visual card grid */
        int shop_cols = 3;
        int cw = 60, ch = 80;
        int padX = 10, padY = 14;
        int gridW = shop_cols * (cw + padX) - padX;
        int startX = (W - gridW) / 2;
        int startY = 42;

        if (!shop->detail_open) {
            for (int i = 0; i < shop->count; i++) {
                int col = i % shop_cols;
                int row = i / shop_cols;
                int cx = startX + col * (cw + padX);
                int cy = startY + row * (ch + padY);

                int card_type = shop->cards[i];
                bool sel = (shop->cursor == i);

                /* Render card to temp buffer */
                memset(card_fan_buf, 0, sizeof(card_fan_buf));
                if (SHOP_IS_CONSUMABLE(card_type)) {
                    draw_consumable_slot(card_fan_buf, cw, ch,
                                        0, 0, cw, ch,
                                        SHOP_CONSUMABLE_TYPE(card_type), sel, shadow);
                } else {
                    combat_draw_card_content(card_fan_buf, cw, ch,
                                             0, 0, cw, ch,
                                             card_type, sel, shadow, -1);
                }
                /* Blit to screen (no rotation) */
                for (int ry = 0; ry < ch; ry++)
                    for (int rx = 0; rx < cw; rx++) {
                        int dx = cx + rx, dy = cy + ry;
                        if (dx >= 0 && dx < W && dy >= 0 && dy < H) {
                            uint32_t c = card_fan_buf[ry * cw + rx];
                            if (c & 0xFF000000) px[dy * W + dx] = c;
                        }
                    }

                /* Price below card */
                char price_buf[16];
                bool bio = shop->is_bio[i];
                int wallet = bio ? player_biomass : player_scrap;
                snprintf(price_buf, sizeof(price_buf), "%d%s", shop->prices[i], bio ? "B" : "S");
                uint32_t pcol = wallet >= shop->prices[i]
                    ? (bio ? 0xFF44CC88 : 0xFF22CC22) : 0xFF882222;
                sr_draw_text_shadow(px, W, H, cx + cw/2 - 8, cy + ch + 2, price_buf, pcol, shadow);

                /* Click detection - open detail popup */
                if (ui_mouse_clicked &&
                    ui_click_x >= cx && ui_click_x < cx + cw &&
                    ui_click_y >= cy && ui_click_y < cy + ch) {
                    shop->detail_idx = i;
                    shop->detail_open = true;
                    shop->cursor = i;
                }
            }
        }

        /* Detail popup */
        if (shop->detail_open && shop->detail_idx >= 0 && shop->detail_idx < shop->count) {
            int idx = shop->detail_idx;
            int card_type = shop->cards[idx];
            bool is_cons_detail = SHOP_IS_CONSUMABLE(card_type);
            int cons_type_detail = SHOP_CONSUMABLE_TYPE(card_type);
            uint32_t ccol = is_cons_detail
                ? (cons_type_detail == CONSUMABLE_HEALTH_KIT ? 0xFF22CC44 : 0xFF4488EE)
                : card_colors[card_type];

            int pw = 250, ph = 150;
            int px2 = (W - pw) / 2;
            int py = (H - ph) / 2;

            /* Background */
            combat_draw_rect(px, W, H, px2, py, pw, ph, 0xFF0A0A18);
            combat_draw_rect_outline(px, W, H, px2, py, pw, ph, ccol);
            combat_draw_rect(px, W, H, px2 + 1, py + 1, pw - 2, 3, ccol);

            if (is_cons_detail) {
                /* Consumable detail */
                const char *cname = consumable_names[cons_type_detail];
                sr_draw_text_shadow(px, W, H, px2 + 8, py + 6, cname, 0xFFFFFFFF, shadow);
                sr_draw_text_shadow(px, W, H, px2 + 8, py + 18, "CONSUMABLE ITEM", 0xFF888888, shadow);
                const char *cdesc = "";
                if (cons_type_detail == CONSUMABLE_HEALTH_KIT)
                    cdesc = "RESTORES 10 HP.\nCAN BE USED ANY TIME\nDURING COMBAT.\nDOES NOT COST ENERGY.";
                else if (cons_type_detail == CONSUMABLE_GRENADE)
                    cdesc = "DEALS 4 DAMAGE TO\nALL ENEMIES.\nCAN BE USED ANY TIME\nDURING COMBAT.";
                sr_draw_text_wrap(px, W, H, px2 + 8, py + 30, cdesc,
                                  pw - 16, 8, ccol, shadow);
                /* Show current inventory count */
                int owned = 0;
                for (int s = 0; s < CONSUMABLE_SLOTS; s++)
                    if (player_consumables[s] == cons_type_detail) owned++;
                char inv_buf[32];
                snprintf(inv_buf, sizeof(inv_buf), "OWNED: %d/%d", owned, CONSUMABLE_SLOTS);
                sr_draw_text_shadow(px, W, H, px2 + 8, py + 75, inv_buf,
                                    owned >= CONSUMABLE_SLOTS ? 0xFF882222 : 0xFF888888, shadow);
            } else {
                /* Card detail */
                if (card_type < (int)(sizeof(spr_card_table)/sizeof(spr_card_table[0])))
                    spr_draw(px, W, H, spr_card_table[card_type], px2 + pw - 60, py + 20, 3);
                sr_draw_text_shadow(px, W, H, px2 + 8, py + 6, card_names[card_type],
                                    0xFFFFFFFF, shadow);
                char ebuf[8];
                snprintf(ebuf, sizeof(ebuf), "%dE", card_energy_cost[card_type]);
                sr_draw_text_shadow(px, W, H, px2 + pw - 76, py + 6, ebuf, 0xFF22CCEE, shadow);
                const char *tgt = "";
                int tt = card_targets[card_type];
                if (tt == TARGET_SELF) tgt = "TARGET: SELF";
                else if (tt == TARGET_ENEMY) tgt = "TARGET: 1 ENEMY";
                else tgt = "TARGET: ALL";
                sr_draw_text_shadow(px, W, H, px2 + 8, py + 18, tgt, 0xFF888888, shadow);
                const char *effect = card_effect_text(card_type);
                int ey = sr_draw_text_wrap(px, W, H, px2 + 8, py + 30, effect,
                                           pw - 80, 8, ccol, shadow);
                const char *desc = card_description_text(card_type);
                sr_draw_text_wrap(px, W, H, px2 + 8, ey + 4, desc,
                                  pw - 80, 8, 0xFF666666, shadow);
            }

            /* Price */
            char price_buf[32];
            bool bio = shop->is_bio[idx];
            bool is_cons = SHOP_IS_CONSUMABLE(shop->cards[idx]);
            int wallet = bio ? player_biomass : player_scrap;
            snprintf(price_buf, sizeof(price_buf), "PRICE: %d %s", shop->prices[idx],
                     bio ? "BIOMASS" : "SCRAP");
            bool has_space;
            if (is_cons) {
                has_space = false;
                for (int s = 0; s < CONSUMABLE_SLOTS; s++)
                    if (player_consumables[s] == CONSUMABLE_NONE) { has_space = true; break; }
            } else {
                has_space = g_player.persistent_deck_count < COMBAT_DECK_MAX;
            }
            bool can_buy = wallet >= shop->prices[idx] && has_space;
            uint32_t price_col = can_buy ? (bio ? 0xFF44CC88 : 0xFF22CC22) : 0xFF882222;
            sr_draw_text_shadow(px, W, H, px2 + 8, py + ph - 30,
                                price_buf, price_col, shadow);

            /* BUY button or reason why not */
            if (!can_buy && !has_space) {
                sr_draw_text_shadow(px, W, H, px2 + 8, py + ph - 18,
                                    "INVENTORY FULL", 0xFF882222, shadow);
            } else if (can_buy) {
                if (ui_button(px, W, H, px2 + 8, py + ph - 18, 60, 14, "BUY",
                              0xFF1A3311, 0xFF224422, 0xFF44CC44)) {
                    if (bio) player_biomass -= shop->prices[idx];
                    else     player_scrap -= shop->prices[idx];
                    if (is_cons) {
                        int ctype = SHOP_CONSUMABLE_TYPE(shop->cards[idx]);
                        for (int s = 0; s < CONSUMABLE_SLOTS; s++) {
                            if (player_consumables[s] == CONSUMABLE_NONE) {
                                player_consumables[s] = ctype;
                                break;
                            }
                        }
                        /* Buying a health kit from the medbay decrements
                           the persistent kit stock (restocked on jump). */
                        if (active_shop_type == 1 && ctype == CONSUMABLE_HEALTH_KIT &&
                            g_medbay_kit_stock > 0)
                            g_medbay_kit_stock--;
                    } else {
                        g_player.persistent_deck[g_player.persistent_deck_count++] = shop->cards[idx];
                        /* First medbay card purchase ticks off the
                           onboarding objective. */
                        if (active_shop_type == 1)
                            mission_medbay_card_bought = true;
                    }
                    for (int j = idx; j < shop->count - 1; j++) {
                        shop->cards[j] = shop->cards[j + 1];
                        shop->prices[j] = shop->prices[j + 1];
                        shop->is_bio[j] = shop->is_bio[j + 1];
                    }
                    shop->count--;
                    shop->detail_open = false;
                    shop->detail_idx = -1;
                    if (shop->cursor >= shop->count && shop->count > 0)
                        shop->cursor = shop->count - 1;
                    ui_mouse_clicked = false; /* consume click to prevent double-buy */
                }
            }

            /* CLOSE button */
            if (ui_button(px, W, H, px2 + pw - 70, py + ph - 18, 60, 14, "CLOSE",
                          0xFF111122, 0xFF222244, 0xFF333366)) {
                shop->detail_open = false;
                shop->detail_idx = -1;
            }
        }
    } else {
        /* Trash mode - show player's deck with card details */
        int trash_cost = 30 + shop->trash_count * 5;
        char trash_hdr[48];
        snprintf(trash_hdr, sizeof(trash_hdr), "YOUR DECK:  TRASH COST: %d SCRAP", trash_cost);
        sr_draw_text_shadow(px, W, H, 60, 44, trash_hdr, 0xFFD0CDC7, shadow); /* pal #8 c7dcd0 */

        /* Card list (left side) */
        int list_x = 30, list_y = 58;
        int list_w = 100;
        for (int i = 0; i < g_player.persistent_deck_count; i++) {
            int cy = list_y + i * 12;
            if (cy > H - 40) break;

            bool hovered = false;
            ui_row_hover(list_x, cy, list_w, 11, &hovered);
            if (hovered) shop->trash_cursor = i;
            bool sel = (shop->trash_cursor == i);

            int card_type = g_player.persistent_deck[i];
            uint32_t ccol = sel ? 0xFFD0CDC7 : card_colors[card_type]; /* pal #8 when selected */
            if (sel) {
                /* Highlight bar */
                for (int ry = cy; ry < cy + 11 && ry < H; ry++)
                    for (int rx = list_x; rx < list_x + list_w && rx < W; rx++)
                        if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF46353E; /* pal #1 */
            }
            sr_draw_text_shadow(px, W, H, list_x + 2, cy + 1, card_names[card_type], ccol, shadow);

            /* Energy cost pip */
            char ebuf[4];
            snprintf(ebuf, sizeof(ebuf), "%d", card_energy_cost[card_type]);
            sr_draw_text_shadow(px, W, H, list_x + list_w - 12, cy + 1, ebuf,
                                0xFF9BAF0E, shadow); /* pal #41 teal */
        }

        /* Card detail panel (right side) */
        if (shop->trash_cursor >= 0 && shop->trash_cursor < g_player.persistent_deck_count) {
            int idx = shop->trash_cursor;
            int card_type = g_player.persistent_deck[idx];
            int px2 = 150, py = 56, pw = W - 180, ph = H - 90;
            uint32_t ccol = card_colors[card_type];

            /* Panel background */
            for (int ry = py; ry < py + ph && ry < H; ry++)
                for (int rx = px2; rx < px2 + pw && rx < W; rx++)
                    if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF2F222E; /* pal #0 */

            /* Border in card color */
            combat_draw_rect_outline(px, W, H, px2, py, pw, ph, ccol);

            /* Card art */
            if (card_type < (int)(sizeof(spr_card_table)/sizeof(spr_card_table[0])) &&
                spr_card_table[card_type])
                spr_draw(px, W, H, spr_card_table[card_type], px2 + pw - 42, py + 4, 2);

            /* Card name */
            sr_draw_text_shadow(px, W, H, px2 + 6, py + 6, card_names[card_type], ccol, shadow);

            /* Energy cost */
            char ebuf2[16];
            snprintf(ebuf2, sizeof(ebuf2), "COST: %d", card_energy_cost[card_type]);
            sr_draw_text_shadow(px, W, H, px2 + 6, py + 16, ebuf2, 0xFF9BAF0E, shadow); /* pal #41 */

            /* Effect text */
            const char *effect = card_effect_text(card_type);
            int ey = sr_draw_text_wrap(px, W, H, px2 + 6, py + 28, effect,
                                       pw - 50, 8, ccol, shadow);

            /* Description */
            const char *desc = card_description_text(card_type);
            sr_draw_text_wrap(px, W, H, px2 + 6, ey + 4, desc,
                              pw - 12, 8, 0xFF8A707F, shadow); /* pal #7 steel */

            /* Trash button */
            bool can_trash = g_player.persistent_deck_count > 5 && player_scrap >= trash_cost;
            char tbuf[24];
            snprintf(tbuf, sizeof(tbuf), "TRASH (%d)", trash_cost);
            if (ui_button(px, W, H, px2 + 6, py + ph - 20, 80, 14, tbuf,
                          can_trash ? 0xFF27276E : 0xFF46353E, /* pal #10 dk red / pal #1 */
                          can_trash ? 0xFF3E459E : 0xFF46353E, /* pal #20 / pal #1 */
                          can_trash ? 0xFF3B3BE8 : 0xFF46353E)) { /* pal #15 bright red */
                if (can_trash) {
                    player_scrap -= trash_cost;
                    shop->trash_count++;
                    for (int j = idx; j < g_player.persistent_deck_count - 1; j++)
                        g_player.persistent_deck[j] = g_player.persistent_deck[j + 1];
                    g_player.persistent_deck_count--;
                    if (shop->trash_cursor >= g_player.persistent_deck_count)
                        shop->trash_cursor = g_player.persistent_deck_count - 1;
                }
            }
            if (!can_trash && g_player.persistent_deck_count <= 5)
                sr_draw_text_shadow(px, W, H, px2 + 6, py + ph - 34, "MIN 5 CARDS",
                                    0xFF655562, shadow);
        }
    }

    /* Leave button */
    if (ui_button(px, W, H, W/2 - 40, H - 18, 80, 16, "LEAVE",
                  0xFF111122, 0xFF222244, 0xFF333366)) {
        shop->active = false;
        app_state = STATE_SHIP_HUB;
    }
}

static void shop_handle_key(int key_code) {
    shop_state *shop = active_shop();
    if (!shop->active) return;

    /* Tab between buy/trash with 1/2 keys */
    if (key_code == SAPP_KEYCODE_1) { shop->mode = 0; return; }
    if (key_code == SAPP_KEYCODE_2) { shop->mode = 1; return; }

    if (shop->mode == 0) {
        /* Buy mode */
        if (key_code == SAPP_KEYCODE_UP || key_code == SAPP_KEYCODE_W) {
            shop->cursor--;
            if (shop->cursor < 0) shop->cursor = shop->count - 1;
        }
        if (key_code == SAPP_KEYCODE_DOWN || key_code == SAPP_KEYCODE_S) {
            shop->cursor++;
            if (shop->cursor >= shop->count) shop->cursor = 0;
        }
        if (key_code == SAPP_KEYCODE_ENTER || key_code == SAPP_KEYCODE_SPACE) {
            int idx = shop->cursor;
            if (idx < 0 || idx >= shop->count) return;
            bool bio = shop->is_bio[idx];
            bool is_cons = SHOP_IS_CONSUMABLE(shop->cards[idx]);
            int wallet = bio ? player_biomass : player_scrap;
            bool has_space;
            if (is_cons) {
                has_space = false;
                for (int s = 0; s < CONSUMABLE_SLOTS; s++)
                    if (player_consumables[s] == CONSUMABLE_NONE) { has_space = true; break; }
            } else {
                has_space = g_player.persistent_deck_count < COMBAT_DECK_MAX;
            }
            if (wallet >= shop->prices[idx] && has_space) {
                if (bio) player_biomass -= shop->prices[idx];
                else     player_scrap -= shop->prices[idx];
                if (is_cons) {
                    int ctype = SHOP_CONSUMABLE_TYPE(shop->cards[idx]);
                    for (int s = 0; s < CONSUMABLE_SLOTS; s++) {
                        if (player_consumables[s] == CONSUMABLE_NONE) {
                            player_consumables[s] = ctype;
                            break;
                        }
                    }
                } else {
                    g_player.persistent_deck[g_player.persistent_deck_count++] = shop->cards[idx];
                }
                /* Remove from shop */
                for (int i = idx; i < shop->count - 1; i++) {
                    shop->cards[i] = shop->cards[i + 1];
                    shop->prices[i] = shop->prices[i + 1];
                    shop->is_bio[i] = shop->is_bio[i + 1];
                }
                shop->count--;
                if (shop->cursor >= shop->count && shop->count > 0)
                    shop->cursor = shop->count - 1;
            }
        }
    } else {
        /* Trash mode */
        if (key_code == SAPP_KEYCODE_LEFT || key_code == SAPP_KEYCODE_A) {
            shop->trash_cursor--;
            if (shop->trash_cursor < 0)
                shop->trash_cursor = g_player.persistent_deck_count - 1;
        }
        if (key_code == SAPP_KEYCODE_RIGHT || key_code == SAPP_KEYCODE_D) {
            shop->trash_cursor++;
            if (shop->trash_cursor >= g_player.persistent_deck_count)
                shop->trash_cursor = 0;
        }
        if (key_code == SAPP_KEYCODE_UP || key_code == SAPP_KEYCODE_W) {
            shop->trash_cursor -= 4;
            if (shop->trash_cursor < 0) shop->trash_cursor = 0;
        }
        if (key_code == SAPP_KEYCODE_DOWN || key_code == SAPP_KEYCODE_S) {
            shop->trash_cursor += 4;
            if (shop->trash_cursor >= g_player.persistent_deck_count)
                shop->trash_cursor = g_player.persistent_deck_count - 1;
        }
        if (key_code == SAPP_KEYCODE_ENTER || key_code == SAPP_KEYCODE_SPACE) {
            int trash_cost = 30 + shop->trash_count * 5;
            if (g_player.persistent_deck_count > 5 && player_scrap >= trash_cost) {
                player_scrap -= trash_cost;
                shop->trash_count++;
                int idx = shop->trash_cursor;
                for (int i = idx; i < g_player.persistent_deck_count - 1; i++)
                    g_player.persistent_deck[i] = g_player.persistent_deck[i + 1];
                g_player.persistent_deck_count--;
                if (shop->trash_cursor >= g_player.persistent_deck_count)
                    shop->trash_cursor = g_player.persistent_deck_count - 1;
            }
        }
    }
}

/* ── Star map generation & rendering ────────────────────────────── */

static const char *sector_names[] = {
    "ALPHA", "BETA", "GAMMA", "DELTA", "EPSILON",
    "ZETA", "SIGMA", "OMEGA", "THETA", "KAPPA",
    "NOVA", "VEGA", "RIGEL", "ORION", "LYRA", "CYGNUS"
};
#define NUM_SECTOR_NAMES 16

static const char *boss_names[] = { "HIVE VESSEL ALPHA", "HIVE VESSEL BETA", "HIVE VESSEL GAMMA" };

static void starmap_generate(starmap_state *sm, int start_sector) {
    memset(sm, 0, sizeof(*sm));

    /* Generate branching path: 3 normal columns + 1 boss column */
    int col_count = 5; /* 1 start + 3 normal + 1 boss */
    int node_idx = 0;

    /* Starting node (current position) */
    sm->nodes[0].difficulty = start_sector;
    sm->nodes[0].scrap_reward = 0;
    sm->nodes[0].visited = true;
    sm->nodes[0].is_boss = false;
    sm->nodes[0].node_type = NODE_NORMAL;
    sm->nodes[0].event_id = -1;
    sm->nodes[0].x = 40;
    sm->nodes[0].y = FB_HEIGHT / 2;
    snprintf(sm->nodes[0].name, 24, "SECTOR %s", sector_names[start_sector % NUM_SECTOR_NAMES]);
    node_idx = 1;

    int prev_start = 0, prev_count = 1;

    for (int col = 1; col < col_count; col++) {
        bool is_boss_col = (col == col_count - 1);
        int nodes_in_col = is_boss_col ? 1 : (2 + dng_rng_int(2));
        if (nodes_in_col > 3) nodes_in_col = 3;
        int col_start = node_idx;

        for (int n = 0; n < nodes_in_col && node_idx < STARMAP_MAX_NODES; n++) {
            starmap_node *nd = &sm->nodes[node_idx];
            int diff = start_sector + col;
            nd->difficulty = diff;
            nd->visited = false;
            nd->is_boss = is_boss_col;
            nd->node_type = is_boss_col ? NODE_BOSS : NODE_NORMAL;
            nd->event_id = -1;

            if (is_boss_col) {
                nd->scrap_reward = 40 + diff * 10;
                int bi = player_starmap;
                if (bi < 0) bi = 0; if (bi > 2) bi = 2;
                snprintf(nd->name, 24, "%s", boss_names[bi]);
            } else if (col == col_count - 2 && n == nodes_in_col - 1) {
                /* Last node before boss column is always a Junkers trader */
                nd->node_type = NODE_JUNKERS;
                nd->scrap_reward = 0;
                snprintf(nd->name, 24, "JUNKERS");
            } else if ((col % 2) == 0 && n == 0 && col < col_count - 2) {
                /* Even normal columns: first node is an event (question mark).
                   Pattern: enemy col, event col, enemy col, ...
                   This ensures at most 1 non-combat node per 2 depths. */
                nd->node_type = NODE_EVENT;
                nd->event_id = dng_rng_int(EVENT_MAX_EVENTS);
                nd->scrap_reward = 0;
                static const char *event_short[] = {
                    "DISTRESS SIGNAL", "ANOMALY SCAN", "RUINS DETECTED",
                    "CARGO DRIFT", "TRANSMISSION", "ARTIFACT",
                    "DERELICT MEDBAY", "VOID STORM"
                };
                snprintf(nd->name, 24, "%s",
                         event_short[nd->event_id % 8]);
            } else if ((col % 2) != 0 && n == nodes_in_col - 1 && nodes_in_col >= 3) {
                /* Odd columns with 3 nodes: make the last one a miniboss */
                nd->node_type = NODE_MINIBOSS;
                nd->scrap_reward = 30 + diff * 8;
                int name_idx = (start_sector + col * 3 + n) % NUM_SECTOR_NAMES;
                snprintf(nd->name, 24, "%s DEN", sector_names[name_idx]);
            } else {
                nd->scrap_reward = 20 + diff * 5 + dng_rng_int(10);
                int name_idx = (start_sector + col * 3 + n) % NUM_SECTOR_NAMES;
                snprintf(nd->name, 24, "SECTOR %s-%d", sector_names[name_idx], diff);
            }

            nd->x = 40 + col * (FB_WIDTH - 80) / (col_count - 1);
            int map_top = FB_HEIGHT / 4;  /* top of node spread */
            int map_bot = FB_HEIGHT * 3 / 4; /* bottom of node spread */
            int map_h = map_bot - map_top;
            nd->y = is_boss_col ? FB_HEIGHT / 2
                : map_top + n * map_h / (nodes_in_col > 1 ? nodes_in_col - 1 : 1);

            nd->next_count = 0;
            for (int i = 0; i < STARMAP_MAX_CHOICES; i++) nd->next[i] = -1;

            node_idx++;
        }

        /* Connect previous column to this column */
        for (int p = prev_start; p < prev_start + prev_count; p++) {
            if (is_boss_col) {
                /* All prev nodes connect to the single boss node */
                if (sm->nodes[p].next_count < STARMAP_MAX_CHOICES)
                    sm->nodes[p].next[sm->nodes[p].next_count++] = col_start;
            } else {
                int conns = 1 + dng_rng_int(2);
                if (conns > nodes_in_col) conns = nodes_in_col;
                for (int c = 0; c < conns; c++) {
                    int target = col_start + dng_rng_int(nodes_in_col);
                    bool dup = false;
                    for (int e = 0; e < sm->nodes[p].next_count; e++)
                        if (sm->nodes[p].next[e] == target) { dup = true; break; }
                    if (!dup && sm->nodes[p].next_count < STARMAP_MAX_CHOICES)
                        sm->nodes[p].next[sm->nodes[p].next_count++] = target;
                }
            }
        }

        /* Ensure every node in this column is reachable */
        for (int n = 0; n < nodes_in_col; n++) {
            int target = col_start + n;
            bool reachable = false;
            for (int p = prev_start; p < prev_start + prev_count; p++) {
                for (int e = 0; e < sm->nodes[p].next_count; e++)
                    if (sm->nodes[p].next[e] == target) { reachable = true; break; }
                if (reachable) break;
            }
            if (!reachable) {
                int p = prev_start + dng_rng_int(prev_count);
                if (sm->nodes[p].next_count < STARMAP_MAX_CHOICES)
                    sm->nodes[p].next[sm->nodes[p].next_count++] = target;
            }
        }

        prev_start = col_start;
        prev_count = nodes_in_col;
    }

    sm->node_count = node_idx;
    sm->current_node = 0;
    sm->cursor = 0;
    sm->active = true;
}

/* ── Starmap JSON save/load ─────────────────────────────────────── */

/* Save starmap STRUCTURE only (no progress data - that goes in the save file) */
static void starmap_save_json(const starmap_state *sm, const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) { printf("[starmap] Failed to save %s\n", path); return; }
    fprintf(f, "{\n");
    fprintf(f, "  \"nodes\": [\n");
    for (int i = 0; i < sm->node_count; i++) {
        const starmap_node *nd = &sm->nodes[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", nd->name);
        if (nd->level_file[0])
            fprintf(f, "      \"levelFile\": \"%s\",\n", nd->level_file);
        fprintf(f, "      \"difficulty\": %d,\n", nd->difficulty);
        fprintf(f, "      \"isBoss\": %s,\n", nd->is_boss ? "true" : "false");
        {
            const char *type_str = "normal";
            if (nd->node_type == NODE_MINIBOSS) type_str = "miniboss";
            else if (nd->node_type == NODE_EVENT) type_str = "event";
            else if (nd->node_type == NODE_BOSS) type_str = "boss";
            else if (nd->node_type == NODE_JUNKERS) type_str = "junkers";
            fprintf(f, "      \"nodeType\": \"%s\",\n", type_str);
        }
        if (nd->boss_room >= 0)
            fprintf(f, "      \"bossRoom\": %d,\n", nd->boss_room);
        if (nd->event_id >= 0)
            fprintf(f, "      \"eventId\": %d,\n", nd->event_id);
        fprintf(f, "      \"x\": %d,\n", nd->x);
        fprintf(f, "      \"y\": %d,\n", nd->y);
        fprintf(f, "      \"next\": [");
        for (int j = 0; j < nd->next_count; j++)
            fprintf(f, "%d%s", nd->next[j], j < nd->next_count - 1 ? ", " : "");
        fprintf(f, "]\n");
        fprintf(f, "    }%s\n", i < sm->node_count - 1 ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    printf("[starmap] Saved %d nodes to %s (current=%d, visited=%d)\n",
           sm->node_count, path, sm->current_node, sm->derelicts_visited);
}

static bool starmap_load_json(starmap_state *sm, const char *path) {
    if (!lvl_file_exists(path)) return false;
    char *data = lvl_read_file(path);
    if (!data) return false;
    sr_json json;
    if (!sr_json_parse(&json, data)) { free(data); return false; }

    int root = 0;
    int nodes_arr = sr_json_find(&json, root, "nodes");
    if (nodes_arr < 0) { printf("[starmap] No 'nodes' in %s\n", path); return false; }

    int count = sr_json_array_len(&json, nodes_arr);
    if (count <= 0 || count > STARMAP_MAX_NODES) {
        printf("[starmap] Invalid node count %d in %s\n", count, path);
        return false;
    }

    memset(sm, 0, sizeof(*sm));
    sm->node_count = count;
    sm->current_node = 0;        /* progress restored from save file, not JSON */
    sm->derelicts_visited = 0;
    sm->cursor = 0;
    sm->active = true;

    for (int i = 0; i < count; i++) {
        int nt = sr_json_array_get(&json, nodes_arr, i);
        if (nt < 0) continue;
        starmap_node *nd = &sm->nodes[i];
        sr_json_str(&json, sr_json_find(&json, nt, "name"), nd->name, sizeof(nd->name));
        sr_json_str(&json, sr_json_find(&json, nt, "levelFile"), nd->level_file, sizeof(nd->level_file));
        nd->difficulty = sr_json_int(&json, sr_json_find(&json, nt, "difficulty"), 0);
        nd->is_boss = sr_json_bool(&json, sr_json_find(&json, nt, "isBoss"), false);
        nd->boss_room = sr_json_int(&json, sr_json_find(&json, nt, "bossRoom"), -1);
        nd->event_id = sr_json_int(&json, sr_json_find(&json, nt, "eventId"), -1);
        nd->visited = (i == 0); /* only start node visited by default */
        nd->x = sr_json_int(&json, sr_json_find(&json, nt, "x"), 0);
        nd->y = sr_json_int(&json, sr_json_find(&json, nt, "y"), 0);

        /* Parse nodeType string → enum */
        {
            char type_str[16] = {0};
            sr_json_str(&json, sr_json_find(&json, nt, "nodeType"), type_str, sizeof(type_str));
            if (strcmp(type_str, "miniboss") == 0)      nd->node_type = NODE_MINIBOSS;
            else if (strcmp(type_str, "event") == 0)     nd->node_type = NODE_EVENT;
            else if (strcmp(type_str, "boss") == 0)      nd->node_type = NODE_BOSS;
            else if (strcmp(type_str, "junkers") == 0)   nd->node_type = NODE_JUNKERS;
            else                                         nd->node_type = NODE_NORMAL;
        }
        /* Sync is_boss from node_type */
        if (nd->node_type == NODE_BOSS) nd->is_boss = true;

        int next_arr = sr_json_find(&json, nt, "next");
        if (next_arr >= 0) {
            nd->next_count = sr_json_array_len(&json, next_arr);
            if (nd->next_count > STARMAP_MAX_CHOICES) nd->next_count = STARMAP_MAX_CHOICES;
            for (int j = 0; j < nd->next_count; j++)
                nd->next[j] = sr_json_int(&json, sr_json_array_get(&json, next_arr, j), -1);
        }
        for (int j = nd->next_count; j < STARMAP_MAX_CHOICES; j++)
            nd->next[j] = -1;
    }

    printf("[starmap] Loaded %d nodes from %s\n", count, path);
    return true;
}

/* Try JSON first, fall back to procedural generation */
static void starmap_generate_or_load(starmap_state *sm, int start_sector) {
    if (starmap_load_json(sm, "levels/starmap.json")) return;
    starmap_generate(sm, start_sector);
    starmap_save_json(sm, "levels/starmap.json");
}

static void draw_starmap(uint32_t *px, int W, int H) {
    if (!g_starmap.active) return;
    uint32_t shadow = 0xFF000000;

    /* Background */
    for (int i = 0; i < W * H; i++) {
        /* Starfield background */
        uint32_t bg = 0xFF080810;
        /* Sparse stars */
        int hash = i * 2654435761u;
        if ((hash & 0x3FF) == 0) bg = 0xFF333344;
        px[i] = bg;
    }

    sr_draw_text_shadow(px, W, H, W/2 - 25, 4, "STAR MAP", 0xFFCCCCFF, shadow);

    /* Samples counter on star map */
    {
        char samp_buf[32];
        snprintf(samp_buf, sizeof(samp_buf), "SAMPLES %d/%d", player_samples, SAMPLES_REQUIRED);
        uint32_t samp_col = (player_samples >= SAMPLES_REQUIRED) ? 0xFF44CC44 : 0xFFCC8822;
        sr_draw_text_shadow(px, W, H, W - 84, 4, samp_buf, samp_col, shadow);
    }

    /* Draw connections first */
    for (int i = 0; i < g_starmap.node_count; i++) {
        starmap_node *nd = &g_starmap.nodes[i];
        for (int c = 0; c < nd->next_count; c++) {
            int target = nd->next[c];
            if (target < 0 || target >= g_starmap.node_count) continue;
            starmap_node *tgt = &g_starmap.nodes[target];
            uint32_t line_col = 0xFF333355;
            if (i == g_starmap.current_node) line_col = 0xFF5555AA;
            minimap_line(px, nd->x, nd->y, tgt->x, tgt->y, line_col);
        }
    }

    /* Draw visited path (bright line showing where player has been) */
    {
        int prev = 0; /* start node */
        for (int i = 0; i < g_starmap.visited_path_count; i++) {
            int cur_p = g_starmap.visited_path[i];
            if (cur_p >= 0 && cur_p < g_starmap.node_count && prev >= 0) {
                minimap_line(px, g_starmap.nodes[prev].x, g_starmap.nodes[prev].y,
                             g_starmap.nodes[cur_p].x, g_starmap.nodes[cur_p].y, 0xFF44AA44);
            }
            prev = cur_p;
        }
    }

    /* Build reachable list: forward connections + back-links from visited nodes */
    starmap_node *cur = &g_starmap.nodes[g_starmap.current_node];
    int reachable[STARMAP_MAX_NODES];
    int reachable_count = 0;
    /* Forward: cur->next[] (unvisited targets for new exploration) */
    for (int c = 0; c < cur->next_count; c++) {
        int t = cur->next[c];
        if (t >= 0 && t < g_starmap.node_count && t != g_starmap.current_node)
            reachable[reachable_count++] = t;
    }
    /* Backward: any visited node that has current_node in its next[] */
    for (int i = 0; i < g_starmap.node_count; i++) {
        if (i == g_starmap.current_node) continue;
        if (!g_starmap.nodes[i].visited) continue;
        /* Check if i connects to current_node (we can go back) */
        for (int c = 0; c < g_starmap.nodes[i].next_count; c++) {
            if (g_starmap.nodes[i].next[c] == g_starmap.current_node) {
                /* Check not already in reachable */
                bool dup = false;
                for (int r = 0; r < reachable_count; r++)
                    if (reachable[r] == i) { dup = true; break; }
                if (!dup) reachable[reachable_count++] = i;
                break;
            }
        }
    }
    if (g_starmap.cursor >= reachable_count) g_starmap.cursor = 0;

    /* Hover detection on reachable nodes */
    int hovered_node = -1;
    if (!g_starmap.confirm_active) {
        for (int c = 0; c < reachable_count; c++) {
            int target = reachable[c];
            starmap_node *nd = &g_starmap.nodes[target];
            float ddx = ui_mouse_x - nd->x, ddy = ui_mouse_y - nd->y;
            if (ddx * ddx + ddy * ddy <= 14 * 14) {
                g_starmap.cursor = c;
                hovered_node = target;
                break;
            }
        }
    }

    /* Draw nodes */
    for (int i = 0; i < g_starmap.node_count; i++) {
        starmap_node *nd = &g_starmap.nodes[i];

        /* Color based on node type */
        uint32_t col;
        if (i == g_starmap.current_node) col = 0xFF22CC22;
        else if (nd->visited) col = 0xFF555555;
        else {
            switch (nd->node_type) {
                case NODE_BOSS:     col = 0xFF4444FF; break;
                case NODE_MINIBOSS: col = 0xFF44CCFF; break; /* yellow-orange */
                case NODE_EVENT:    col = 0xFFDDDD00; break; /* cyan */
                case NODE_JUNKERS:  col = 0xFF32A0DC; break; /* amber/orange */
                default:            col = 0xFFCCCCFF; break; /* white-blue */
            }
        }

        bool selectable = false;
        for (int c = 0; c < reachable_count; c++) {
            if (reachable[c] == i) {
                selectable = true;
                if (g_starmap.cursor == c) col = 0xFF00DDDD;
                break;
            }
        }

        bool is_hovered = (i == hovered_node);

        /* Node icon - draw 8x8 sprite if available, else fallback to circle */
        int icon_stex = -1;
        switch (nd->node_type) {
            case NODE_BOSS:     icon_stex = STEX_MAP_BOSS; break;
            case NODE_MINIBOSS: icon_stex = STEX_MAP_MINIBOSS; break;
            case NODE_EVENT:    icon_stex = STEX_MAP_EVENT; break;
            case NODE_JUNKERS:  icon_stex = STEX_MAP_JUNKERS; break;
            default:            icon_stex = STEX_MAP_NORMAL; break;
        }

        /* Hover glow ring */
        if (is_hovered) {
            uint32_t glow = 0xFF005555;
            for (int dy = -6; dy <= 6; dy++)
                for (int dx = -6; dx <= 6; dx++) {
                    int d2 = dx*dx + dy*dy;
                    if (d2 > 36 || d2 <= 20) continue;
                    int rx = nd->x + dx, ry = nd->y + dy;
                    if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                        px[ry * W + rx] = glow;
                }
        }

        if (icon_stex >= 0 && stextures[icon_stex].pixels && !nd->visited) {
            /* Draw icon centered on node position */
            spr_draw_tex(px, W, H, &stextures[icon_stex], nd->x - 4, nd->y - 4, 1);
        } else {
            /* Fallback: filled circle */
            int node_r = (nd->node_type == NODE_BOSS) ? 4 :
                         (nd->node_type == NODE_MINIBOSS) ? 3 : 2;
            int node_r2 = node_r * node_r + node_r;
            for (int dy = -node_r; dy <= node_r; dy++)
                for (int dx = -node_r; dx <= node_r; dx++) {
                    if (dx*dx + dy*dy > node_r2) continue;
                    int rx = nd->x + dx, ry = nd->y + dy;
                    if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                        px[ry * W + rx] = is_hovered ? 0xFF00FFFF : col;
                }
        }

        /* Label */
        if (i == g_starmap.current_node || selectable) {
            int label_y = (nd->node_type == NODE_BOSS) ? nd->y + 8 : nd->y + 6;
            sr_draw_text_shadow(px, W, H, nd->x - 20, label_y,
                                nd->name, col, shadow);
            if (selectable) {
                char rbuf[24];
                switch (nd->node_type) {
                    case NODE_BOSS:     snprintf(rbuf, sizeof(rbuf), "BOSS"); break;
                    case NODE_MINIBOSS: snprintf(rbuf, sizeof(rbuf), "ELITE D%d", nd->difficulty); break;
                    case NODE_EVENT:    snprintf(rbuf, sizeof(rbuf), "EVENT"); break;
                    case NODE_JUNKERS:  snprintf(rbuf, sizeof(rbuf), "JUNKERS"); break;
                    default:            snprintf(rbuf, sizeof(rbuf), "D%d", nd->difficulty); break;
                }
                uint32_t tag_col;
                switch (nd->node_type) {
                    case NODE_BOSS:     tag_col = 0xFFCC8844; break;
                    case NODE_MINIBOSS: tag_col = 0xFF44AACC; break;
                    case NODE_EVENT:    tag_col = 0xFFCCCC44; break;
                    case NODE_JUNKERS:  tag_col = 0xFF32A0DC; break;
                    default:            tag_col = 0xFF888888; break;
                }
                sr_draw_text_shadow(px, W, H, nd->x - 20, label_y + 10,
                                    rbuf, tag_col, shadow);
            }
        }
    }

    /* ── Confirm dialog ────────────────────────────────────────── */
    if (g_starmap.confirm_active) {
        int ct = g_starmap.confirm_target;
        if (ct >= 0 && ct < g_starmap.node_count) {
            starmap_node *tgt = &g_starmap.nodes[ct];
            /* Dialog box */
            int dbw = 160, dbh = 50;
            int dbx = W/2 - dbw/2, dby = H/2 - dbh/2;
            for (int ry = dby; ry < dby + dbh && ry < H; ry++)
                for (int rx = dbx; rx < dbx + dbw && rx < W; rx++)
                    if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF111133;
            /* Border */
            for (int rx = dbx; rx < dbx + dbw && rx < W; rx++) {
                if (dby >= 0 && dby < H) px[dby * W + rx] = 0xFF4444AA;
                if (dby+dbh-1 >= 0 && dby+dbh-1 < H) px[(dby+dbh-1) * W + rx] = 0xFF4444AA;
            }
            for (int ry = dby; ry < dby + dbh && ry < H; ry++) {
                if (dbx >= 0 && dbx < W) px[ry * W + dbx] = 0xFF4444AA;
                if (dbx+dbw-1 >= 0 && dbx+dbw-1 < W) px[ry * W + dbx+dbw-1] = 0xFF4444AA;
            }
            sr_draw_text_shadow(px, W, H, dbx + 10, dby + 6, "JUMP TO", 0xFFCCCCCC, shadow);
            sr_draw_text_shadow(px, W, H, dbx + 10, dby + 16, tgt->name, 0xFF00DDDD, shadow);
            {
                char dbuf[32];
                switch (tgt->node_type) {
                    case NODE_BOSS:     snprintf(dbuf, sizeof(dbuf), "BOSS FIGHT"); break;
                    case NODE_MINIBOSS: snprintf(dbuf, sizeof(dbuf), "ELITE D%d", tgt->difficulty); break;
                    case NODE_EVENT:    snprintf(dbuf, sizeof(dbuf), "EVENT"); break;
                    case NODE_JUNKERS:  snprintf(dbuf, sizeof(dbuf), "TRADE JUNK"); break;
                    default:            snprintf(dbuf, sizeof(dbuf), "D%d", tgt->difficulty); break;
                }
                sr_draw_text_shadow(px, W, H, dbx + 10, dby + 26, dbuf, 0xFF888888, shadow);
            }

            if (ui_button(px, W, H, dbx + 10, dby + dbh - 16, 60, 14, "YES",
                          0xFF112211, 0xFF223322, 0xFF44CC44)) {
                tgt->visited = true;
                g_starmap.current_node = ct;
                if (g_starmap.visited_path_count < STARMAP_MAX_NODES)
                    g_starmap.visited_path[g_starmap.visited_path_count++] = ct;
                player_sector = tgt->difficulty;
                g_starmap.confirm_active = false;

                if (tgt->node_type == NODE_EVENT) {
                    /* Event node: show event dialog instead of loading a dungeon */
                    g_event.active = true;
                    g_event.event_id = tgt->event_id;
                    if (g_event.event_id < 0 || g_event.event_id >= EVENT_MAX_EVENTS)
                        g_event.event_id = 0;
                    g_event.showing_result = false;
                    g_event.result_timer = 0;
                    /* Stay on starmap - event overlay will draw on top */
                } else if (tgt->node_type == NODE_JUNKERS) {
                    /* Junkers node: open junk trade overlay */
                    g_junkers.active = true;
                    g_junkers.junk_traded = 0;
                } else {
                    /* Derelict node (normal, miniboss, boss): load dungeon */
                    g_starmap.derelicts_visited++;
                    current_mission_is_boss = tgt->is_boss;
                    current_mission_is_miniboss = (tgt->node_type == NODE_MINIBOSS);
                    if (current_mission_is_miniboss) {
                        /* Pick a miniboss type based on difficulty */
                        int mb_idx = tgt->difficulty % 4;
                        current_miniboss_type = ENEMY_MINIBOSS_1 + mb_idx;
                    }
                    /* Clear pregen so new ship loads from starmap node's levelFile */
                    memset(dng_state.floor_generated, 0, sizeof(dng_state.floor_generated));
                    remote_hull_computed = false;
                    hub_generate(&g_hub);
                    g_hub.mission_available = true;
                    app_state = STATE_SHIP_HUB;
                    snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "JUMPED TO %s", tgt->name);
                    g_hub.hud_msg_timer = 90;
                    /* Restock medbay medpacks by 1, capped at the max. */
                    if (g_medbay_kit_stock < MEDBAY_KIT_STOCK_MAX)
                        g_medbay_kit_stock++;
                }
                game_save();
            }
            if (ui_button(px, W, H, dbx + dbw - 70, dby + dbh - 16, 60, 14, "NO",
                          0xFF221111, 0xFF332222, 0xFF882222)) {
                g_starmap.confirm_active = false;
            }
        }
    } else {
        /* Jump button - opens confirm dialog. Hidden while a mission is
           active; the player can still browse the starmap but cannot jump
           until the current derelict is cleared. */
        if (g_hub.mission_available) {
            sr_draw_text_shadow(px, W, H, W/2 - 17 * 3, H - 26,
                                "CLEAR THE DERELICT TO JUMP", 0xFFCC8822, shadow);
        } else if (reachable_count > 0) {
            if (ui_button(px, W, H, W/2 - 30, H - 30, 60, 14, "JUMP",
                          0xFF111133, 0xFF222255, 0xFF333377)) {
                int next = reachable[g_starmap.cursor];
                if (next >= 0 && next < g_starmap.node_count) {
                    g_starmap.confirm_active = true;
                    g_starmap.confirm_target = next;
                }
            }
        }
    }

    /* ── Event overlay ─────────────────────────────────────────── */
    if (g_event.active) {
        const space_event *ev = &g_events[g_event.event_id];
        int ebw = 260, ebh = 160;
        int ebx = W/2 - ebw/2, eby = H/2 - ebh/2;
        /* Background */
        for (int ry = eby; ry < eby + ebh && ry < H; ry++)
            for (int rx = ebx; rx < ebx + ebw && rx < W; rx++)
                if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF0C0C1E;
        /* Border */
        for (int rx = ebx; rx < ebx + ebw && rx < W; rx++) {
            if (eby >= 0 && eby < H) px[eby * W + rx] = 0xFF4488AA;
            if (eby+ebh-1 >= 0 && eby+ebh-1 < H) px[(eby+ebh-1) * W + rx] = 0xFF4488AA;
        }
        for (int ry = eby; ry < eby + ebh && ry < H; ry++) {
            if (ebx >= 0 && ebx < W) px[ry * W + ebx] = 0xFF4488AA;
            if (ebx+ebw-1 >= 0 && ebx+ebw-1 < W) px[ry * W + ebx+ebw-1] = 0xFF4488AA;
        }

        if (g_event.showing_result) {
            /* Show outcome text */
            sr_draw_text_shadow(px, W, H, ebx + 10, eby + 8, ev->title, 0xFF00DDDD, shadow);
            sr_draw_text_wrap(px, W, H, ebx + 10, eby + 24, g_event.result_text, 240, 10, 0xFFCCCCCC, shadow);

            /* Continue button */
            if (ui_button(px, W, H, W/2 - 30, eby + ebh - 20, 60, 14, "CONTINUE",
                          0xFF112211, 0xFF223322, 0xFF44CC44)) {
                g_event.active = false;
                /* Return to starmap (node already visited) */
            }
        } else {
            /* Show event description + choices */
            sr_draw_text_shadow(px, W, H, ebx + 10, eby + 8, ev->title, 0xFF00DDDD, shadow);
            sr_draw_text_wrap(px, W, H, ebx + 10, eby + 24, ev->description, 240, 10, 0xFFCCCCCC, shadow);

            /* Draw choice buttons */
            int btn_y = eby + 80;
            for (int c = 0; c < ev->choice_count; c++) {
                const event_choice *ch = &ev->choices[c];
                if (ui_button(px, W, H, ebx + 10, btn_y, 240, 14, ch->text,
                              0xFF111133, 0xFF222255, 0xFF6688AA)) {
                    /* Resolve outcome */
                    int roll = dng_rng_int(100);
                    bool success = (roll < ch->success_chance);

                    if (success && ch->good_amount > 0) {
                        switch (ch->outcome_good) {
                            case OUTCOME_SCRAP:
                                player_scrap += ch->good_amount;
                                break;
                            case OUTCOME_BIOMASS:
                                player_biomass += ch->good_amount;
                                break;
                            case OUTCOME_CARD: {
                                /* Add random good cards */
                                int good_cards[] = { CARD_DOUBLE_SHOT, CARD_FORTIFY, CARD_STUN,
                                                     CARD_REPAIR, CARD_OVERCHARGE, CARD_DASH,
                                                     CARD_SNIPER, CARD_SHOTGUN, CARD_LASER };
                                for (int k = 0; k < ch->good_amount; k++) {
                                    int card = good_cards[dng_rng_int(9)];
                                    if (g_player.persistent_deck_count < COMBAT_DECK_MAX)
                                        g_player.persistent_deck[g_player.persistent_deck_count++] = card;
                                }
                                break;
                            }
                            case OUTCOME_HEAL:
                                g_player.hp += ch->good_amount;
                                if (g_player.hp > g_player.hp_max) g_player.hp = g_player.hp_max;
                                break;
                        }
                        snprintf(g_event.result_text, sizeof(g_event.result_text), "%s", ch->good_text);
                    } else if (!success && ch->bad_amount > 0) {
                        switch (ch->outcome_bad) {
                            case OUTCOME_HEALTH_LOSS:
                                g_player.hp -= ch->bad_amount;
                                if (g_player.hp < 1) g_player.hp = 1;
                                break;
                            case OUTCOME_JUNK_CARD:
                                for (int k = 0; k < ch->bad_amount; k++) {
                                    if (g_player.persistent_deck_count < COMBAT_DECK_MAX)
                                        g_player.persistent_deck[g_player.persistent_deck_count++] = CARD_JUNK;
                                }
                                break;
                            case OUTCOME_MAX_HP_LOSS:
                                g_player.hp_max -= ch->bad_amount;
                                if (g_player.hp_max < 20) g_player.hp_max = 20;
                                if (g_player.hp > g_player.hp_max) g_player.hp = g_player.hp_max;
                                break;
                        }
                        snprintf(g_event.result_text, sizeof(g_event.result_text), "%s", ch->bad_text);
                    } else {
                        /* No effect (safe choice or 100% success with amount 0) */
                        snprintf(g_event.result_text, sizeof(g_event.result_text), "%s",
                                 success ? ch->good_text : ch->bad_text);
                        if (g_event.result_text[0] == '\0')
                            snprintf(g_event.result_text, sizeof(g_event.result_text), "Nothing happened.");
                    }
                    g_event.showing_result = true;
                    game_save();
                }
                btn_y += 20;
            }
        }
    }

    /* ── Junkers trade overlay ──────────────────────────────────── */
    if (g_junkers.active) {
        int jw = 220, jh = 120;
        int jx = W/2 - jw/2, jy = H/2 - jh/2;
        /* Background */
        for (int ry = jy; ry < jy + jh && ry < H; ry++)
            for (int rx = jx; rx < jx + jw && rx < W; rx++)
                if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF0E1218;
        /* Border */
        uint32_t jborder = 0xFF32A0DC;
        for (int rx = jx; rx < jx + jw && rx < W; rx++) {
            if (jy >= 0 && jy < H) px[jy * W + rx] = jborder;
            if (jy+jh-1 >= 0 && jy+jh-1 < H) px[(jy+jh-1) * W + rx] = jborder;
        }
        for (int ry = jy; ry < jy + jh && ry < H; ry++) {
            if (jx >= 0 && jx < W) px[ry * W + jx] = jborder;
            if (jx+jw-1 >= 0 && jx+jw-1 < W) px[ry * W + jx+jw-1] = jborder;
        }

        sr_draw_text_shadow(px, W, H, jx + 10, jy + 6, "JUNKERS", 0xFF32A0DC, shadow);
        sr_draw_text_shadow(px, W, H, jx + 10, jy + 18,
                            "Trade junk cards for scrap.", 0xFFAAAAAA, shadow);

        int junk_count = junkers_count_junk();
        int trades_left = JUNKERS_MAX_TRADES - g_junkers.junk_traded;

        char jbuf[48];
        snprintf(jbuf, sizeof(jbuf), "JUNK IN DECK: %d", junk_count);
        sr_draw_text_shadow(px, W, H, jx + 10, jy + 34, jbuf, 0xFFCCCCCC, shadow);

        snprintf(jbuf, sizeof(jbuf), "TRADES LEFT: %d", trades_left);
        sr_draw_text_shadow(px, W, H, jx + 10, jy + 44, jbuf, 0xFFCCCCCC, shadow);

        snprintf(jbuf, sizeof(jbuf), "SCRAP: %d", player_scrap);
        sr_draw_text_shadow(px, W, H, jx + 10, jy + 54, jbuf, 0xFF00DDDD, shadow);

        if (junk_count > 0 && trades_left > 0) {
            snprintf(jbuf, sizeof(jbuf), "TRADE 1 JUNK (+%d SCRAP)", JUNKERS_SCRAP_PER_JUNK);
            if (ui_button(px, W, H, jx + 10, jy + 70, jw - 20, 14, jbuf,
                          0xFF112211, 0xFF223322, 0xFF44CC44)) {
                if (junkers_remove_one_junk()) {
                    player_scrap += JUNKERS_SCRAP_PER_JUNK;
                    g_junkers.junk_traded++;
                    game_save();
                }
            }
        } else if (junk_count == 0) {
            sr_draw_text_shadow(px, W, H, jx + 10, jy + 74,
                                "No junk to trade.", 0xFF888888, shadow);
        } else {
            sr_draw_text_shadow(px, W, H, jx + 10, jy + 74,
                                "All trades used.", 0xFF888888, shadow);
        }

        if (ui_button(px, W, H, jx + jw/2 - 30, jy + jh - 18, 60, 14, "LEAVE",
                      0xFF221111, 0xFF332222, 0xFF882222)) {
            g_junkers.active = false;
        }
    }

    /* Back button (disabled during event or junkers) */
    if (!g_event.active && !g_junkers.active) {
        if (ui_button(px, W, H, W/2 - 30, H - 14, 60, 12, "BACK",
                      0xFF111122, 0xFF222244, 0xFF333366)) {
            g_starmap.active = false;
            g_starmap.confirm_active = false;
            app_state = STATE_SHIP_HUB;
        }
    }
}

static void starmap_handle_key(int key_code) {
    if (!g_starmap.active) return;

    if (g_starmap.confirm_active) {
        /* Confirm dialog: Y/Enter = yes, N/Escape = no */
        if (key_code == SAPP_KEYCODE_Y || key_code == SAPP_KEYCODE_ENTER ||
            key_code == SAPP_KEYCODE_SPACE) {
            int ct = g_starmap.confirm_target;
            if (ct >= 0 && ct < g_starmap.node_count) {
                starmap_node *tgt = &g_starmap.nodes[ct];
                tgt->visited = true;
                g_starmap.current_node = ct;
                if (g_starmap.visited_path_count < STARMAP_MAX_NODES)
                    g_starmap.visited_path[g_starmap.visited_path_count++] = ct;
                player_sector = tgt->difficulty;
                g_starmap.confirm_active = false;

                if (tgt->node_type == NODE_EVENT) {
                    g_event.active = true;
                    g_event.event_id = tgt->event_id;
                    if (g_event.event_id < 0 || g_event.event_id >= EVENT_MAX_EVENTS)
                        g_event.event_id = 0;
                    g_event.showing_result = false;
                    g_event.result_timer = 0;
                } else if (tgt->node_type == NODE_JUNKERS) {
                    g_junkers.active = true;
                    g_junkers.junk_traded = 0;
                } else {
                    g_starmap.derelicts_visited++;
                    current_mission_is_boss = tgt->is_boss;
                    current_mission_is_miniboss = (tgt->node_type == NODE_MINIBOSS);
                    if (current_mission_is_miniboss) {
                        int mb_idx = tgt->difficulty % 4;
                        current_miniboss_type = ENEMY_MINIBOSS_1 + mb_idx;
                    }
                    memset(dng_state.floor_generated, 0, sizeof(dng_state.floor_generated));
                    remote_hull_computed = false;
                    hub_generate(&g_hub);
                    g_hub.mission_available = true;
                    app_state = STATE_SHIP_HUB;
                    snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "JUMPED TO %s", tgt->name);
                    g_hub.hud_msg_timer = 90;
                    /* Restock medbay medpacks by 1, capped at the max. */
                    if (g_medbay_kit_stock < MEDBAY_KIT_STOCK_MAX)
                        g_medbay_kit_stock++;
                }
                game_save();
            }
        } else if (key_code == SAPP_KEYCODE_N || key_code == SAPP_KEYCODE_ESCAPE) {
            g_starmap.confirm_active = false;
        }
        return;
    }

    starmap_node *cur = &g_starmap.nodes[g_starmap.current_node];

    if (key_code == SAPP_KEYCODE_LEFT || key_code == SAPP_KEYCODE_A) {
        g_starmap.cursor--;
        if (g_starmap.cursor < 0) g_starmap.cursor = cur->next_count - 1;
    }
    if (key_code == SAPP_KEYCODE_RIGHT || key_code == SAPP_KEYCODE_D) {
        g_starmap.cursor++;
        if (g_starmap.cursor >= cur->next_count) g_starmap.cursor = 0;
    }
}

/* ── Deck viewer overlay ───────────────────────────────────────── */

static bool deck_view_active = false;
static int deck_view_selected = 0; /* row cursor into player deck */

/* card_description_text() is now in sr_combat.h */

static void draw_deck_viewer(uint32_t *px, int W, int H) {
    if (!deck_view_active) return;
    uint32_t shadow = 0xFF000000;

    /* Darken background */
    for (int i = 0; i < W * H; i++) {
        uint32_t c = px[i];
        int r = ((c >> 0) & 0xFF) / 4;
        int g = ((c >> 8) & 0xFF) / 4;
        int b = ((c >> 16) & 0xFF) / 4;
        px[i] = 0xFF000000 | (b << 16) | (g << 8) | r;
    }

    /* Clamp cursor */
    if (g_player.persistent_deck_count <= 0) deck_view_selected = 0;
    else if (deck_view_selected < 0) deck_view_selected = 0;
    else if (deck_view_selected >= g_player.persistent_deck_count)
        deck_view_selected = g_player.persistent_deck_count - 1;

    /* Header */
    char hdr[48];
    snprintf(hdr, sizeof(hdr), "YOUR DECK (%d CARDS)", g_player.persistent_deck_count);
    sr_draw_text_shadow(px, W, H, 30, 6, hdr, 0xFFD0CDC7, shadow);

    /* Bottom area reserved for HP / medpack / close - keeps list + detail
       panel from colliding with the footer. */
    int footer_y = H - 42;

    /* Card list (left side) - matches trash menu layout */
    int list_x = 30, list_y = 18;
    int list_w = 100;
    int row_h = 11;
    int max_rows = (footer_y - list_y) / row_h;
    if (max_rows < 1) max_rows = 1;
    int scroll = 0;
    if (deck_view_selected >= max_rows)
        scroll = deck_view_selected - max_rows + 1;
    for (int i = 0; i < g_player.persistent_deck_count; i++) {
        int row = i - scroll;
        if (row < 0 || row >= max_rows) continue;
        int cy = list_y + row * row_h;

        bool hovered = false;
        ui_row_hover(list_x, cy, list_w, row_h - 1, &hovered);
        if (hovered) deck_view_selected = i;
        bool sel = (deck_view_selected == i);

        int card_type = g_player.persistent_deck[i];
        uint32_t ccol = sel ? 0xFFD0CDC7 : card_colors[card_type];
        if (sel) {
            /* Highlight bar */
            for (int ry = cy; ry < cy + row_h - 1 && ry < H; ry++)
                for (int rx = list_x; rx < list_x + list_w && rx < W; rx++)
                    if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF46353E;
        }
        sr_draw_text_shadow(px, W, H, list_x + 2, cy + 1,
                            card_names[card_type], ccol, shadow);

        /* Energy cost pip */
        char ebuf[4];
        snprintf(ebuf, sizeof(ebuf), "%d", card_energy_cost[card_type]);
        sr_draw_text_shadow(px, W, H, list_x + list_w - 12, cy + 1,
                            ebuf, 0xFF9BAF0E, shadow);
    }

    /* Card detail panel (right side) */
    if (deck_view_selected >= 0 && deck_view_selected < g_player.persistent_deck_count) {
        int idx = deck_view_selected;
        int card_type = g_player.persistent_deck[idx];
        int px2 = 150, py = 18, pw = W - 180, ph = footer_y - py - 4;
        uint32_t ccol = card_colors[card_type];

        /* Panel background */
        for (int ry = py; ry < py + ph && ry < H; ry++)
            for (int rx = px2; rx < px2 + pw && rx < W; rx++)
                if (rx >= 0 && ry >= 0) px[ry * W + rx] = 0xFF2F222E;

        /* Border in card color */
        combat_draw_rect_outline(px, W, H, px2, py, pw, ph, ccol);

        /* Card art */
        if (card_type < (int)(sizeof(spr_card_table)/sizeof(spr_card_table[0])) &&
            spr_card_table[card_type])
            spr_draw(px, W, H, spr_card_table[card_type], px2 + pw - 42, py + 4, 2);

        /* Card name */
        sr_draw_text_shadow(px, W, H, px2 + 6, py + 6,
                            card_names[card_type], ccol, shadow);

        /* Energy cost */
        char ebuf2[16];
        snprintf(ebuf2, sizeof(ebuf2), "COST: %d",
                 card_energy_cost[card_type]);
        sr_draw_text_shadow(px, W, H, px2 + 6, py + 16, ebuf2,
                            0xFF9BAF0E, shadow);

        /* Effect text */
        const char *effect = card_effect_text(card_type);
        int ey = sr_draw_text_wrap(px, W, H, px2 + 6, py + 28, effect,
                                   pw - 50, 8, ccol, shadow);

        /* Description */
        const char *desc = card_description_text(card_type);
        sr_draw_text_wrap(px, W, H, px2 + 6, ey + 4, desc,
                          pw - 12, 8, 0xFF8A707F, shadow);
    }

    /* Medpack slots - usable any time. Heal 10 HP, consumes the kit. */
    {
        int hp_x = 30, hp_y = footer_y;
        char hpbuf[24];
        snprintf(hpbuf, sizeof(hpbuf), "HP  %d/%d",
                 g_player.hp, g_player.hp_max);
        sr_draw_text_shadow(px, W, H, hp_x, hp_y, hpbuf, 0xFF22CC22, shadow);

        int bx = hp_x + 60, by = hp_y - 2;
        for (int s = 0; s < CONSUMABLE_SLOTS; s++) {
            if (player_consumables[s] != CONSUMABLE_HEALTH_KIT) continue;
            if (ui_button(px, W, H, bx, by, 74, 12, "USE MEDPACK",
                          0xFF113322, 0xFF226644, 0xFF44CC88)) {
                int heal = 10;
                g_player.hp += heal;
                if (g_player.hp > g_player.hp_max)
                    g_player.hp = g_player.hp_max;
                player_consumables[s] = CONSUMABLE_NONE;
            }
            bx += 78;
        }
    }

    /* Close button */
    if (ui_button(px, W, H, W - 60, H - 18, 50, 14, "CLOSE",
                  0xFF111122, 0xFF222244, 0xFF333366)) {
        deck_view_active = false;
        deck_view_selected = 0;
    }
}

/* ── Hub scene drawing (reuses dungeon renderer with hub lighting) ── */

static void hub_draw_scene(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = &g_hub.dungeon;
    dng_player *p = &g_hub.player;

    /* Override the lighting for hub using hub config */
    float save_ambient = dng_cfg.ambient_brightness;
    float save_torch = dng_cfg.light_brightness;
    float save_rl_color[3], save_rl_bright, save_rl_radius;
    memcpy(save_rl_color, dng_cfg.room_light_color, sizeof(save_rl_color));
    save_rl_bright = dng_cfg.room_light_brightness;
    save_rl_radius = dng_cfg.room_light_radius;

    dng_cfg.ambient_brightness = hub_cfg.ambient_brightness;
    dng_cfg.light_brightness = hub_cfg.torch_brightness;
    memcpy(dng_cfg.room_light_color, hub_cfg.room_light_color, sizeof(dng_cfg.room_light_color));
    dng_cfg.room_light_brightness = hub_cfg.room_light_brightness;
    dng_cfg.room_light_radius = hub_cfg.room_light_radius;

    /* Temporarily point dng_state at hub dungeon for rendering */
    sr_dungeon *save_dungeon = dng_state.dungeon;
    dng_player save_player = dng_state.player;
    dng_state.dungeon = d;
    dng_state.player = *p;

    /* Hub: all cells visible (no fog of war on your own ship) */
    for (int gy = 1; gy <= d->h; gy++)
        for (int gx = 1; gx <= d->w; gx++)
            dng_vis[gy][gx] = true;

    /* Use fog mode (mode 1) for hub with hub-specific fog function */
    int save_light_mode = dng_light_mode;
    int save_render_radius = dng_render_radius;
    dng_light_mode = 1;
    dng_fog_fn = hub_fog_vertex_intensity;
    dng_render_radius = hub_cfg.draw_distance;
    dng_sprites_unlit = true;
    dng_wall_texture = ITEX_HUB_CORRIDOR;
    dng_room_wall_texture = ITEX_WALL_A;
    dng_floor_texture = ITEX_HUB_FLOOR;
    dng_ceiling_texture = ITEX_HUB_CEILING;
    dng_skip_pillars = true;

    sr_mat4 vp;
    draw_dungeon_scene(fb_ptr, &vp);


    /* Render enemy ship exterior visible outside windows (100 units north) */
    if (current_ship.initialized && g_hub.mission_available &&
        dng_state.floor_generated[0] && dng_state.floors[0].w > 0) {
        sr_dungeon *enemy_d = &dng_state.floors[0];
        /* Build a separate MVP with extended far plane to reach the enemy ship */
        float cam_angle = p->angle * 6.28318f;
        float cam_x = p->x, cam_y = p->y, cam_z = p->z;
        float ca_cos = cosf(cam_angle), ca_sin = sinf(cam_angle);
        sr_vec3 eye = { cam_x, cam_y, cam_z };
        sr_vec3 fwd = { ca_sin, 0, -ca_cos };
        sr_vec3 target = { eye.x + fwd.x, eye.y + fwd.y, eye.z + fwd.z };
        sr_vec3 up = { 0, 1, 0 };
        sr_mat4 view = sr_mat4_lookat(eye, target, up);
        sr_mat4 proj = sr_mat4_perspective(
            70.0f * 3.14159f / 180.0f,
            (float)fb_ptr->width / (float)fb_ptr->height,
            0.05f, 500.0f); /* far plane extended for remote ship */
        sr_mat4 remote_mvp = sr_mat4_mul(proj, view);

        /* Select config based on enemy ship grid size */
        enemy_ship_cfg *esc = (enemy_d->w >= 80) ? &enemy_ship_large
                            : (enemy_d->w >= 40) ? &enemy_ship_medium
                            : &enemy_ship_small;
        float center_x = -(enemy_d->w * DNG_CELL_SIZE) * 0.5f + (d->w * DNG_CELL_SIZE) * 0.5f;
        float hover = sinf((float)dng_time * esc->hover_speed) * esc->hover_amp;
        float ship_ox = center_x + esc->x_off;
        float ship_oy = esc->y_off + hover;
        float ship_oz = esc->z_off;
        static bool _rs_logged = false;
        if (!_rs_logged) {
            printf("_ship] w=%d h=%d ox=%.1f oz=%.1f\n",
                   enemy_d->w, enemy_d->h, ship_ox, ship_oz);
            _rs_logged = true;
        }
        sr_set_pixel_light_fn(NULL);
        /* Render all generated floors of the remote enemy ship */
        float floor_height = DNG_CELL_SIZE;
        for (int fl = 0; fl < dng_state.max_floors && fl < DNG_MAX_FLOORS; fl++) {
            if (!dng_state.floor_generated[fl]) continue;
            float fl_oy = ship_oy + fl * floor_height;
            sr_dungeon *fl_d = &dng_state.floors[fl];
            /* Set floor above for roof culling */
            _remote_floor_above = NULL;
            if (fl + 1 < dng_state.max_floors && fl + 1 < DNG_MAX_FLOORS && dng_state.floor_generated[fl + 1])
                _remote_floor_above = &dng_state.floors[fl + 1];
            draw_remote_ship_interior(fb_ptr, &remote_mvp, fl_d, ship_ox, fl_oy, ship_oz, true);
            draw_remote_ship_exterior(fb_ptr, &remote_mvp, fl_d, ship_ox, fl_oy, ship_oz, true);
        }
        _remote_floor_above = NULL;
    }

    /* Restore */
    dng_state.dungeon = save_dungeon;
    dng_state.player = save_player;
    dng_light_mode = save_light_mode;
    dng_render_radius = save_render_radius;
    dng_fog_fn = NULL;
    dng_sprites_unlit = false;
    dng_wall_texture = -1;
    dng_room_wall_texture = -1;
    dng_floor_texture = -1;
    dng_ceiling_texture = -1;
    dng_skip_pillars = false;
    dng_alien_exterior = false;
    dng_cfg.ambient_brightness = save_ambient;
    dng_cfg.light_brightness = save_torch;
    memcpy(dng_cfg.room_light_color, save_rl_color, sizeof(dng_cfg.room_light_color));
    dng_cfg.room_light_brightness = save_rl_bright;
    dng_cfg.room_light_radius = save_rl_radius;
}

/* ── Hub HUD overlay ────────────────────────────────────────────── */

static void hub_draw_hud(uint32_t *px, int W, int H) {
    uint32_t shadow = 0xFF000000;

    /* Ship name */
    sr_draw_text_shadow(px, W, H, 4, 4, "ISS ENDEAVOR", 0xFF22CCEE, shadow);

    /* Player HP */
    char hp_num[32];
    snprintf(hp_num, sizeof(hp_num), "%d/%d", g_player.hp, g_player.hp_max);
    sr_draw_text_shadow(px, W, H, 4, 14, "HP", 0xFFCCCCCC, shadow);
    sr_draw_text_shadow(px, W, H, 4 + 3 * 6, 14, hp_num, 0xFF22CC22, shadow);

    /* Currency */
    char scrap_num[32];
    snprintf(scrap_num, sizeof(scrap_num), "%d", player_scrap);
    sr_draw_text_shadow(px, W, H, 4, 24, "SCRAP", 0xFFCCCCCC, shadow);
    sr_draw_text_shadow(px, W, H, 4 + 6 * 6, 24, scrap_num, 0xFFEECC44, shadow);
    char bio_num[32];
    snprintf(bio_num, sizeof(bio_num), "%d", player_biomass);
    sr_draw_text_shadow(px, W, H, 4, 34, "BIO", 0xFFCCCCCC, shadow);
    sr_draw_text_shadow(px, W, H, 4 + 4 * 6, 34, bio_num, 0xFF44CC88, shadow);

    /* Sector + Samples - right-aligned, leaving room for hamburger menu at W-16..W-4 */
    char sec_buf[32];
    snprintf(sec_buf, sizeof(sec_buf), "SECTOR %d", player_sector + 1);
    sr_draw_text_shadow(px, W, H, W - 18 - sr_text_width(sec_buf), 4, sec_buf, 0xFF888888, shadow);

    if (mission_briefed) {
        char samp_buf[32];
        snprintf(samp_buf, sizeof(samp_buf), "SAMPLES %d/%d", player_samples, SAMPLES_REQUIRED);
        uint32_t samp_col = (player_samples >= SAMPLES_REQUIRED) ? 0xFF44CC44 : 0xFFCC8822;
        sr_draw_text_shadow(px, W, H, W - 18 - sr_text_width(samp_buf), 14, samp_buf, samp_col, shadow);
    }

    /* Mission objectives */
    {
        int oy = 50;
        bool prep_done = mission_briefed && mission_medbay_done &&
                         mission_medbay_card_bought && mission_armory_done;
        if (!prep_done && g_dlgd.loaded) {
            /* Initial prep flow: captain, medbay heal, medbay buy, armory */
            sr_draw_text_shadow(px, W, H, 4, oy, "OBJECTIVES:", 0xFFCC8822, shadow);
            oy += 10;
            bool obj_done[4] = { mission_briefed, mission_medbay_done,
                                  mission_medbay_card_bought, mission_armory_done };
            for (int i = 0; i < g_dlgd.objective_count && i < 4; i++) {
                char obj_buf[DLGD_LINE_LEN + 8];
                bool done = obj_done[i];
                snprintf(obj_buf, sizeof(obj_buf), "[%c] %s", done ? 'X' : ' ', g_dlgd.objectives[i]);
                uint32_t col = done ? 0xFF448844 : 0xFFCCCCCC;
                sr_draw_text_shadow(px, W, H, 8, oy, obj_buf, col, shadow);
                oy += 10;
            }
        } else if (!g_hub.mission_available) {
            /* Post-mission: plot course */
            sr_draw_text_shadow(px, W, H, 4, oy, "OBJECTIVES:", 0xFFCC8822, shadow);
            oy += 10;
            sr_draw_text_shadow(px, W, H, 8, oy, "[ ] PLOT A COURSE FOR THE NEXT SYSTEM", 0xFFCCCCCC, shadow);
        } else if (g_hub.mission_available) {
            /* Arrived at new system: investigate */
            sr_draw_text_shadow(px, W, H, 4, oy, "OBJECTIVES:", 0xFFCC8822, shadow);
            oy += 10;
            sr_draw_text_shadow(px, W, H, 8, oy, "[ ] INVESTIGATE DERELICT", 0xFFCCCCCC, shadow);
            oy += 10;
            sr_draw_text_shadow(px, W, H, 8, oy, "[ ] COLLECT BIOMASS", 0xFFCCCCCC, shadow);
        }
    }

    /* Deck button (clickable) - sits above the hub minimap. The hub scene
       sets dng_minimap_y = 42 so the button (y=26..38) has room. */
    char deck_buf[32];
    snprintf(deck_buf, sizeof(deck_buf), "DECK %d", g_player.persistent_deck_count);
    int deck_btn_y = 26;
    if (ui_button(px, W, H, W - 70, deck_btn_y, 66, 12, deck_buf,
                  0xFF1A1A2A, 0xFF222244, 0xFF333366)) {
        deck_view_active = true;
        deck_view_selected = -1;
    }

    /* Room/NPC interaction button - merged room name + action into one button */
    /* Skip when deck viewer is open (modal) or dialog is active */
    if (deck_view_active || g_dialog.active || elem_gift_active) return;

    int room_idx = hub_room_at_pos(g_hub.player.gx, g_hub.player.gy);
    int look_gx = g_hub.player.gx + dng_dir_dx[g_hub.player.dir];
    int look_gy = g_hub.player.gy + dng_dir_dz[g_hub.player.dir];
    int npc = hub_npc_at(look_gx, look_gy);

    if (npc >= 0) {
        /* NPC in front - show "TALK: NAME" button in room color */
        int npc_room = g_hub.crew[npc].room;
        uint32_t rc = (npc_room >= 0 && npc_room < HUB_ROOM_COUNT)
            ? hub_room_colors[g_hub.room_types[npc_room]] : 0xFF00DDDD;
        /* Derive button colors from room color */
        uint32_t r = (rc>>0)&0xFF, g = (rc>>8)&0xFF, b = (rc>>16)&0xFF;
        uint32_t base  = 0xFF000000 | ((b/5)<<16) | ((g/5)<<8) | (r/5);
        uint32_t hover = 0xFF000000 | ((b/3)<<16) | ((g/3)<<8) | (r/3);

        char buf[32];
        snprintf(buf, sizeof(buf), "TALK: %s", g_hub.crew[npc].name);
        int llen = 0; for (const char *c = buf; *c; c++) llen++;
        int bw = llen * 6 + 12, bh = 14;
        if (ui_button(px, W, H, W/2 - bw/2, H - 18, bw, bh, buf, base, hover, rc)) {
            int action = DIALOG_ACTION_NONE;
            if (npc_room >= 0 && npc_room < g_hub.dungeon.room_count)
                action = hub_room_action_for_type(g_hub.room_types[npc_room]);
            hub_start_dialog(npc, action);
        }
    } else if (room_idx >= 0) {
        int rt = g_hub.room_types[room_idx];
        const char *btn_label = NULL;
        uint32_t rc = hub_room_colors[rt];
        int action = DIALOG_ACTION_NONE;
        if (rt == HUB_ROOM_TELEPORTER) {
            btn_label = "TELEPORTER";
            /* Only allow teleport if mission available AND full prep done. */
            if (g_hub.mission_available && mission_medbay_done &&
                mission_medbay_card_bought && mission_armory_done)
                action = DIALOG_ACTION_TELEPORT;
        } else if (rt == HUB_ROOM_BRIDGE) {
            btn_label = "BRIDGE";
            action = DIALOG_ACTION_STARMAP;
        } else if (rt == HUB_ROOM_SHOP) {
            btn_label = "ARMORY";
            /* Block armory until briefed and medbay done (or past first mission) */
            if (mission_briefed && mission_medbay_done)
                action = DIALOG_ACTION_SHOP;
        } else if (rt == HUB_ROOM_MEDBAY) {
            btn_label = "MEDBAY"; action = DIALOG_ACTION_HEAL;
        } else if (rt == HUB_ROOM_QUARTERS) {
            btn_label = "QUARTERS";
        } else if (rt == HUB_ROOM_CORRIDOR) {
            btn_label = "CORRIDOR";
        }
        if (btn_label) {
            uint32_t r = (rc>>0)&0xFF, g = (rc>>8)&0xFF, b = (rc>>16)&0xFF;
            uint32_t base  = 0xFF000000 | ((b/5)<<16) | ((g/5)<<8) | (r/5);
            uint32_t hover = 0xFF000000 | ((b/3)<<16) | ((g/3)<<8) | (r/3);

            int llen = 0; for (const char *c = btn_label; *c; c++) llen++;
            int bw = llen * 6 + 16, bh = 14;
            if (action != DIALOG_ACTION_NONE) {
                if (ui_button(px, W, H, W/2 - bw/2, H - 18, bw, bh, btn_label, base, hover, rc)) {
                    if (action == DIALOG_ACTION_TELEPORT) {
                        if (current_mission_is_boss && !g_elem_gift_given)
                            teleporter_offer_elem_gift();
                        else
                            hub_show_teleport_confirm();
                    } else {
                        memset(&g_dialog, 0, sizeof(g_dialog));
                        snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "%s", hub_room_names[rt]);
                        switch (action) {
                            case DIALOG_ACTION_STARMAP:
                                snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "ACCESSING STAR MAP...");
                                g_dialog.line_count = 1; break;
                            case DIALOG_ACTION_SHOP:
                                snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "BROWSING INVENTORY...");
                                g_dialog.line_count = 1; break;
                            case DIALOG_ACTION_HEAL:
                                snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "BIOMASS RESEARCH TERMINAL ONLINE.");
                                g_dialog.line_count = 1; break;
                        }
                        g_dialog.pending_action = action;
                        g_dialog.active = true;
                    }
                }
            } else {
                /* Non-interactive rooms - just show room name as label */
                sr_draw_text_shadow(px, W, H, W/2 - llen * 3, H - 14, btn_label, rc, shadow);
            }
        }
    }

    /* HUD message */
    if (g_hub.hud_msg_timer > 0) {
        g_hub.hud_msg_timer--;
        uint32_t msg_col = (g_hub.hud_msg_timer < 20) ? 0xFF555555 : 0xFFFFCC22;
        int mlen = 0; for (const char *c = g_hub.hud_msg; *c; c++) mlen++;
        int mx = W / 2 - mlen * 3;
        sr_draw_text_shadow(px, W, H, mx, H / 2 - 4, g_hub.hud_msg, msg_col, shadow);
    }
}

/* ── Hub minimap (reuses dungeon minimap) ──────────────────────── */

static void hub_draw_minimap(sr_framebuffer *fb_ptr) {
    /* Temporarily swap dng_state to hub data so dungeon minimap draws it */
    sr_dungeon *save_d = dng_state.dungeon;
    dng_player save_p = dng_state.player;
    dng_state.dungeon = &g_hub.dungeon;
    dng_state.player = g_hub.player;

    draw_dungeon_minimap(fb_ptr);

    dng_state.dungeon = save_d;
    dng_state.player = save_p;
}

#endif /* SR_SCENE_SHIP_HUB_H */
