#ifndef SR_LEVEL_LOADER_H
#define SR_LEVEL_LOADER_H

/*  Level loader for Space Hulks.
 *
 *  Reads JSON level files (produced by the C# level editor) and populates
 *  sr_dungeon floors and ship_state structs.
 *
 *  JSON format matches LevelSerializer.cs output.
 */

#include "sr_json.h"

/* ── Room/enemy type name tables for enum parsing ──────────────── */

static const char *lvl_room_names[] = {
    "Corridor", "Bridge", "Medbay", "Weapons", "Engines",
    "Reactor", "Shields", "Cargo", "Barracks", "Teleporter"
};
#define LVL_ROOM_NAME_COUNT 10

static const char *lvl_enemy_names[] = {
    "None", "Lurker", "Brute", "Spitter", "Hiveguard"
};
#define LVL_ENEMY_NAME_COUNT 5

static const char *lvl_rank_names[] = {
    "Ensign", "Lieutenant", "Commander", "Captain"
};
#define LVL_RANK_NAME_COUNT 4

static const char *lvl_mission_names[] = {
    "DestroyShip", "CaptureOfficer", "SabotageReactor", "RetrieveData", "Rescue"
};
#define LVL_MISSION_NAME_COUNT 5

/* ── File I/O helper ───────────────────────────────────────────── */

static char *lvl_read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static bool lvl_file_exists(const char *path) {
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return true; }
    return false;
}

/* ── Load a floor from JSON into sr_dungeon ────────────────────── */

