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
            tmp = safe_malloc(5 * sizeof(char));
            for (int i = 1; i < 5; i++) {
                if (s[i] == '\0') {
                    free(tmp);
                    return 0;
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
                return 0;
            }
        }

        if (s[end_idx] == '"') {
            return end_idx;
        }

        end_idx++;
    }

    return 0;
}

static void *safe_realloc(void *ptr, size_t nmemb, size_t size) {
    void *mem = realloc(ptr, nmemb * size);

    if (!mem && nmemb) {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }

    return mem;
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
            if (!value_end) {
                return NULL;
            }
            break;
        default:
            c = s[value_end];
            if (value_end + 1 == n && c == end_c) {
                *end_idx = value_end + 1;
                return NULL;
            }
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
            return NULL;
        }
        idx++;
    }

    if (idx + 1 < n && s[idx] == end_c) {
        return NULL;
    }


    json_entry_t *entry = json_parse((s + value_start),
            value_end - value_start + 1);

    if (entry) {
        *end_idx = idx;
    }

    return entry;
}

json_entry_t *json_parse(char *json, size_t len) {
    json_entry_t *entry = safe_malloc(sizeof(json_entry_t));
    char *tmp;
    entry->type = UNKNOWN;
    _Bool fail = 0;

    switch (json[0]) {
        case '{':
            if (json[len - 1] == '}') {
                json_obj_t *obj = hash_init();
                size_t key_start;
                size_t key_end;
                for (size_t i = 1; i < len; i++) {
                    if (json[i] == '"') {
                        key_start = i + 1;
                        key_end = validate_string(json, key_start, len);
                        if (key_end < key_start) {
                            fail = 1;
                            break;
                        }
                        size_t value_start = key_end + 1;
                        while (value_start < len && json[value_start] != ':') {
                            if (!is_ws(json[value_start])) {
                                fail = 1;
                                break;
                            }
                            value_start++;
                        }
                        if (fail) {
                            break;
                        }
                        value_start++;
                        size_t old_i = i;
                        json_entry_t *ent = get_value(json, value_start,
                                &i, len, '}');
                        if (!ent && i == old_i) {
                            fail = 2;
                            break;
                        }
                        if (ent) {
                            hash_insert(obj, (json + key_start),
                                    key_end - key_start, ent);
                        }
                    } else if (!is_ws(json[i])) {
                        if (json[i] != '}') {
                            fail = 1;
                        }
                        break;
                    }

                }
                entry->type = OBJECT;
                entry->item = obj;
            }
            break;
        case '[':
            if (json[len - 1] == ']') {
                json_array_t *array = safe_malloc(sizeof(json_array_t));
                array->size = 0;
                size_t capacity = 10;
                json_entry_t *entries = safe_malloc(capacity
                        * sizeof(json_entry_t));

                for (size_t i = 1; i < len; i++) {
                    size_t old_i = i;
                    json_entry_t *ent = get_value(json, i, &i, len, ']');
                    if (!ent && i == old_i) {
                        fail = 2;
                        break;
                    }

                    if (ent) {
                        if (array->size == capacity) {
                            capacity *= 2;
                            entries = safe_realloc(entries, capacity,
                                    sizeof(json_entry_t));
                        }
                        memcpy((entries + array->size), ent,
                                sizeof(json_entry_t));
                        free(ent);
                        array->size++;
                    }
                }

                if (array->size < capacity) {
                    entries = safe_realloc(entries, array->size,
                            sizeof(json_entry_t));
                }

                array->entries = entries;
                entry->type = ARRAY;
                entry->item = array;
            }
            break;
        case '"':
            if (json[len - 1] == '"') {
                if (!validate_string(json, 1, len)) {
                    break;
                }
                char *string = safe_malloc((len - 1) * sizeof(char));
                strncpy(string, (json + sizeof(char)), len - 2);
                string[len - 2] = '\0';
                entry->type = STRING;
                entry->item = string;
            }
            break;
        default:
            tmp = safe_malloc((len + 1) * sizeof(char));
            strncpy(tmp, json, len);
            tmp[len] = '\0';

            if (!regexec(&num_re, tmp, 0, NULL, 0)) {
                long double num = strtold(tmp, NULL);
                entry->type = NUMBER;
                entry->item = safe_malloc(sizeof(long double));
                memcpy(entry->item, &num, sizeof(long double));
            } else if (!strcmp(tmp, "true")) {
                entry->type = BOOL;
                entry->item = safe_malloc(sizeof(_Bool));
                memset(entry->item, 1, sizeof(_Bool));
            } else if (!strcmp(tmp, "false")) {
                entry->type = BOOL;
                entry->item = safe_malloc(sizeof(_Bool));
                memset(entry->item, 0, sizeof(_Bool));
            } else if (!strcmp(tmp, "null")) {
                entry->type = NIL;
                entry->item = NULL;
            }

            free(tmp);
    }


    if (entry->type == UNKNOWN || fail) {
        json_destroy(entry);
        if (fail == 1) {
            fprintf(stderr, "Invalid JSON\n");
        }
        return NULL;
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
            return;
        case OBJECT:
            obj = entry->item;
            for (size_t i = 0; i < obj->capacity; i++) {
                entry_t *e = (obj->entries + i);
                if (e->key) {
                    _json_destroy((json_entry_t *) e->value);
                }
            }
            hash_destroy(obj);
            break;
        case ARRAY:
            arr = entry->item;
            for (size_t i = 0; i < arr->size; i++) {
                _json_destroy((arr->entries + i));
            }
            free(arr->entries);
            free(arr);
            break;
        default:
            free(entry->item);
            break;
    }
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
            return json;
        case OBJECT:
            json = safe_malloc(3 * sizeof(char));
            json[0] = '{';
            json_obj_t *obj = entry->item;
            len = 2;
            for (size_t i = 0; i < obj->capacity; i++) {
                entry_t *e = (obj->entries + i);
                if (e->key) {
                    size_t key_len = strlen(e->key) + 2;
                    char *item_json = json_stringify((json_entry_t *)
                            e->value);
                    size_t item_len = strlen(item_json) + 2;
                    json = safe_realloc(json, len + key_len + item_len,
                            sizeof(char));
                    sprintf((json + (len - 1)), "\"%s\":%s,", e->key,
                            item_json);
                    free(item_json);
                    len += key_len + item_len;
                }
            }
            if (obj->size) {
                len--;
            }
            json[len - 1] = '}';
            json[len] = '\0';
            break;
        case ARRAY:
            json = safe_malloc(3 * sizeof(char));
            json[0] = '[';
            json_array_t *arr = entry->item;
            len = 2;
            for (size_t i = 0; i < arr->size; i++) {
                char *item_json = json_stringify((arr->entries + i));
                size_t item_len = strlen(item_json) + 1;
                json = safe_realloc(json, len + item_len, sizeof(char));
                sprintf((json + (len - 1)), "%s,", item_json);
                free(item_json);
                len += item_len;
            }
            if (arr->size) {
                len--;
            }

            json[len - 1] = ']';
            json[len] = '\0';
            break;
        case STRING:
            json = safe_malloc((strlen((char *) entry->item) + 3)
                    * sizeof(char));
            sprintf(json, "\"%s\"", (char *) entry->item);
            break;
        case NUMBER:
            json = safe_malloc((snprintf(NULL, 0, "%Lf",
                        *((long double *) entry->item)) + 1) * sizeof(char));
            sprintf(json, "%Lf", *((long double *) entry->item));
            break;
        case BOOL:
            if (*((_Bool *) entry->item)) {
                json = safe_malloc(5 * sizeof(char));
                strcpy(json, "true");
            } else {
                json = safe_malloc(6 * sizeof(char));
                strcpy(json, "false");
            }
            break;
        case NIL:
            json = safe_malloc(5 * sizeof(char));
            strcpy(json, "null");
            break;
    }

    return json;
}
