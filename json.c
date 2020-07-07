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

        switch (s[end_idx]) {
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                return 0;
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

static json_entry_t *get_entry(char *, size_t, size_t *, size_t);

static json_entry_t *get_value(char *s, size_t start_idx,
        size_t *end_idx, size_t n, char end_c) {

    size_t value_start = start_idx;
    while (value_start < n && is_ws(s[value_start])) {
        value_start++;
    }

    if (s[value_start] == end_c) {
        *end_idx = value_start;
        return NULL;
    }

    json_entry_t *entry = get_entry(s, value_start, end_idx, n);

    if (!entry) {
        *end_idx = start_idx;
    } else {
        while (*end_idx < n && is_ws(s[*end_idx])) {
            (*end_idx)++;
        }
    }

    return entry;
}

static json_entry_t *get_entry(char *s, size_t start_idx, size_t *end_idx,
        size_t len) {
    json_entry_t *entry = safe_malloc(sizeof(json_entry_t));
    entry->type = UNKNOWN;

    char *json = (s + start_idx);
    _Bool fail = 0;
    char end_c = '\0';
    *end_idx = start_idx;
    size_t i;

    switch (s[start_idx]) {
        case '{':;
            json_obj_t *obj = hash_init();
            size_t key_start;
            size_t key_end;
            for (i = start_idx + 1; i < len; i++) {
                if (s[i] == '"') {
                    key_start = i + 1;
                    key_end = validate_string(s, key_start, len);
                    if (!key_end) {
                        fail = 1;
                        break;
                    }
                    size_t value_start = key_end + 1;
                    while (value_start < len && s[value_start] != ':') {
                        if (!is_ws(s[value_start])) {
                            fail = 1;
                            break;
                        }
                        value_start++;
                    }
                    if (fail) {
                        break;
                    }
                    value_start++;
                    size_t old_i = i - 1;
                    json_entry_t *ent = get_value(s, value_start,
                            &i, len, '}');
                    end_c = s[i];
                    if (!ent) {
                        if (end_c == ',') {
                            fail = 1;
                        }
                        break;
                    } else {
                        hash_insert(obj, (s + key_start), key_end - key_start,
                                ent);
                    }
                    if (end_c == '}') {
                        end_c = '}';
                        i++;
                        break;
                    }
                } else if (i < len && !is_ws(s[i])) {
                    fail = 1;
                    if (end_c != ',' && s[i] == '}') {
                        end_c = '}';
                        i++;
                        fail = 0;
                    }
                    break;
                }
            }

            if (end_c != '}') {
                fail = 1;
            } else {
                *end_idx = i;
            }

            entry->type = OBJECT;
            entry->item = obj;
            break;
        case '[':;
            json_array_t *array = safe_malloc(sizeof(json_array_t));
            array->size = 0;
            size_t capacity = 10;
            json_entry_t *entries = safe_malloc(capacity
                    * sizeof(json_entry_t));

            for (i = start_idx + 1; i < len; i++) {
                json_entry_t *ent = get_value(s, i, &i, len, ']');

                if (!ent && end_c == ',') {
                    fail = 1;
                    break;
                }

                if (ent) {
                    if (array->size == capacity) {
                        capacity *= 2;
                        entries = safe_realloc(entries, capacity,
                                sizeof(json_entry_t));
                    }
                    memcpy((entries + array->size), ent, sizeof(json_entry_t));
                    free(ent);
                    array->size++;
                }

                if (i < len && s[i] == ']') {
                    end_c = ']';
                    i++;
                    break;
                } else if (i < len && s[i] == ',') {
                    end_c = ',';
                } else {
                    fail = 1;
                    break;
                }
            }

            if (end_c != ']') {
                fail = 1;
            } else {
                *end_idx = i;
            }


            if (array->size < capacity) {
                entries = safe_realloc(entries, array->size,
                        sizeof(json_entry_t));
            }

            array->entries = entries;
            entry->type = ARRAY;
            entry->item = array;
            break;
        case '"':
            *end_idx = validate_string(s, start_idx + 1, len) + 1;
            size_t n = *end_idx - start_idx;
            char *string = safe_malloc((n - 1) * sizeof(char));
            strncpy(string, (json + sizeof(char)), n - 2);
            string[n - 2] = '\0';
            entry->type = STRING;
            entry->item = string;
            break;
        default:
            if (!strncmp(json, "true", 4)) {
                entry->type = BOOL;
                entry->item = safe_malloc(sizeof(_Bool));
                memset(entry->item, 1, sizeof(_Bool));
                *end_idx += 4;
            } else if (!strncmp(json, "false", 5)) {
                entry->type = BOOL;
                entry->item = safe_malloc(sizeof(_Bool));
                memset(entry->item, 0, sizeof(_Bool));
                *end_idx += 5;
            } else if (!strncmp(json, "null", 4)) {
                entry->type = NIL;
                entry->item = NULL;
                *end_idx += 4;
            } else {
                // TODO: rewrite number parsing
                /*char *tmp = safe_malloc((len + 1) * sizeof(char));
                strncpy(tmp, json, len);
                tmp[len] = '\0';
                if (!regexec(&num_re, tmp, 0, NULL, 0)) {
                    long double num = strtold(tmp, NULL);
                    entry->type = NUMBER;
                    entry->item = safe_malloc(sizeof(long double));
                    memcpy(entry->item, &num, sizeof(long double));
                }
                free(tmp);*/
            }

    }

    if (entry->type == UNKNOWN || fail) {
        json_destroy(entry);
        return NULL;
    }

    return entry;
}

static json_entry_t *json_parse_len(char *json, size_t len) {
    size_t end_idx;
    json_entry_t *entry = get_value(json, 0, &end_idx, len, '\0');

    if (!entry) {
        fprintf(stderr, "Invalid JSON!\n");
        return NULL;
    }

    return entry;
}

json_entry_t *json_parse(char *json) {
    return json_parse_len(json, strlen(json));
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

static void *get_json_item(json_entry_t *entry, entry_type required_type) {
    if (entry->type != required_type) {
        fprintf(stderr, "Incorrect type!\n");
        return NULL;
    }

    return entry->item;
}

json_obj_t *get_json_obj(json_entry_t *entry) {
    return get_json_item(entry, OBJECT);
}

json_array_t *get_json_array(json_entry_t *entry) {
    return get_json_item(entry, ARRAY);
}

json_entry_t *get_json_obj_entry(json_obj_t *obj, char *key) {
    return hash_search(obj, key);
}
