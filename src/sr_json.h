#ifndef SR_JSON_H
#define SR_JSON_H

/*  Minimal JSON parser for Space Hulks level loading.
 *
 *  Token-based parser (no malloc). Parses JSON into a flat token array.
 *  Supports: objects, arrays, strings, numbers, booleans, null.
 *
 *  Usage:
 *    sr_json json;
 *    sr_json_parse(&json, json_string);
 *    int floors = sr_json_find(&json, 0, "floors");
 *    int floor0 = sr_json_array_get(&json, floors, 0);
 *    int width = sr_json_find(&json, floor0, "width");
 *    int val = sr_json_int(&json, width, 20);
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define SR_JSON_MAX_TOKENS 8192

typedef enum {
    SR_JSON_OBJECT,
    SR_JSON_ARRAY,
    SR_JSON_STRING,
    SR_JSON_NUMBER,
    SR_JSON_BOOL,
    SR_JSON_NULL,
} sr_json_type;

typedef struct {
    sr_json_type type;
    int start;      /* start index in source string */
    int end;        /* end index (exclusive) */
    int size;       /* number of child tokens (for objects/arrays) */
    int parent;     /* parent token index (-1 for root) */
} sr_json_token;

typedef struct {
    const char *src;
    sr_json_token tokens[SR_JSON_MAX_TOKENS];
    int count;
    int pos;        /* parser cursor */
} sr_json;

/* ── Forward declarations ──────────────────────────────────────── */
static int sr_json_parse_value(sr_json *j);

static void sr_json_skip_ws(sr_json *j) {
    while (j->src[j->pos] == ' ' || j->src[j->pos] == '\t' ||
           j->src[j->pos] == '\n' || j->src[j->pos] == '\r')
        j->pos++;
}

static int sr_json_alloc_token(sr_json *j, sr_json_type type, int start) {
    if (j->count >= SR_JSON_MAX_TOKENS) return -1;
    int idx = j->count++;
    j->tokens[idx].type = type;
    j->tokens[idx].start = start;
    j->tokens[idx].end = start;
    j->tokens[idx].size = 0;
    j->tokens[idx].parent = -1;
    return idx;
}

static int sr_json_parse_string(sr_json *j) {
    if (j->src[j->pos] != '"') return -1;
    j->pos++; /* skip opening quote */
    int start = j->pos;
    while (j->src[j->pos] && j->src[j->pos] != '"') {
        if (j->src[j->pos] == '\\') j->pos++; /* skip escape */
        j->pos++;
    }
    int idx = sr_json_alloc_token(j, SR_JSON_STRING, start);
    if (idx < 0) return -1;
    j->tokens[idx].end = j->pos;
    if (j->src[j->pos] == '"') j->pos++; /* skip closing quote */
    return idx;
}

static int sr_json_parse_number(sr_json *j) {
    int start = j->pos;
    if (j->src[j->pos] == '-') j->pos++;
    while (j->src[j->pos] >= '0' && j->src[j->pos] <= '9') j->pos++;
    if (j->src[j->pos] == '.') {
        j->pos++;
        while (j->src[j->pos] >= '0' && j->src[j->pos] <= '9') j->pos++;
    }
    int idx = sr_json_alloc_token(j, SR_JSON_NUMBER, start);
    if (idx < 0) return -1;
    j->tokens[idx].end = j->pos;
    return idx;
}

static int sr_json_parse_literal(sr_json *j, const char *lit, sr_json_type type) {
    int len = (int)strlen(lit);
    if (strncmp(j->src + j->pos, lit, len) != 0) return -1;
    int idx = sr_json_alloc_token(j, type, j->pos);
    if (idx < 0) return -1;
    j->tokens[idx].end = j->pos + len;
    j->pos += len;
    return idx;
}

static int sr_json_parse_object(sr_json *j) {
    if (j->src[j->pos] != '{') return -1;
    int idx = sr_json_alloc_token(j, SR_JSON_OBJECT, j->pos);
    if (idx < 0) return -1;
    j->pos++; /* skip { */
    int count = 0;

    sr_json_skip_ws(j);
    if (j->src[j->pos] == '}') { j->pos++; j->tokens[idx].end = j->pos; return idx; }

    while (1) {
        sr_json_skip_ws(j);
        /* Parse key */
        int key = sr_json_parse_string(j);
        if (key < 0) break;
        j->tokens[key].parent = idx;

        sr_json_skip_ws(j);
        if (j->src[j->pos] == ':') j->pos++;
        sr_json_skip_ws(j);

        /* Parse value */
        int val = sr_json_parse_value(j);
        if (val < 0) break;
        j->tokens[val].parent = idx;
        count += 2; /* key + value */

        sr_json_skip_ws(j);
        if (j->src[j->pos] == ',') { j->pos++; continue; }
        if (j->src[j->pos] == '}') { j->pos++; break; }
        break; /* error */
    }

    j->tokens[idx].size = count;
    j->tokens[idx].end = j->pos;
    return idx;
}