static bool lvl_load_floor(sr_dungeon *d, const sr_json *j, int floor_token) {
    if (floor_token < 0) return false;
    memset(d, 0, sizeof(*d));

    d->w = sr_json_int(j, sr_json_find(j, floor_token, "width"), 20);
    d->h = sr_json_int(j, sr_json_find(j, floor_token, "height"), 20);
    if (d->w > DNG_GRID_W) d->w = DNG_GRID_W;
    if (d->h > DNG_GRID_H) d->h = DNG_GRID_H;

    /* Fill with walls first */
    memset(d->map, 1, sizeof(d->map));

    /* Parse map array (jagged: map[y][x], 0-indexed in JSON, 1-indexed in struct) */
    int map_arr = sr_json_find(j, floor_token, "map");
    if (map_arr >= 0) {
        int rows = sr_json_array_len(j, map_arr);
        for (int y = 0; y < rows && y < d->h; y++) {
            int row = sr_json_array_get(j, map_arr, y);
            if (row < 0) continue;
            int cols = sr_json_array_len(j, row);
            for (int x = 0; x < cols && x < d->w; x++) {
                int cell = sr_json_array_get(j, row, x);
                d->map[y + 1][x + 1] = (uint8_t)sr_json_int(j, cell, 1);
            }
        }
    }

    /* Spawn */
    d->spawn_gx = sr_json_int(j, sr_json_find(j, floor_token, "spawnGX"), 3);
    d->spawn_gy = sr_json_int(j, sr_json_find(j, floor_token, "spawnGY"), 10);
    d->spawn_dir = sr_json_int(j, sr_json_find(j, floor_token, "spawnDir"), 1);

    /* Stairs up */
    d->has_up = sr_json_bool(j, sr_json_find(j, floor_token, "hasUp"), false);
    d->stairs_gx = sr_json_int(j, sr_json_find(j, floor_token, "stairsGX"), -1);
    d->stairs_gy = sr_json_int(j, sr_json_find(j, floor_token, "stairsGY"), -1);
    d->stairs_dir = sr_json_int(j, sr_json_find(j, floor_token, "stairsDir"), 1);

    /* Stairs down */
    d->has_down = sr_json_bool(j, sr_json_find(j, floor_token, "hasDown"), false);
    d->down_gx = sr_json_int(j, sr_json_find(j, floor_token, "downGX"), -1);
    d->down_gy = sr_json_int(j, sr_json_find(j, floor_token, "downGY"), -1);
    d->down_dir = sr_json_int(j, sr_json_find(j, floor_token, "downDir"), 3);

    /* Rooms */
    int rooms_arr = sr_json_find(j, floor_token, "rooms");
    if (rooms_arr >= 0) {
        d->room_count = sr_json_array_len(j, rooms_arr);
        if (d->room_count > DNG_MAX_ROOMS) d->room_count = DNG_MAX_ROOMS;
        for (int r = 0; r < d->room_count; r++) {
            int room = sr_json_array_get(j, rooms_arr, r);
            if (room < 0) continue;
            d->room_x[r] = sr_json_int(j, sr_json_find(j, room, "x"), 0);
            d->room_y[r] = sr_json_int(j, sr_json_find(j, room, "y"), 0);
            d->room_w[r] = sr_json_int(j, sr_json_find(j, room, "width"), 3);
            d->room_h[r] = sr_json_int(j, sr_json_find(j, room, "height"), 3);
            d->room_cx[r] = d->room_x[r] + d->room_w[r] / 2;
            d->room_cy[r] = d->room_y[r] + d->room_h[r] / 2;
            d->room_light_on[r] = sr_json_bool(j, sr_json_find(j, room, "lightOn"), true);
            d->room_ship_idx[r] = sr_json_int(j, sr_json_find(j, room, "shipIdx"), -1);
        }
    }

    /* Enemies */
    int enemies_arr = sr_json_find(j, floor_token, "enemies");
    if (enemies_arr >= 0) {
        int count = sr_json_array_len(j, enemies_arr);
        for (int i = 0; i < count; i++) {
            int en = sr_json_array_get(j, enemies_arr, i);
            if (en < 0) continue;
            int gx = sr_json_int(j, sr_json_find(j, en, "gX"), 0);
            int gy = sr_json_int(j, sr_json_find(j, en, "gY"), 0);
            if (gx < 1 || gx > d->w || gy < 1 || gy > d->h) continue;
            int etype = sr_json_enum(j, sr_json_find(j, en, "enemyType"),
                                     lvl_enemy_names, LVL_ENEMY_NAME_COUNT, 1);
            d->aliens[gy][gx] = (uint8_t)(etype); /* stored as enum value directly */
            sr_json_str(j, sr_json_find(j, en, "name"),
                        d->alien_names[gy][gx], 16);
        }
    }

    /* Consoles */
    int consoles_arr = sr_json_find(j, floor_token, "consoles");
    if (consoles_arr >= 0) {
        int count = sr_json_array_len(j, consoles_arr);
        for (int i = 0; i < count; i++) {
            int con = sr_json_array_get(j, consoles_arr, i);
            if (con < 0) continue;
            int gx = sr_json_int(j, sr_json_find(j, con, "gX"), 0);
            int gy = sr_json_int(j, sr_json_find(j, con, "gY"), 0);
            if (gx < 1 || gx > d->w || gy < 1 || gy > d->h) continue;
            int rtype = sr_json_enum(j, sr_json_find(j, con, "roomType"),
                                     lvl_room_names, LVL_ROOM_NAME_COUNT, 0);
            d->consoles[gy][gx] = (uint8_t)rtype;
        }
    }

    /* Windows */
    static const char *lvl_dir_names[] = { "North", "South", "East", "West" };
    static const uint8_t lvl_dir_bits[] = { DNG_WIN_N, DNG_WIN_S, DNG_WIN_E, DNG_WIN_W };
    #define LVL_DIR_NAME_COUNT 4
    int windows_arr = sr_json_find(j, floor_token, "windows");
    if (windows_arr >= 0) {
        int wcount = sr_json_array_len(j, windows_arr);
        for (int i = 0; i < wcount; i++) {
            int wt = sr_json_array_get(j, windows_arr, i);
            if (wt < 0) continue;
            int gx = sr_json_int(j, sr_json_find(j, wt, "gX"), 0);
            int gy = sr_json_int(j, sr_json_find(j, wt, "gY"), 0);
            if (gx < 1 || gx > d->w || gy < 1 || gy > d->h) continue;
            int dir = sr_json_enum(j, sr_json_find(j, wt, "dir"), lvl_dir_names, LVL_DIR_NAME_COUNT, 0);
            if (dir >= 0 && dir < LVL_DIR_NAME_COUNT)
                d->win_faces[gy][gx] |= lvl_dir_bits[dir];
        }
    }

    return true;
}

/* ── Load ship state from JSON level ───────────────────────────── */

