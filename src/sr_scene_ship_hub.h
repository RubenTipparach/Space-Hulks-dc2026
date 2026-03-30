/*  sr_scene_ship_hub.h — Player's own ship hub scene.
 *  Walk around your ship, talk to crew, use teleporter, shop, bridge/starmap.
 *  Single-TU header-only. Depends on sr_dungeon.h, sr_combat.h, sr_app.h. */
#ifndef SR_SCENE_SHIP_HUB_H
#define SR_SCENE_SHIP_HUB_H

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

#define SHOP_MAX_CARDS 6

typedef struct {
    int cards[SHOP_MAX_CARDS];   /* card types for sale */
    int prices[SHOP_MAX_CARDS];  /* scrap cost */
    int count;
    int cursor;
    int mode;            /* 0=buy, 1=trash */
    int trash_cursor;    /* cursor into player deck for trashing */
    int trash_count;     /* number of cards trashed this visit */
    bool active;
} shop_state;

static shop_state g_shop;

/* ── Dialog state ───────────────────────────────────────────────── */

#define DIALOG_MAX_LINES 4
#define DIALOG_LINE_LEN 48

enum {
    DIALOG_ACTION_NONE,
    DIALOG_ACTION_STARMAP,
    DIALOG_ACTION_SHOP,
    DIALOG_ACTION_TELEPORT,
    DIALOG_ACTION_HEAL,
};

typedef struct {
    char lines[DIALOG_MAX_LINES][DIALOG_LINE_LEN];
    int line_count;
    char speaker[24];
    bool active;
    int pending_action;    /* action to trigger on dismiss */
} dialog_state;

