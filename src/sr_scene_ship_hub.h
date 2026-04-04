/*  sr_scene_ship_hub.h — Player's own ship hub scene.
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

#define STARMAP_MAX_NODES 16
#define STARMAP_MAX_CHOICES 3

typedef struct {
    char name[24];
    int difficulty;
    int scrap_reward;
    bool visited;
    bool is_boss;       /* true = boss node (rightmost) */
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
} starmap_state;

static starmap_state g_starmap;

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

/* Which shop is active for STATE_SHOP — 0=armory, 1=medbay */
static int active_shop_type = 0;

/* ── Dialog state ───────────────────────────────────────────────── */

#define DIALOG_MAX_LINES 4
#define DIALOG_LINE_LEN 72

enum {
    DIALOG_ACTION_NONE,
    DIALOG_ACTION_STARMAP,
    DIALOG_ACTION_SHOP,
    DIALOG_ACTION_TELEPORT,       /* opens confirm prompt */
    DIALOG_ACTION_TELEPORT_GO,    /* confirmed — actually teleport */
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
} dialog_state;

static dialog_state g_dialog;

/* ── Kit display overlay (Chen first visit) ────────────────────── */

typedef struct {
    bool active;
    int card_count;
    int cards[40];         /* copy of persistent deck */
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

            dng_player_init(&hub->player, d->spawn_gx, d->spawn_gy, 1);

            /* Place NPC sprites in the dungeon grid */
            {
                static const int crew_stex[] = {
                    STEX_CREW_CAPTAIN, STEX_CREW_SERGEANT, STEX_CREW_QUARTERMASTER,
                    STEX_CREW_PRIVATE, STEX_CREW_DOCTOR, STEX_CREW_CAPTAIN
                };
                for (int i = 0; i < hub->crew_count; i++) {
                    hub_npc *npc = &hub->crew[i];
                    if (!npc->active) continue;
                    int stex = (i < 6) ? crew_stex[i] : STEX_CREW_CAPTAIN;
                    if (npc->gx >= 1 && npc->gx <= d->w && npc->gy >= 1 && npc->gy <= d->h) {
                        d->aliens[npc->gy][npc->gx] = (uint8_t)(stex + 1);
                        snprintf(d->alien_names[npc->gy][npc->gx], 16, "%s", npc->name);
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
        /* Bridge at the front (right side) — 1 wall gap above corridor */
        { 15, mid_y - 5, 4, 4, HUB_ROOM_BRIDGE },
        /* Teleporter at the back (left side) */
        { 3, mid_y - 5, 4, 4, HUB_ROOM_TELEPORTER },
        /* Shop/Armory upper-middle */
        { 9, mid_y - 5, 4, 4, HUB_ROOM_SHOP },
        /* Quarters below-left — 1 wall gap below corridor */
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

    /* Spawn in center of corridor */
    d->spawn_gx = 10;
    d->spawn_gy = mid_y;

    dng_player_init(&hub->player, d->spawn_gx, d->spawn_gy, 1);

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
    if (did < 0 || did > 4) did = 0;

    /* Captain (dialog_id 0) — complex state machine */
    if (did == 0) {
        if (!mission_first_done && !mission_briefed) {
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
        } else if (!mission_first_done) {
            /* Already briefed but haven't done first mission */
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_pre_mission);
            action = DIALOG_ACTION_NONE;
        } else if (current_map_boss_done && !g_hub.mission_available) {
            /* Just beat a boss — show sample dialog, then open star map for next run */
            int si = player_samples - 1;
            if (si < 0) si = 0; if (si > 2) si = 2;
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_sample[si]);
            action = DIALOG_ACTION_STARMAP;
        } else if (g_hub.mission_available) {
            /* Post-first-mission, under attack */
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_under_attack);
        } else {
            /* Post-first-mission, neutralized — ready to jump */
            if (g_dlgd.loaded)
                dialog_from_block(&g_dlgd.captain_post_mission);
            action = DIALOG_ACTION_STARMAP;
        }
    }
    /* SGT REYES (dialog_id 1) — teleporter */
    else if (did == 1) {
        if (!mission_first_done && !mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[1]);
            action = DIALOG_ACTION_NONE;
        } else if (!mission_first_done && mission_briefed) {
            if (mission_medbay_done && mission_armory_done) {
                if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_reyes_ready);
                action = DIALOG_ACTION_TELEPORT;
            } else {
                if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_reyes_blocked);
                action = DIALOG_ACTION_NONE;
            }
        } else {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[1]);
        }
    }
    /* QM CHEN (dialog_id 2) — armory */
    else if (did == 2) {
        if (!mission_first_done && !mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[2]);
            action = DIALOG_ACTION_NONE;
        } else if (!mission_first_done && mission_briefed && !mission_armory_done) {
            /* First visit after briefing: show starting deck cards */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_chen);
            action = DIALOG_ACTION_SHOW_KIT;
        } else {
            /* Subsequent visits: open armory shop (scrap cards) */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[2]);
            action = DIALOG_ACTION_SHOP;
        }
    }
    /* PVT KOWALSKI (dialog_id 3) — quarters */
    else if (did == 3) {
        if (!mission_first_done && !mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[3]);
        } else {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[3]);
        }
        action = DIALOG_ACTION_NONE;
    }
    /* DR VASQUEZ (dialog_id 4) — medbay */
    else if (did == 4) {
        if (!mission_first_done && !mission_briefed) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_init[4]);
            action = DIALOG_ACTION_NONE;
        } else if (!mission_first_done && mission_briefed && !mission_medbay_done) {
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.post_vasquez);
            action = DIALOG_ACTION_HEAL;
        } else {
            /* Subsequent visits: open medbay shop (elemental/biomass cards) */
            if (g_dlgd.loaded) dialog_from_block(&g_dlgd.crew_default[4]);
            action = DIALOG_ACTION_MEDBAY_SHOP;
        }
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

