#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "json.h"

static regex_t num_re;
static regex_t hex4_re;

void json_init() {
    const char *num_pattern =
        "^-?(0|[1-9][[:digit:]]*)(\\.[[:digit:]]+)?([eE][+-]?[[:digit:]]+)?$";
    regcomp(&num_re, num_pattern, REG_EXTENDED);
    regcomp(&hex4_re, "^[0-9a-fA-F]{4}$", REG_EXTENDED);
}

static _Bool is_ws(char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            return 1;
    }

    return 0;
}

static void failure(char *msg) {
    fprintf(stderr, msg ? msg : "Invalid JSON\n");
    exit(1);
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

static int is_valid_escape(char *s) {
    char *tmp;
    switch (s[0]) {
        case 'u':
            tmp = calloc(5, sizeof(char));
            for (int i = 1; i < 5; i++) {
                if (s[i] == '\0') {
                    failure(NULL);
                }
                tmp[i - 1] = s[i];
            }

            tmp[4] = '\0';
            int ret = 0;

            if (!regexec(&hex4_re, tmp, 0, NULL, 0)) {
                ret = 6;
            }

            free(tmp);
            return ret;
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
            return 2;
    }

    return 0;
}

static size_t validate_string(char *s, size_t start_idx, size_t n) {
    size_t end_idx = start_idx;
    while (end_idx < n) {
        if (s[end_idx] == '\\') {
            int ret;
            if (end_idx + 1 < n
                    && (ret = is_valid_escape((s + (end_idx + 1))))) {
                end_idx += ret;
                continue;
            } else {
                failure(NULL);
            }
        }

        if (s[end_idx] == '"') {
            return end_idx;
        }

        end_idx++;
    }

    return -1;
}

static json_entry_t *get_value(char *s, size_t start_idx,
        size_t *end_idx, size_t n, char end_c) {
    size_t value_start = start_idx;

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
            value_end = validate_string(s, value_start + 1, n);
            break;
        default:
            c = s[value_end];
            while (value_end < n && !is_ws(c) && c != ',' && c != end_c) {
                value_end++;
                c = s[value_end];
            }
            value_end--;
            break;
    }

    size_t idx = value_end + 1;
    while (idx < n && s[idx] != ',' && s[idx] != end_c) {
        if (!is_ws(s[idx])) {
            failure(NULL);
        }
        idx++;
    }

    if (idx + 1 < n && s[idx] == end_c) {
        failure(NULL);
    }

    *end_idx = idx;

    return json_parse((s + value_start), value_end - value_start + 1);
}

json_entry_t *json_parse(char *json, size_t len) {
    json_entry_t *entry = malloc(sizeof(json_entry_t));
    char *tmp;
    entry->type = UNKNOWN;

    switch (json[0]) {
        case '{':
            if (json[len - 1] == '}') {
                json_obj_t *obj = malloc(sizeof(json_obj_t));
                obj->tbl = hash_init();
                size_t key_start;
                size_t key_end;
                for (size_t i = 1; i < len; i++) {
                    if (json[i] == '"') {
                        key_start = i + 1;
                        key_end = validate_string(json, key_start, len);
                        size_t value_start = key_end + 1;
                        while (value_start < len && json[value_start] != ':') {
                            if (!is_ws(json[value_start])) {
                                failure(NULL);
                            }
                            value_start++;
                        }
                        value_start++;
                        json_entry_t *entry = get_value(json, value_start,
                                &i, len, '}');
                        hash_insert(obj->tbl, (json + key_start),
                                key_end - key_start, entry);
                    } else if (!is_ws(json[i])) {
                        failure(NULL);
                    }

                }
                entry->type = OBJECT;
                entry->item = obj;
            }
            break;
        case '[':
            if (json[len - 1] == ']') {
                json_array_t *array = malloc(sizeof(json_array_t));
                array->size = 0;
                size_t capacity = 10;
                json_entry_t *entries = calloc(capacity, sizeof(json_entry_t));

                for (size_t i = 1; i < len; i++) {
                    json_entry_t *entry = get_value(json, i, &i, len, ']');
                    if (array->size == capacity) {
                        capacity *= 2;
                        entries = reallocarray(entries, capacity,
                                sizeof(json_entry_t));
                    }

                    memcpy((entries + array->size), entry,
                            sizeof(json_entry_t));
                    free(entry);
                    array->size++;
                }

                if (array->size < capacity) {
                    entries = reallocarray(entries, array->size,
                            sizeof(json_entry_t));
                }

                array->entries = entries;
                entry->type = ARRAY;
                entry->item = array;
            }
            break;
        case '"':
            if (json[len - 1] == '"') {
                validate_string(json, 1, len);
                char *string = calloc(len - 1, sizeof(char));
                strncpy(string, (json + sizeof(char)), len - 2);
                string[len - 2] = '\0';
                entry->type = STRING;
                entry->item = string;
            }
            break;
        default:
            tmp = calloc(len + 1, sizeof(char));
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
        failure(NULL);
    }

    return entry;
}