static dialog_state g_dialog;

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
    d->w = DNG_GRID_W;
    d->h = DNG_GRID_H;
    d->has_up = false;
    d->has_down = false;

    /* Fill with walls */
    for (int y = 1; y <= d->h; y++)
        for (int x = 1; x <= d->w; x++)
            d->map[y][x] = 1;

    int mid_y = d->h / 2;

    /* Central corridor (2 tiles wide) */
    for (int x = 3; x <= 18; x++) {
        d->map[mid_y][x] = 0;
        d->map[mid_y + 1][x] = 0;
    }

    /* Room definitions: {x, y, w, h, type} */
    struct { int x, y, w, h, type; } room_defs[] = {
        /* Bridge at the front (right side) */
        { 15, mid_y - 5, 4, 4, HUB_ROOM_BRIDGE },
        /* Teleporter at the back (left side) */
        { 3, mid_y - 5, 4, 4, HUB_ROOM_TELEPORTER },
        /* Shop/Armory upper-middle */
        { 9, mid_y - 5, 4, 4, HUB_ROOM_SHOP },
        /* Quarters below-left */
        { 5, mid_y + 3, 4, 4, HUB_ROOM_QUARTERS },
        /* Medbay below-right */
        { 12, mid_y + 3, 4, 4, HUB_ROOM_MEDBAY },
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

        /* Connect room to corridor */
        int conn_x = rx + rw / 2;
        if (ry < mid_y) {
            for (int y = ry + rh; y <= mid_y; y++)
                if (y >= 1 && y <= d->h && conn_x >= 1 && conn_x <= d->w)
                    d->map[y][conn_x] = 0;
        } else {
            for (int y = mid_y + 1; y < ry; y++)
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
        .gx = 6, .gy = mid_y + 5
    };
    /* Medic */
    hub->crew[4] = (hub_npc){
        .name = "DR VASQUEZ", .room = 4, .dialog_id = 4, .active = true,
        .gx = 13, .gy = mid_y + 5
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

    hub->mission_available = true;
    hub->initialized = true;
}

/* ── Hub lighting (60% ambient, depth-based, no point lights) ───── */

static float hub_fog_vertex_intensity(float wx, float wy, float wz) {
    dng_player *p = &g_hub.player;
    float dx = p->x - wx;
    float dy = p->y - wy;
    float dz = p->z - wz;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz) / DNG_CELL_SIZE;

    /* 60% ambient base + depth falloff */
    float base = 0.6f;
    if (dist > 1.0f) {
        float fade = 1.0f - (dist - 1.0f) / 8.0f;
        if (fade < 0.0f) fade = 0.0f;
        base += 0.3f * fade;
    } else {
        base += 0.3f;
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

static const char *crew_dialogs[][3] = {
    /* CPT HARDEN (bridge) */
    { "WE NEED TO KEEP MOVING.", "HEAD TO THE BRIDGE WHEN", "YOU'RE READY TO JUMP." },
    /* SGT REYES (teleporter) */
    { "TELEPORTER IS PRIMED.", "STEP ON THE PAD WHEN", "YOU'RE READY FOR ACTION." },
    /* QM CHEN (shop) */
    { "GOT SOME NEW GEAR.", "TAKE A LOOK AT WHAT", "I'VE SCROUNGED UP." },
    /* PVT KOWALSKI (quarters) */
    { "ANOTHER DAY IN SPACE.", "AT LEAST WE'RE STILL", "BREATHING, RIGHT?" },
    /* DR VASQUEZ (medbay) */
    { "YOU LOOK ROUGHED UP.", "LET ME PATCH YOU UP", "BEFORE YOUR NEXT RUN." },
};

static void hub_start_dialog(int npc_idx, int action) {
    if (npc_idx < 0 || npc_idx >= g_hub.crew_count) return;
    hub_npc *npc = &g_hub.crew[npc_idx];
    memset(&g_dialog, 0, sizeof(g_dialog));
    snprintf(g_dialog.speaker, sizeof(g_dialog.speaker), "%s", npc->name);

    int did = npc->dialog_id;
    if (did < 0 || did > 4) did = 0;
    g_dialog.line_count = 3;
    for (int i = 0; i < 3; i++)
        snprintf(g_dialog.lines[i], DIALOG_LINE_LEN, "%s", crew_dialogs[did][i]);

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

static void draw_dialog(uint32_t *px, int W, int H) {
    if (!g_dialog.active) return;
    uint32_t shadow = 0xFF000000;

    /* Dialog box at bottom */
    int bx = 40, by = H - 70, bw = W - 80, bh = 60;
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

    /* Dialog lines */
    for (int i = 0; i < g_dialog.line_count; i++)
        sr_draw_text_shadow(px, W, H, bx + 4, by + 16 + i * 10,
                            g_dialog.lines[i], 0xFFCCCCCC, shadow);

    const char *dismiss_label = g_dialog.pending_action != DIALOG_ACTION_NONE
        ? "CONTINUE" : "CLOSE";
    if (ui_button(px, W, H, bx + bw - 80, by + bh - 14, 70, 12,
                  dismiss_label, 0xFF1A1A33, 0xFF222255, 0xFF44CC44)) {
        /* Mark for dismiss — handled by handle_screen_tap via tap flow */
    }
}

/* ── Shop system ────────────────────────────────────────────────── */

static void shop_generate(shop_state *shop) {
    memset(shop, 0, sizeof(*shop));
    shop->count = 4 + dng_rng_int(3); /* 4-6 cards */
    if (shop->count > SHOP_MAX_CARDS) shop->count = SHOP_MAX_CARDS;

    int droppable_start = CARD_OVERCHARGE;
    int droppable_count = CARD_TYPE_COUNT - droppable_start;
    for (int i = 0; i < shop->count; i++) {
        shop->cards[i] = droppable_start + dng_rng_int(droppable_count);
        /* Price: 15-40 scrap based on card energy cost */
        shop->prices[i] = 15 + card_energy_cost[shop->cards[i]] * 10 + dng_rng_int(6);
    }
    shop->cursor = 0;
    shop->mode = 0;
    shop->trash_cursor = 0;
    shop->active = true;
}

static void draw_shop(uint32_t *px, int W, int H) {
    if (!g_shop.active) return;
    uint32_t shadow = 0xFF000000;

    /* Background */
    for (int i = 0; i < W * H; i++) px[i] = 0xFF0D0D15;

    sr_draw_text_shadow(px, W, H, W/2 - 30, 10, "ARMORY", 0xFFCC8822, shadow);

    char scrap_buf[32];
    snprintf(scrap_buf, sizeof(scrap_buf), "SCRAP: %d", player_scrap);
    sr_draw_text_shadow(px, W, H, W - 80, 10, scrap_buf, 0xFF00DDDD, shadow);

    /* Mode tab buttons */
    if (ui_button(px, W, H, 30, 24, 90, 14, "BUY CARDS",
                  g_shop.mode == 0 ? 0xFF1A1A33 : 0xFF111122,
                  0xFF222255, 0xFF333366))
        g_shop.mode = 0;
    if (ui_button(px, W, H, 130, 24, 100, 14, "TRASH CARDS",
                  g_shop.mode == 1 ? 0xFF1A1A33 : 0xFF111122,
                  0xFF222255, 0xFF333366))
        g_shop.mode = 1;

    if (g_shop.mode == 0) {
        /* Buy mode */
        for (int i = 0; i < g_shop.count; i++) {
            int cy = 44 + i * 30;
            int rx0 = 60, rw = W - 120, rh = 26;

            bool hovered = false;
            bool row_clicked = ui_row_hover(rx0, cy, rw, rh, &hovered);
            bool sel = (g_shop.cursor == i);

            /* Hover selects */
            if (hovered) g_shop.cursor = i;

            uint32_t bg = sel ? (hovered ? 0xFF2A2A55 : 0xFF222244) : (hovered ? 0xFF1A1A33 : 0xFF111122);

            for (int ry = cy; ry < cy + rh && ry < H; ry++)
                for (int rx = rx0; rx < rx0 + rw && rx < W; rx++)
                    if (ry >= 0 && rx >= 0) px[ry * W + rx] = bg;

            /* Border on selected/hovered */
            if (sel || hovered) {
                uint32_t bdr = hovered ? 0xFF6666CC : 0xFF4444AA;
                for (int rx = rx0; rx < rx0 + rw; rx++) {
                    if (cy >= 0 && cy < H) px[cy * W + rx] = bdr;
                    if (cy+rh-1 >= 0 && cy+rh-1 < H) px[(cy+rh-1) * W + rx] = bdr;
                }
            }

            int card_type = g_shop.cards[i];
            uint32_t ccol = card_colors[card_type];
            sr_draw_text_shadow(px, W, H, 66, cy + 4, card_names[card_type], ccol, shadow);

            char cost_buf[16];
            snprintf(cost_buf, sizeof(cost_buf), "%dE", card_energy_cost[card_type]);
            sr_draw_text_shadow(px, W, H, 170, cy + 4, cost_buf, 0xFF888888, shadow);

            char price_buf[16];
            snprintf(price_buf, sizeof(price_buf), "%d SCRAP", g_shop.prices[i]);
            uint32_t pcol = player_scrap >= g_shop.prices[i] ? 0xFF22CC22 : 0xFF882222;
            sr_draw_text_shadow(px, W, H, W - 140, cy + 4, price_buf, pcol, shadow);

            /* Buy button on selected row */
            if (sel) {
                if (ui_button(px, W, H, 66, cy + 14, 60, 12, "BUY",
                              0xFF1A1A33, 0xFF222255, 0xFF44CC44)) {
                    /* Trigger buy */
                    if (player_scrap >= g_shop.prices[i] &&
                        g_player.persistent_deck_count < COMBAT_DECK_MAX) {
                        player_scrap -= g_shop.prices[i];
                        g_player.persistent_deck[g_player.persistent_deck_count++] = g_shop.cards[i];
                        for (int j = i; j < g_shop.count - 1; j++) {
                            g_shop.cards[j] = g_shop.cards[j + 1];
                            g_shop.prices[j] = g_shop.prices[j + 1];
                        }
                        g_shop.count--;
                        if (g_shop.cursor >= g_shop.count && g_shop.count > 0)
                            g_shop.cursor = g_shop.count - 1;
                    }
                }
            }

            /* Click row to buy (same as clicking buy button) */
            if (row_clicked && sel) {
                if (player_scrap >= g_shop.prices[i] &&
                    g_player.persistent_deck_count < COMBAT_DECK_MAX) {
                    player_scrap -= g_shop.prices[i];
                    g_player.persistent_deck[g_player.persistent_deck_count++] = g_shop.cards[i];
                    for (int j = i; j < g_shop.count - 1; j++) {
                        g_shop.cards[j] = g_shop.cards[j + 1];
                        g_shop.prices[j] = g_shop.prices[j + 1];
                    }
                    g_shop.count--;
                    if (g_shop.cursor >= g_shop.count && g_shop.count > 0)
                        g_shop.cursor = g_shop.count - 1;
                    break; /* list shifted, stop iterating */
                }
            }
        }
    } else {
        /* Trash mode - show player's deck */
        int trash_cost = 30 + g_shop.trash_count * 5;
        char trash_hdr[48];
        snprintf(trash_hdr, sizeof(trash_hdr), "YOUR DECK:  TRASH COST: %d SCRAP", trash_cost);
        sr_draw_text_shadow(px, W, H, 60, 44, trash_hdr, 0xFFCCCCCC, shadow);
        int cols = 4;
        for (int i = 0; i < g_player.persistent_deck_count; i++) {
            int col = i % cols;
            int row = i / cols;
            int cx = 60 + col * 100;
            int cy = 58 + row * 24;

            bool hovered = false;
            ui_row_hover(cx, cy, 90, 20, &hovered);
            if (hovered) g_shop.trash_cursor = i;
            bool sel = (g_shop.trash_cursor == i);

            int card_type = g_player.persistent_deck[i];
            uint32_t ccol = hovered ? 0xFF00FFFF : (sel ? 0xFF00DDDD : card_colors[card_type]);
            sr_draw_text_shadow(px, W, H, cx, cy, card_names[card_type], ccol, shadow);
            if (sel) {
                bool can_trash = g_player.persistent_deck_count > 5 && player_scrap >= trash_cost;
                char tbuf[24];
                snprintf(tbuf, sizeof(tbuf), "TRASH (%d)", trash_cost);
                if (ui_button(px, W, H, cx, cy + 10, 70, 12, tbuf,
                              can_trash ? 0xFF331111 : 0xFF222222,
                              can_trash ? 0xFF442222 : 0xFF333333,
                              can_trash ? 0xFFCC2222 : 0xFF333333)) {
                    if (can_trash) {
                        player_scrap -= trash_cost;
                        g_shop.trash_count++;
                        for (int j = i; j < g_player.persistent_deck_count - 1; j++)
                            g_player.persistent_deck[j] = g_player.persistent_deck[j + 1];
                        g_player.persistent_deck_count--;
                        if (g_shop.trash_cursor >= g_player.persistent_deck_count)
                            g_shop.trash_cursor = g_player.persistent_deck_count - 1;
                    }
                }
            }
        }
        sr_draw_text_shadow(px, W, H, 60, H - 30,
            "MIN 5 CARDS", 0xFF888888, shadow);
    }

    /* Leave button */
    if (ui_button(px, W, H, W/2 - 40, H - 18, 80, 16, "LEAVE",
                  0xFF111122, 0xFF222244, 0xFF333366)) {
        g_shop.active = false;
        app_state = STATE_SHIP_HUB;
    }
}

static void shop_handle_key(int key_code) {
    if (!g_shop.active) return;

    /* Tab between buy/trash with 1/2 keys */
    if (key_code == SAPP_KEYCODE_1) { g_shop.mode = 0; return; }
    if (key_code == SAPP_KEYCODE_2) { g_shop.mode = 1; return; }

    if (g_shop.mode == 0) {
        /* Buy mode */
        if (key_code == SAPP_KEYCODE_UP || key_code == SAPP_KEYCODE_W) {
            g_shop.cursor--;
            if (g_shop.cursor < 0) g_shop.cursor = g_shop.count - 1;
        }
        if (key_code == SAPP_KEYCODE_DOWN || key_code == SAPP_KEYCODE_S) {
            g_shop.cursor++;
            if (g_shop.cursor >= g_shop.count) g_shop.cursor = 0;
        }
        if (key_code == SAPP_KEYCODE_ENTER || key_code == SAPP_KEYCODE_SPACE) {
            int idx = g_shop.cursor;
            if (idx >= 0 && idx < g_shop.count &&
                player_scrap >= g_shop.prices[idx] &&
                g_player.persistent_deck_count < COMBAT_DECK_MAX) {
                player_scrap -= g_shop.prices[idx];
                g_player.persistent_deck[g_player.persistent_deck_count++] = g_shop.cards[idx];
                /* Remove from shop */
                for (int i = idx; i < g_shop.count - 1; i++) {
                    g_shop.cards[i] = g_shop.cards[i + 1];
                    g_shop.prices[i] = g_shop.prices[i + 1];
                }
                g_shop.count--;
                if (g_shop.cursor >= g_shop.count && g_shop.count > 0)
                    g_shop.cursor = g_shop.count - 1;
            }
        }
    } else {
        /* Trash mode */
        if (key_code == SAPP_KEYCODE_LEFT || key_code == SAPP_KEYCODE_A) {
            g_shop.trash_cursor--;
            if (g_shop.trash_cursor < 0)
                g_shop.trash_cursor = g_player.persistent_deck_count - 1;
        }
        if (key_code == SAPP_KEYCODE_RIGHT || key_code == SAPP_KEYCODE_D) {
            g_shop.trash_cursor++;
            if (g_shop.trash_cursor >= g_player.persistent_deck_count)
                g_shop.trash_cursor = 0;
        }
        if (key_code == SAPP_KEYCODE_UP || key_code == SAPP_KEYCODE_W) {
            g_shop.trash_cursor -= 4;
            if (g_shop.trash_cursor < 0) g_shop.trash_cursor = 0;
        }
        if (key_code == SAPP_KEYCODE_DOWN || key_code == SAPP_KEYCODE_S) {
            g_shop.trash_cursor += 4;
            if (g_shop.trash_cursor >= g_player.persistent_deck_count)
                g_shop.trash_cursor = g_player.persistent_deck_count - 1;
        }
        if (key_code == SAPP_KEYCODE_ENTER || key_code == SAPP_KEYCODE_SPACE) {
            int trash_cost = 30 + g_shop.trash_count * 5;
            if (g_player.persistent_deck_count > 5 && player_scrap >= trash_cost) {
                player_scrap -= trash_cost;
                g_shop.trash_count++;
                int idx = g_shop.trash_cursor;
                for (int i = idx; i < g_player.persistent_deck_count - 1; i++)
                    g_player.persistent_deck[i] = g_player.persistent_deck[i + 1];
                g_player.persistent_deck_count--;
                if (g_shop.trash_cursor >= g_player.persistent_deck_count)
                    g_shop.trash_cursor = g_player.persistent_deck_count - 1;
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

static void starmap_generate(starmap_state *sm, int start_sector) {
    memset(sm, 0, sizeof(*sm));

    /* Generate a branching path of 3 columns with 2-3 nodes each */
    int col_count = 4; /* columns of nodes */
    int node_idx = 0;

    /* Starting node (current position) */
    sm->nodes[0].difficulty = start_sector;
    sm->nodes[0].scrap_reward = 0;
    sm->nodes[0].visited = true;
    sm->nodes[0].x = 40;
    sm->nodes[0].y = FB_HEIGHT / 2;
    snprintf(sm->nodes[0].name, 24, "SECTOR %s", sector_names[start_sector % NUM_SECTOR_NAMES]);
    node_idx = 1;

    int prev_start = 0, prev_count = 1;

    for (int col = 1; col < col_count; col++) {
        int nodes_in_col = 2 + dng_rng_int(2); /* 2-3 nodes per column */
        if (nodes_in_col > 3) nodes_in_col = 3;
        int col_start = node_idx;

        for (int n = 0; n < nodes_in_col && node_idx < STARMAP_MAX_NODES; n++) {
            starmap_node *nd = &sm->nodes[node_idx];
            int diff = start_sector + col;
            nd->difficulty = diff;
            nd->scrap_reward = 20 + diff * 5 + dng_rng_int(10);
            nd->visited = false;
            nd->x = 40 + col * (FB_WIDTH - 80) / (col_count - 1);
            nd->y = 40 + n * (FB_HEIGHT - 80) / (nodes_in_col > 1 ? nodes_in_col - 1 : 1);

            int name_idx = (start_sector + col * 3 + n) % NUM_SECTOR_NAMES;
            snprintf(nd->name, 24, "SECTOR %s-%d", sector_names[name_idx], diff);

            nd->next_count = 0;
            for (int i = 0; i < STARMAP_MAX_CHOICES; i++) nd->next[i] = -1;

            node_idx++;
        }

        /* Connect previous column to this column */
        for (int p = prev_start; p < prev_start + prev_count; p++) {
            /* Each prev node connects to 1-2 nodes in this column */
            int conns = 1 + dng_rng_int(2);
            if (conns > nodes_in_col) conns = nodes_in_col;
            for (int c = 0; c < conns; c++) {
                int target = col_start + dng_rng_int(nodes_in_col);
                /* Avoid duplicate connections */
                bool dup = false;
                for (int e = 0; e < sm->nodes[p].next_count; e++)
                    if (sm->nodes[p].next[e] == target) { dup = true; break; }
                if (!dup && sm->nodes[p].next_count < STARMAP_MAX_CHOICES)
                    sm->nodes[p].next[sm->nodes[p].next_count++] = target;
            }
        }

        /* Ensure every node in this column is reachable from at least one prev */
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

        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++) {
                if (dx*dx + dy*dy > 5) continue;
                int rx = nd->x + dx, ry = nd->y + dy;
                if (rx >= 0 && rx < W && ry >= 0 && ry < H)
                    px[ry * W + rx] = is_hovered ? 0xFF00FFFF : col;
            }

        /* Label */
        if (i == g_starmap.current_node || selectable) {
            sr_draw_text_shadow(px, W, H, nd->x - 20, nd->y + 6,
                                nd->name, col, shadow);
            if (selectable && nd->scrap_reward > 0) {
                char rbuf[24];
                snprintf(rbuf, sizeof(rbuf), "D%d  ~%d SCRAP", nd->difficulty, nd->scrap_reward);
                sr_draw_text_shadow(px, W, H, nd->x - 20, nd->y + 16,
                                    rbuf, 0xFF888888, shadow);
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

/* ── Hub scene drawing (reuses dungeon renderer with hub lighting) ── */

static void hub_draw_scene(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = &g_hub.dungeon;
    dng_player *p = &g_hub.player;

    /* Override the lighting for hub: set high ambient, disable pixel lighting */
    float save_ambient = dng_cfg.ambient_brightness;
    float save_torch = dng_cfg.light_brightness;
    dng_cfg.ambient_brightness = 0.6f;
    dng_cfg.light_brightness = 0.4f;

    /* Temporarily point dng_state at hub dungeon for rendering */
    sr_dungeon *save_dungeon = dng_state.dungeon;
    dng_player save_player = dng_state.player;
    dng_state.dungeon = d;
    dng_state.player = *p;

    /* Use fog mode (mode 1) for hub — hub_mode flag makes it fully lit */
    int save_light_mode = dng_light_mode;
    dng_light_mode = 1;
    dng_sprites_unlit = true;

    sr_mat4 vp;
    draw_dungeon_scene(fb_ptr, &vp);

    /* Restore */
    dng_state.dungeon = save_dungeon;
    dng_state.player = save_player;
    dng_light_mode = save_light_mode;
    dng_sprites_unlit = false;
    dng_cfg.ambient_brightness = save_ambient;
    dng_cfg.light_brightness = save_torch;
}

/* ── Hub HUD overlay ────────────────────────────────────────────── */

static void hub_draw_hud(uint32_t *px, int W, int H) {
    uint32_t shadow = 0xFF000000;

    /* Ship name */
    sr_draw_text_shadow(px, W, H, 4, 4, "ISS ENDURANCE", 0xFF22CCEE, shadow);

    /* Player HP */
    char hp_buf[32];
    snprintf(hp_buf, sizeof(hp_buf), "HP %d/%d", g_player.hp, g_player.hp_max);
    sr_draw_text_shadow(px, W, H, 4, 14, hp_buf, 0xFF22CC22, shadow);

    /* Scrap */
    char scrap_buf[32];
    snprintf(scrap_buf, sizeof(scrap_buf), "SCRAP %d", player_scrap);
    sr_draw_text_shadow(px, W, H, 4, 24, scrap_buf, 0xFF00DDDD, shadow);

    /* Sector */
    char sec_buf[32];
    snprintf(sec_buf, sizeof(sec_buf), "SECTOR %d", player_sector + 1);
    sr_draw_text_shadow(px, W, H, W - 60, 4, sec_buf, 0xFF888888, shadow);

    /* Deck size */
    char deck_buf[32];
    snprintf(deck_buf, sizeof(deck_buf), "DECK: %d CARDS", g_player.persistent_deck_count);
    sr_draw_text_shadow(px, W, H, W - 80, 14, deck_buf, 0xFF888888, shadow);

    /* Show what room you're in */
    int room_idx = hub_room_at_pos(g_hub.player.gx, g_hub.player.gy);
    if (room_idx >= 0 && room_idx < (int)(sizeof(hub_room_names)/sizeof(hub_room_names[0]))) {
        int rt = g_hub.room_types[room_idx];
        const char *rn = hub_room_names[rt];
        int rnlen = 0; for (const char *c = rn; *c; c++) rnlen++;
        sr_draw_text_shadow(px, W, H, W/2 - rnlen * 3, H - 14,
                            rn, hub_room_colors[rt], shadow);
    }

    /* Show NPC name when facing one */
    int look_gx = g_hub.player.gx + dng_dir_dx[g_hub.player.dir];
    int look_gy = g_hub.player.gy + dng_dir_dz[g_hub.player.dir];
    int npc = hub_npc_at(look_gx, look_gy);
    if (npc >= 0) {
        const char *nn = g_hub.crew[npc].name;
        int nnlen = 0; for (const char *c = nn; *c; c++) nnlen++;
        sr_draw_text_shadow(px, W, H, W/2 - nnlen * 3, H - 24,
                            nn, 0xFF00DDDD, shadow);
        sr_draw_text_shadow(px, W, H, W/2 - 24, H - 34,
                            "[F] TALK", 0xFF888888, shadow);
    } else if (room_idx >= 0) {
        int rt = g_hub.room_types[room_idx];
        const char *action_label = NULL;
        uint32_t action_col = 0xFF888888;
        if (rt == HUB_ROOM_TELEPORTER && g_hub.mission_available) {
            action_label = "[F] TELEPORTER"; action_col = 0xFF22CC44;
        } else if (rt == HUB_ROOM_BRIDGE) {
            action_label = "[F] STAR MAP"; action_col = 0xFF22CCEE;
        } else if (rt == HUB_ROOM_SHOP) {
            action_label = "[F] ARMORY"; action_col = 0xFFCC8822;
        } else if (rt == HUB_ROOM_MEDBAY) {
            action_label = "[F] MEDBAY"; action_col = 0xFF44CC44;
        }
        if (action_label) {
            int allen = 0; for (const char *c = action_label; *c; c++) allen++;
            sr_draw_text_shadow(px, W, H, W/2 - allen * 3, H - 34,
                                action_label, action_col, shadow);
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

/* ── Hub minimap ────────────────────────────────────────────────── */

static void hub_draw_minimap(sr_framebuffer *fb_ptr) {
    sr_dungeon *d = &g_hub.dungeon;
    dng_player *p = &g_hub.player;
    int scale = 2;
    int mx = FB_WIDTH - d->w * scale - 4;
    int my = 30;
    uint32_t *px = fb_ptr->color;

    for (int y = 1; y <= d->h; y++) {
        for (int x = 1; x <= d->w; x++) {
            int px0 = mx + (x - 1) * scale;
            int py0 = my + (y - 1) * scale;
            uint32_t col;
            if (d->map[y][x] == 1) col = 0xFF222222;
            else {
                int ri = hub_room_at_pos(x, y);
                if (ri >= 0) col = hub_room_colors[g_hub.room_types[ri]];
                else col = 0xFF444444;
                /* Dim it */
                int r = ((col >> 0) & 0xFF) / 3;
                int g = ((col >> 8) & 0xFF) / 3;
                int b = ((col >> 16) & 0xFF) / 3;
                col = 0xFF000000 | (b << 16) | (g << 8) | r;
            }
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++) {
                    int rx = px0 + dx, ry = py0 + dy;
                    if (rx >= 0 && rx < fb_ptr->width && ry >= 0 && ry < fb_ptr->height)
                        px[ry * fb_ptr->width + rx] = col;
                }
        }
    }

    /* Player dot */
    int pdx = mx + (p->gx - 1) * scale;
    int pdy = my + (p->gy - 1) * scale;
    for (int dy = 0; dy < scale; dy++)
        for (int dx = 0; dx < scale; dx++) {
            int rx = pdx + dx, ry = pdy + dy;
            if (rx >= 0 && rx < fb_ptr->width && ry >= 0 && ry < fb_ptr->height)
                px[ry * fb_ptr->width + rx] = 0xFF00FF00;
        }
}

#endif /* SR_SCENE_SHIP_HUB_H */
