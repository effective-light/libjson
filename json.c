#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "json.h"

static regex_t num_re;

void json_init() {
    const char *num_pattern =
        "^-?(0|[1-9][[:digit:]]*)(\\.[[:digit:]]+)?([eE][+-]?[[:digit:]]+)?$";
    regcomp(&num_re, num_pattern, REG_EXTENDED);
}

_Bool is_ws(char c) {
    switch(c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            return 1;
    }

    return 0;
}

static size_t find_end(char *text, size_t start_idx,
        size_t n, char start, char end) {
    size_t end_idx = start_idx;
    size_t count = 1;
    while (count && end_idx < n) {
        char c = text[++end_idx];
        if (c == start) {
            count++;
        } else if (c == end) {
            count--;
        }
    }

    return end_idx;
}

static size_t get_string_end(char *s, size_t start_idx, size_t n) {
    size_t end_idx = start_idx;
    while (end_idx < n) {
        if (s[end_idx] == '\\' && end_idx + 1 < n && s[end_idx + 1] == '"') {
            end_idx += 2;
            continue;
        }

        if (s[end_idx] == '"') {
            return end_idx - 1;
        }

        end_idx++;
    }

    return -1;
}

static json_entry_t *get_value(char *s, size_t start_idx,
        size_t *end_idx, size_t n) {
    size_t value_start = start_idx;

    while (value_start < n && s[value_start] != ':') {
        value_start++;
    }
    value_start++;

    while (value_start < n && is_ws(s[value_start])) {
        value_start++;
    }

    size_t value_end = value_start;
    char c;
    switch (s[value_start]) {
        case '{':
            value_end = find_end(s, value_start, n, '{', '}');
            break;
        case '[':
            value_end = find_end(s, value_start, n, '[', ']');
            break;
        case '"':
            value_end = get_string_end(s, value_start + 1, n) + 1;
            break;
        default:
            c = s[value_end];
            while (value_end < n && !is_ws(c) && c != ',' && c != '}') {
                value_end++;
                c = s[value_end];
            }
            value_end--;
            break;
    }

    size_t idx = value_end;
    while (idx < n && s[idx] != ',' && s[idx] != '}') {
        idx++;
    }

    *end_idx = idx;

    return parse_json((s + value_start), value_end - value_start + 1);
}

json_entry_t *parse_json(char *json, size_t len) {
    json_entry_t *entry = malloc(sizeof(json_entry_t));
    entry->type = UNKNOWN;

    switch (json[0]) {
        case '{':
            if (json[len - 1] == '}') {
                json_obj_t *obj = malloc(sizeof(json_obj_t));
                obj->tbl = hash_init();
                size_t key_start;
                size_t key_end;
                size_t i = 1;
                while (i < len) {
                    if (json[i] == '"') {
                        key_start = i + 1;
                        key_end = get_string_end(json, key_start, len);
                        json_entry_t *entry = get_value(json, key_end + 2,
                                &i, len);
                        hash_insert(obj->tbl, (json + key_start),
                                key_end - key_start + 1, entry);
                    }
                    i++;
                }
                entry->type = OBJECT;
                entry->item = obj;
            }
            break;
        case '[':
            if (json[len - 1] == ']') {
                json_array_t *array = malloc(sizeof(json_array_t));
                // TODO: parse array
                entry->type = ARRAY;
                entry->item = array;
            }
            break;
        case '"':
            if (json[len - 1] == '"') {
                char *string = malloc(len - 1);
                strncpy(string, (json + sizeof(char)), len - 2);
                string[len - 1] = '\0';
                // TODO: verify validity of string
                entry->type = STRING;
                entry->item = string;
            }
            break;
        default:;
            char *tmp = malloc(len + 1);
            strncpy(tmp, json, len);
            tmp[len] = '\0';

            if (!regexec(&num_re, tmp, 0, NULL, 0)) {
                long double num = strtold(tmp, NULL);
                entry->type = NUMBER;
                entry->item = malloc(sizeof(long double));
                memcpy(entry->item, &num, sizeof(long double));
            } else if (!strcmp(tmp, "true")) {
                entry->type = BOOL;
                entry->item = malloc(sizeof(_Bool));
                memset(entry->item, 1, sizeof(_Bool));
            } else if (!strcmp(tmp, "false")) {
                entry->type = BOOL;
                entry->item = malloc(sizeof(_Bool));
                memset(entry->item, 0, sizeof(_Bool));
            } else if (!strcmp(tmp, "null")) {
                entry->type = NIL;
                entry->item = NULL;
            }

            free(tmp);
    }

    if (entry->type == UNKNOWN) {
        free(entry);
        fprintf(stderr, "Invalid JSON\n");
        return NULL;
    }

    return entry;
}