static int sr_json_parse_array(sr_json *j) {
    if (j->src[j->pos] != '[') return -1;
    int idx = sr_json_alloc_token(j, SR_JSON_ARRAY, j->pos);
    if (idx < 0) return -1;
    j->pos++; /* skip [ */
    int count = 0;

    sr_json_skip_ws(j);
    if (j->src[j->pos] == ']') { j->pos++; j->tokens[idx].end = j->pos; return idx; }

    while (1) {
        sr_json_skip_ws(j);
        int val = sr_json_parse_value(j);
        if (val < 0) break;
        j->tokens[val].parent = idx;
        count++;

        sr_json_skip_ws(j);
        if (j->src[j->pos] == ',') { j->pos++; continue; }
        if (j->src[j->pos] == ']') { j->pos++; break; }
        break; /* error */
    }

    j->tokens[idx].size = count;
    j->tokens[idx].end = j->pos;
    return idx;
}

static int sr_json_parse_value(sr_json *j) {
    sr_json_skip_ws(j);
    char c = j->src[j->pos];
    if (c == '{') return sr_json_parse_object(j);
    if (c == '[') return sr_json_parse_array(j);
    if (c == '"') return sr_json_parse_string(j);
    if (c == '-' || (c >= '0' && c <= '9')) return sr_json_parse_number(j);
    if (c == 't') return sr_json_parse_literal(j, "true", SR_JSON_BOOL);
    if (c == 'f') return sr_json_parse_literal(j, "false", SR_JSON_BOOL);
    if (c == 'n') return sr_json_parse_literal(j, "null", SR_JSON_NULL);
    return -1;
}

/* ── Public API ────────────────────────────────────────────────── */

static bool sr_json_parse(sr_json *j, const char *src) {
    memset(j, 0, sizeof(*j));
    j->src = src;
    j->pos = 0;
    int root = sr_json_parse_value(j);
    return root >= 0;
}

/* Compare token string content */
static bool sr_json_eq(const sr_json *j, int token, const char *str) {
    if (token < 0 || token >= j->count) return false;
    const sr_json_token *t = &j->tokens[token];
    int len = t->end - t->start;
    if (len != (int)strlen(str)) return false;
    return strncmp(j->src + t->start, str, len) == 0;
}

/* Find a key in an object, return value token index */
static int sr_json_find(const sr_json *j, int obj, const char *key) {
    if (obj < 0 || obj >= j->count) return -1;
    if (j->tokens[obj].type != SR_JSON_OBJECT) return -1;
    /* Walk direct children (parent == obj). Keys and values alternate. */
    int child_count = 0;
    for (int i = obj + 1; i < j->count && child_count < j->tokens[obj].size; i++) {
        if (j->tokens[i].parent != obj) continue;
        child_count++;
        /* Odd children are keys (1st, 3rd, ...) */
        if ((child_count & 1) == 1 && sr_json_eq(j, i, key)) {
            /* Find the next direct child = the value */
            for (int v = i + 1; v < j->count; v++) {
                if (j->tokens[v].parent == obj) return v;
            }
        }
    }
    return -1;
}

/* Get nth element of an array */
static int sr_json_array_get(const sr_json *j, int arr, int index) {
    if (arr < 0 || arr >= j->count) return -1;
    if (j->tokens[arr].type != SR_JSON_ARRAY) return -1;
    if (index >= j->tokens[arr].size) return -1;
    int count = 0;
    for (int i = arr + 1; i < j->count; i++) {
        if (j->tokens[i].parent == arr) {
            if (count == index) return i;
            count++;
        }
    }
    return -1;
}

/* Extract integer value */
static int sr_json_int(const sr_json *j, int token, int def) {
    if (token < 0 || token >= j->count) return def;
    const sr_json_token *t = &j->tokens[token];
    if (t->type != SR_JSON_NUMBER) return def;
    char buf[32];
    int len = t->end - t->start;
    if (len >= 32) len = 31;
    memcpy(buf, j->src + t->start, len);
    buf[len] = '\0';
    return atoi(buf);
}

/* Extract boolean value */
static bool sr_json_bool(const sr_json *j, int token, bool def) {
    if (token < 0 || token >= j->count) return def;
    return j->src[j->tokens[token].start] == 't';
}

/* Extract string into buffer */
static void sr_json_str(const sr_json *j, int token, char *out, int max_len) {
    out[0] = '\0';
    if (token < 0 || token >= j->count) return;
    const sr_json_token *t = &j->tokens[token];
    if (t->type != SR_JSON_STRING) return;
    int len = t->end - t->start;
    if (len >= max_len) len = max_len - 1;
    memcpy(out, j->src + t->start, len);
    out[len] = '\0';
}

/* Get array length */
static int sr_json_array_len(const sr_json *j, int arr) {
    if (arr < 0 || arr >= j->count) return 0;
    if (j->tokens[arr].type != SR_JSON_ARRAY) return 0;
    return j->tokens[arr].size;
}

/* Map string to enum value using name table */
static int sr_json_enum(const sr_json *j, int token, const char **names, int count, int def) {
    if (token < 0 || token >= j->count) return def;
    for (int i = 0; i < count; i++) {
        if (sr_json_eq(j, token, names[i])) return i;
    }
    return def;
}

#endif /* SR_JSON_H */