static bool lvl_load_ship(ship_state *ship, const sr_json *j, int root) {
    if (root < 0) return false;
    memset(ship, 0, sizeof(*ship));

    sr_json_str(j, sr_json_find(j, root, "shipName"), ship->name, 32);
    ship->hull_hp = sr_json_int(j, sr_json_find(j, root, "hullHp"), 30);
    ship->hull_hp_max = sr_json_int(j, sr_json_find(j, root, "hullHpMax"), 30);

    int floors_arr = sr_json_find(j, root, "floors");
    if (floors_arr < 0) return false;
    ship->num_decks = sr_json_array_len(j, floors_arr);
    if (ship->num_decks > SHIP_MAX_DECKS) ship->num_decks = SHIP_MAX_DECKS;

    /* Build ship rooms from floor room data */
    int total_rooms = 0;
    for (int deck = 0; deck < ship->num_decks; deck++) {
        int floor_tok = sr_json_array_get(j, floors_arr, deck);
        if (floor_tok < 0) continue;

        int rooms_arr = sr_json_find(j, floor_tok, "rooms");
        int room_count = rooms_arr >= 0 ? sr_json_array_len(j, rooms_arr) : 0;

        ship->deck_room_start[deck] = total_rooms;
        ship->deck_room_count[deck] = room_count;

        for (int r = 0; r < room_count && total_rooms < SHIP_MAX_ROOMS; r++) {
            int room_tok = sr_json_array_get(j, rooms_arr, r);
            if (room_tok < 0) continue;

            ship_room *rm = &ship->rooms[total_rooms];
            rm->type = sr_json_enum(j, sr_json_find(j, room_tok, "type"),
                                    lvl_room_names, LVL_ROOM_NAME_COUNT, 0);
            rm->deck = deck;
            rm->room_idx = r;
            rm->subsystem_hp_max = sr_json_int(j, sr_json_find(j, room_tok, "subsystemHpMax"), 0);
            rm->subsystem_hp = sr_json_int(j, sr_json_find(j, room_tok, "subsystemHp"), rm->subsystem_hp_max);
            rm->cleared = false;
            rm->visited = false;
            total_rooms++;
        }
    }
    ship->room_count = total_rooms;

    /* Officers from floor data */
    int total_officers = 0;
    for (int deck = 0; deck < ship->num_decks; deck++) {
        int floor_tok = sr_json_array_get(j, floors_arr, deck);
        if (floor_tok < 0) continue;

        int officers_arr = sr_json_find(j, floor_tok, "officers");
        if (officers_arr < 0) continue;
        int ocount = sr_json_array_len(j, officers_arr);

        for (int o = 0; o < ocount && total_officers < SHIP_MAX_OFFICERS; o++) {
            int off_tok = sr_json_array_get(j, officers_arr, o);
            if (off_tok < 0) continue;

            ship_officer *officer = &ship->officers[total_officers];
            sr_json_str(j, sr_json_find(j, off_tok, "name"), officer->name, 32);
            officer->rank = sr_json_enum(j, sr_json_find(j, off_tok, "rank"),
                                         lvl_rank_names, LVL_RANK_NAME_COUNT, 0);
            officer->room_idx = sr_json_int(j, sr_json_find(j, off_tok, "roomIdx"),
                                            ship->deck_room_start[deck]);
            officer->alive = true;
            officer->captured = false;
            officer->combat_type = sr_json_enum(j, sr_json_find(j, off_tok, "combatType"),
                                                lvl_enemy_names, LVL_ENEMY_NAME_COUNT, 2) - 1;
            if (officer->combat_type < 0) officer->combat_type = 0;
            total_officers++;
        }
    }
    ship->officer_count = total_officers;

    /* Missions */
    int missions_arr = sr_json_find(j, root, "missions");
    if (missions_arr >= 0 && sr_json_array_len(j, missions_arr) > 0) {
        int m0 = sr_json_array_get(j, missions_arr, 0);
        if (m0 >= 0) {
            ship->mission.type = sr_json_enum(j, sr_json_find(j, m0, "type"),
                                              lvl_mission_names, LVL_MISSION_NAME_COUNT, 0);
            ship->mission.target_officer = sr_json_int(j, sr_json_find(j, m0, "targetOfficer"), -1);
            sr_json_str(j, sr_json_find(j, m0, "description"),
                        ship->mission.description, 64);
            ship->mission.completed = false;
            ship->mission.failed = false;
        }
    }

    ship->boarding_active = true;
    ship->initialized = true;
    return true;
}

/* ── High-level: load a complete level file ────────────────────── */

typedef struct {
    bool valid;
    bool is_hub;
    int num_floors;
    sr_json json;       /* parsed JSON (holds token data) */
    int root;           /* root object token */
    int floors_arr;     /* floors array token */
} lvl_loaded;

static lvl_loaded lvl_load(const char *path) {
    lvl_loaded result;
    memset(&result, 0, sizeof(result));

    char *data = lvl_read_file(path);
    if (!data) return result;

    if (!sr_json_parse(&result.json, data)) {
        free(data);
        return result;
    }
    /* Note: data must stay alive while json is used (tokens reference it) */
    /* We store data pointer in src field which is set by sr_json_parse */

    result.root = 0; /* root is always token 0 */
    result.is_hub = sr_json_bool(&result.json,
                                  sr_json_find(&result.json, 0, "isHub"), false);
    result.floors_arr = sr_json_find(&result.json, 0, "floors");
    result.num_floors = sr_json_array_len(&result.json, result.floors_arr);
    result.valid = result.num_floors > 0;

    return result;
}

static void lvl_load_all_floors(lvl_loaded *lvl, sr_dungeon *floors,
                                 bool *floor_generated, int max_floors) {
    for (int i = 0; i < lvl->num_floors && i < max_floors; i++) {
        int floor_tok = sr_json_array_get(&lvl->json, lvl->floors_arr, i);
        if (floor_tok >= 0) {
            lvl_load_floor(&floors[i], &lvl->json, floor_tok);
            floor_generated[i] = true;
        }
    }
}

#endif /* SR_LEVEL_LOADER_H */