void json_exit() {
    regfree(&num_re);
    regfree(&hex4_re);
}

static void _json_destroy(json_entry_t *entry) {
    json_obj_t *obj;
    json_array_t *arr;

    switch (entry->type) {
        case UNKNOWN:
            failure(NULL);
            break;
        case OBJECT:
            obj = entry->item;
            hashtable_t *tbl = obj->tbl;
            for (size_t i = 0; i < tbl->capacity; i++) {
                entry_t *e = (tbl->entries + i);
                if (e->key) {
                    _json_destroy((json_entry_t *) e->value);
                }
            }
            hash_destroy(tbl);
            break;
        case ARRAY:
            arr = entry->item;
            for (size_t i = 0; i < arr->size; i++) {
                _json_destroy((arr->entries + i));
            }
            free(arr->entries);
            break;
    }

    free(entry->item);
}

void json_destroy(json_entry_t *entry) {
    _json_destroy(entry);
    free(entry);
    entry = NULL;
}

char *json_stringify(json_entry_t *entry) {
    char *json = NULL;
    size_t len;
    switch (entry->type) {
        case UNKNOWN:
            failure(NULL);
            break;
        case OBJECT:
            json = calloc(3, sizeof(char));
            json[0] = '{';
            json_obj_t *obj = entry->item;
            hashtable_t *tbl = obj->tbl;
            len = 2;
            for (size_t i = 0; i < tbl->capacity; i++) {
                entry_t *e = (tbl->entries + i);
                if (e->key) {
                    size_t key_len = strlen(e->key) + 2;
                    char *item_json = json_stringify((json_entry_t *)
                            e->value);
                    size_t item_len = strlen(item_json) + 2;
                    json = reallocarray(json, len + key_len + item_len,
                            sizeof(char));
                    sprintf((json + (len - 1)), "\"%s\":%s,", e->key,
                            item_json);
                    free(item_json);
                    len += key_len + item_len;
                }
            }
            json[len - 2] = '}';
            json[len - 1] = '\0';
            break;
        case ARRAY:
            json = calloc(3, sizeof(char));
            json[0] = '[';
            json_array_t *arr = entry->item;
            len = 2;
            for (size_t i = 0; i < arr->size; i++) {
                char *item_json = json_stringify((arr->entries + i));
                size_t item_len = strlen(item_json) + 1;
                json = reallocarray(json, len + item_len, sizeof(char));
                sprintf((json + (len - 1)), "%s,", item_json);
                free(item_json);
                len += item_len;
            }
            json[len - 2] = ']';
            json[len - 1] = '\0';
            break;
        case STRING:
            json = calloc(strlen((char *) entry->item) + 3, sizeof(char));
            sprintf(json, "\"%s\"", (char *) entry->item);
            break;
        case NUMBER:
            json = calloc((snprintf(NULL, 0, "%Lf",
                        *((long double *) entry->item)) + 1), sizeof(char));
            sprintf(json, "%Lf", *((long double *) entry->item));
            break;
        case BOOL:
            if (*((_Bool *) entry->item)) {
                json = calloc(5, sizeof(char));
                strcpy(json, "true");
            } else {
                json = calloc(6, sizeof(char));
                strcpy(json, "false");
            }
            break;
        case NIL:
            json = calloc(5, sizeof(char));
            strcpy(json, "null");
            break;
    }

    return json;
}