/* Handle CONTINUE click — instantly reveal all, or dismiss if already done. */
static bool dialog_teletype_advance(void) {
    if (g_dialog.tt_all_done) return true;
    g_dialog.tt_all_done = true;
    return false;
}

static void draw_dialog(uint32_t *px, int W, int H) {
    if (!g_dialog.active) return;
    uint32_t shadow = 0xFF000000;

    dialog_teletype_tick();

    /* Dialog box — spans most of screen width */
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

    /* Dialog lines with teletype effect — flows across all lines */
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

    /* Buttons — attached tab below the box, right side */
    int tab_y = by + bh;
    if (g_dialog.confirm_mode && g_dialog.tt_all_done) {
        /* YES / NO buttons only after all text revealed */
        if (ui_button(px, W, H, bx + bw - 140, tab_y, 60, 14,
                      "YES", 0xFF112211, 0xFF223322, 0xFF44CC44)) {
        }
        if (ui_button(px, W, H, bx + bw - 70, tab_y, 60, 14,
                      "NO", 0xFF221111, 0xFF332222, 0xFF882222)) {
        }
        /* Connect tabs to box — draw border continuation */
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
    ui_button(px, W, H, W - 60, H - 18, 50, 14,
              "CLOSE", 0xFF1A1A33, 0xFF222255, 0xFF44CC44);

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

/* Generate medbay shop: expensive elemental cards (biomass) + cheap consumables (scrap) */
static void medbay_shop_generate(shop_state *shop) {
    memset(shop, 0, sizeof(*shop));

    /* Shuffle elemental pool for variety */
    int elem[] = { CARD_ICE, CARD_ACID, CARD_FIRE, CARD_LIGHTNING };
    for (int i = 3; i > 0; i--) {
        int j = dng_rng_int(i + 1);
        int tmp = elem[i]; elem[i] = elem[j]; elem[j] = tmp;
    }

    int idx = 0;
    /* 2 elemental cards — expensive, costs biomass */
    for (int i = 0; i < 2 && idx < SHOP_MAX_CARDS; i++, idx++) {
        shop->cards[idx] = elem[i];
        shop->prices[idx] = 200;
        shop->is_bio[idx] = true;
    }
    /* 2 health kits — cheap consumables, costs scrap */
    for (int i = 0; i < 2 && idx < SHOP_MAX_CARDS; i++, idx++) {
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

    if (shop->mode == 0) {
        /* Buy mode — visual card grid */
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

                /* Click detection — open detail popup */
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

            /* BUY button */
            if (can_buy) {
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
                    } else {
                        g_player.persistent_deck[g_player.persistent_deck_count++] = shop->cards[idx];
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
    sm->nodes[0].x = 40;
    sm->nodes[0].y = FB_HEIGHT / 4;
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

            if (is_boss_col) {
                nd->scrap_reward = 40 + diff * 10;
                int bi = player_starmap;
                if (bi < 0) bi = 0; if (bi > 2) bi = 2;
                snprintf(nd->name, 24, "%s", boss_names[bi]);
            } else {
                nd->scrap_reward = 20 + diff * 5 + dng_rng_int(10);
                int name_idx = (start_sector + col * 3 + n) % NUM_SECTOR_NAMES;
                snprintf(nd->name, 24, "SECTOR %s-%d", sector_names[name_idx], diff);
            }

            nd->x = 40 + col * (FB_WIDTH - 80) / (col_count - 1);
            int map_h = FB_HEIGHT / 2; /* squished to 50% height */
            nd->y = is_boss_col ? map_h / 2
                : 20 + n * (map_h - 40) / (nodes_in_col > 1 ? nodes_in_col - 1 : 1);

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

    /* Hover detection on selectable nodes (before drawing so cursor is up to date) */
    starmap_node *cur = &g_starmap.nodes[g_starmap.current_node];
    int hovered_node = -1;
    if (!g_starmap.confirm_active) {
        for (int c = 0; c < cur->next_count; c++) {
            int target = cur->next[c];
            if (target < 0 || target >= g_starmap.node_count) continue;
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
        uint32_t col;
        if (i == g_starmap.current_node) col = 0xFF22CC22;
        else if (nd->visited) col = 0xFF555555;
        else if (nd->is_boss) col = 0xFF4444FF; /* red/orange for boss */
        else col = 0xFFCCCCFF;

        bool selectable = false;
        for (int c = 0; c < cur->next_count; c++) {
            if (cur->next[c] == i) {
                selectable = true;
                if (g_starmap.cursor == c) col = 0xFF00DDDD;
                break;
            }
        }

        bool is_hovered = (i == hovered_node);

        /* Node circle — larger glow ring when hovered */
        if (is_hovered) {
            uint32_t glow = 0xFF005555;
            for (int dy = -4; dy <= 4; dy++)
                for (int dx = -4; dx <= 4; dx++) {
                    int d2 = dx*dx + dy*dy;
                    if (d2 > 16 || d2 <= 5) continue;
                    int rx = nd->x + dx, ry = nd->y + dy;
                    if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                        px[ry * W + rx] = glow;
                }
        }

        int node_r = nd->is_boss ? 4 : 2;
        int node_r2 = node_r * node_r + node_r;
        for (int dy = -node_r; dy <= node_r; dy++)
            for (int dx = -node_r; dx <= node_r; dx++) {
                if (dx*dx + dy*dy > node_r2) continue;
                int rx = nd->x + dx, ry = nd->y + dy;
                if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                    px[ry * W + rx] = is_hovered ? 0xFF00FFFF : col;
            }

        /* Label */
        if (i == g_starmap.current_node || selectable) {
            int label_y = nd->is_boss ? nd->y + 8 : nd->y + 6;
            sr_draw_text_shadow(px, W, H, nd->x - 20, label_y,
                                nd->name, col, shadow);
            if (selectable && nd->scrap_reward > 0) {
                char rbuf[24];
                if (nd->is_boss)
                    snprintf(rbuf, sizeof(rbuf), "BOSS  ~%d SCRAP", nd->scrap_reward);
                else
                    snprintf(rbuf, sizeof(rbuf), "D%d  ~%d SCRAP", nd->difficulty, nd->scrap_reward);
                sr_draw_text_shadow(px, W, H, nd->x - 20, label_y + 10,
                                    rbuf, nd->is_boss ? 0xFFCC8844 : 0xFF888888, shadow);
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
            char dbuf[32];
            snprintf(dbuf, sizeof(dbuf), "D%d  ~%d SCRAP", tgt->difficulty, tgt->scrap_reward);
            sr_draw_text_shadow(px, W, H, dbx + 10, dby + 26, dbuf, 0xFF888888, shadow);

            if (ui_button(px, W, H, dbx + 10, dby + dbh - 16, 60, 14, "YES",
                          0xFF112211, 0xFF223322, 0xFF44CC44)) {
                tgt->visited = true;
                g_starmap.current_node = ct;
                player_sector = tgt->difficulty;
                current_mission_is_boss = tgt->is_boss;
                g_starmap.active = false;
                g_starmap.confirm_active = false;
                hub_generate(&g_hub);
                g_hub.mission_available = true;
                app_state = STATE_SHIP_HUB;
                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "JUMPED TO %s", tgt->name);
                g_hub.hud_msg_timer = 90;
            }
            if (ui_button(px, W, H, dbx + dbw - 70, dby + dbh - 16, 60, 14, "NO",
                          0xFF221111, 0xFF332222, 0xFF882222)) {
                g_starmap.confirm_active = false;
            }
        }
    } else {
        /* Jump button — opens confirm dialog */
        if (cur->next_count > 0) {
            if (ui_button(px, W, H, W/2 - 30, H - 30, 60, 14, "JUMP",
                          0xFF111133, 0xFF222255, 0xFF333377)) {
                int next = cur->next[g_starmap.cursor];
                if (next >= 0 && next < g_starmap.node_count) {
                    g_starmap.confirm_active = true;
                    g_starmap.confirm_target = next;
                }
            }
        }
    }

    /* Back button */
    if (ui_button(px, W, H, W/2 - 30, H - 14, 60, 12, "BACK",
                  0xFF111122, 0xFF222244, 0xFF333366)) {
        g_starmap.active = false;
        g_starmap.confirm_active = false;
        app_state = STATE_SHIP_HUB;
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
                g_starmap.nodes[ct].visited = true;
                g_starmap.current_node = ct;
                player_sector = g_starmap.nodes[ct].difficulty;
                current_mission_is_boss = g_starmap.nodes[ct].is_boss;
                g_starmap.active = false;
                g_starmap.confirm_active = false;
                hub_generate(&g_hub);
                g_hub.mission_available = true;
                app_state = STATE_SHIP_HUB;
                snprintf(g_hub.hud_msg, sizeof(g_hub.hud_msg), "JUMPED TO %s", g_starmap.nodes[ct].name);
                g_hub.hud_msg_timer = 90;
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
static int deck_view_selected = -1; /* index of selected card for detail view, -1 = none */

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

    /* Title */
    char title[32];
    snprintf(title, sizeof(title), "DECK (%d CARDS)", g_player.persistent_deck_count);
    sr_draw_text_shadow(px, W, H, 10, 4, title, 0xFF00DDDD, shadow);

    /* Card grid using full card rendering */
    int cw = 50, ch = 66, pad = 6;
    int cols = 7;
    int startX = 10, startY = 14;

    for (int i = 0; i < g_player.persistent_deck_count; i++) {
        int col = i % cols;
        int row = i / cols;
        int cx = startX + col * (cw + pad);
        int cy = startY + row * (ch + pad);
        if (cy + ch > H - 20) break;

        bool sel = (i == deck_view_selected);
        combat_draw_card_content(px, W, H, cx, cy, cw, ch,
                                 g_player.persistent_deck[i], sel, shadow, -1);

        /* Click detection */
        if (ui_mouse_clicked &&
            ui_click_x >= cx && ui_click_x < cx + cw &&
            ui_click_y >= cy && ui_click_y < cy + ch) {
            deck_view_selected = (deck_view_selected == i) ? -1 : i;
        }
    }

    /* Detail overlay for selected card */
    if (deck_view_selected >= 0 && deck_view_selected < g_player.persistent_deck_count) {
        int card_type = g_player.persistent_deck[deck_view_selected];

        /* Centered large card */
        int dw = 100, dh = 140;
        int dx = (W - dw) / 2, dy = (H - dh) / 2 - 10;
        /* Dark backdrop */
        for (int ry = dy - 4; ry < dy + dh + 24 && ry < H; ry++)
            for (int rx = dx - 4; rx < dx + dw + 4 && rx < W; rx++)
                if (rx >= 0 && ry >= 0)
                    px[ry * W + rx] = 0xFF000000;
        combat_draw_card_content(px, W, H, dx, dy, dw, dh,
                                 card_type, true, shadow, -1);
        /* Description below card */
        const char *desc = card_description_text(card_type);
        sr_draw_text_wrap(px, W, H, dx, dy + dh + 4, desc,
                          dw, 8, 0xFFAAAAAA, shadow);
    }

    /* Close button */
    if (ui_button(px, W, H, W - 60, H - 18, 50, 14, "CLOSE",
                  0xFF111122, 0xFF222244, 0xFF333366)) {
        deck_view_active = false;
        deck_view_selected = -1;
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
        float ship_ox = -(enemy_d->w * DNG_CELL_SIZE) * 0.5f + (d->w * DNG_CELL_SIZE) * 0.5f;
        float hover = sinf((float)dng_time * esc->hover_speed) * esc->hover_amp;
        float ship_oy = esc->vertical + hover;
        float ship_oz = -esc->distance;
        static bool _rs_logged = false;
        if (!_rs_logged) {
            printf("_ship] w=%d h=%d ox=%.1f oz=%.1f\n",
                   enemy_d->w, enemy_d->h, ship_ox, ship_oz);
            _rs_logged = true;
        }
        sr_set_pixel_light_fn(NULL);
        draw_remote_ship_exterior(fb_ptr, &remote_mvp, enemy_d, ship_ox, ship_oy, ship_oz, true);
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

    /* Sector + Samples */
    char sec_buf[32];
    snprintf(sec_buf, sizeof(sec_buf), "SECTOR %d", player_sector + 1);
    sr_draw_text_shadow(px, W, H, W - 60, 4, sec_buf, 0xFF888888, shadow);

    if (mission_first_done) {
        char samp_buf[32];
        snprintf(samp_buf, sizeof(samp_buf), "SAMPLES %d/%d", player_samples, SAMPLES_REQUIRED);
        uint32_t samp_col = (player_samples >= SAMPLES_REQUIRED) ? 0xFF44CC44 : 0xFFCC8822;
        sr_draw_text_shadow(px, W, H, W - 78, 14, samp_buf, samp_col, shadow);
    }

    /* Mission objectives */
    {
        int oy = 50;
        if (mission_briefed && !mission_first_done && g_dlgd.loaded) {
            /* Initial prep flow: medbay, armory, board derelict */
            sr_draw_text_shadow(px, W, H, 4, oy, "OBJECTIVES:", 0xFFCC8822, shadow);
            oy += 10;
            bool obj_done[3] = { mission_medbay_done, mission_armory_done, false };
            for (int i = 0; i < g_dlgd.objective_count && i < 3; i++) {
                char obj_buf[DLGD_LINE_LEN + 8];
                bool done = obj_done[i];
                snprintf(obj_buf, sizeof(obj_buf), "[%c] %s", done ? 'X' : ' ', g_dlgd.objectives[i]);
                uint32_t col = done ? 0xFF448844 : 0xFFCCCCCC;
                sr_draw_text_shadow(px, W, H, 8, oy, obj_buf, col, shadow);
                oy += 10;
            }
        } else if (mission_first_done && !g_hub.mission_available) {
            /* Post-mission: head to warp */
            sr_draw_text_shadow(px, W, H, 4, oy, "OBJECTIVES:", 0xFFCC8822, shadow);
            oy += 10;
            sr_draw_text_shadow(px, W, H, 8, oy, "[ ] GO TO WARP", 0xFFCCCCCC, shadow);
        } else if (mission_first_done && g_hub.mission_available) {
            /* Under attack: board the enemy ship */
            sr_draw_text_shadow(px, W, H, 4, oy, "OBJECTIVES:", 0xFFCC8822, shadow);
            oy += 10;
            sr_draw_text_shadow(px, W, H, 8, oy, "[ ] BOARD ENEMY SHIP", 0xFFCCCCCC, shadow);
        }
    }

    /* Deck button (clickable) */
    char deck_buf[32];
    snprintf(deck_buf, sizeof(deck_buf), "DECK %d", g_player.persistent_deck_count);
    int deck_btn_y = mission_first_done ? 26 : 14;
    if (ui_button(px, W, H, W - 70, deck_btn_y, 66, 12, deck_buf,
                  0xFF1A1A2A, 0xFF222244, 0xFF333366)) {
        deck_view_active = true;
        deck_view_selected = -1;
    }

    /* Room/NPC interaction button — merged room name + action into one button */
    /* Skip when deck viewer is open (modal) or dialog is active */
    if (deck_view_active || g_dialog.active) return;

    int room_idx = hub_room_at_pos(g_hub.player.gx, g_hub.player.gy);
    int look_gx = g_hub.player.gx + dng_dir_dx[g_hub.player.dir];
    int look_gy = g_hub.player.gy + dng_dir_dz[g_hub.player.dir];
    int npc = hub_npc_at(look_gx, look_gy);

    if (npc >= 0) {
        /* NPC in front — show "TALK: NAME" button in room color */
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
            /* Only allow teleport if mission available AND prep done (or post-first-mission) */
            if (g_hub.mission_available && (mission_first_done || (mission_medbay_done && mission_armory_done)))
                action = DIALOG_ACTION_TELEPORT;
        } else if (rt == HUB_ROOM_BRIDGE) {
            btn_label = "BRIDGE";
            action = DIALOG_ACTION_STARMAP;
        } else if (rt == HUB_ROOM_SHOP) {
            btn_label = "ARMORY";
            /* Block armory until briefed and medbay done (or past first mission) */
            if (mission_first_done || (mission_briefed && mission_medbay_done))
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
                                snprintf(g_dialog.lines[0], DIALOG_LINE_LEN, "MEDICAL STATION ONLINE.");
                                g_dialog.line_count = 1; break;
                        }
                        g_dialog.pending_action = action;
                        g_dialog.active = true;
                    }
                }
            } else {
                /* Non-interactive rooms — just show room name as label */
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
