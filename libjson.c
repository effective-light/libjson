#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "libjson.h"

#define SHRINK_FACTOR 0.30f

static const size_t DEFAULT_NUMBER_LENGTH =
    ((size_t) ceill(log10l(powl(2.0L, sizeof(long double) * 8 - 1)))) + 2;

static _Thread_local const char *s;

static bool is_ws(char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            return true;
    }

    return false;
}

static bool is_hex(char c) {
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
        || (c >= '0' && c <= '9');
}

static bool validate_escape() {
    s++;
    switch (*s) {
        case 'u':
            for (int i = 1; i < 5; i++) {
                if (!is_hex(s[i])) {
                    return false;
                }
            }
            s += 5;
            return true;
        case '"':
        case '\\':
        case '/':
        case 'b':
        case 'f':
        case 'n':
        case 'r':
        case 't':
            s++;
            return true;
    }

    return false;
}

static bool validate_string() {
    s++;
    while (*s) {
        if (*s == '\\') {
            if (validate_escape()) {
                continue;
            } else {
                return false;
            }
        }

        switch (*s) {
            case '\a':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
            case '\v':
            case 0x1A:
            case 0x1B:
                return false;
        }

        if (*s == '"') {
            s++;
            return true;
        }

        s++;
    }

    return false;
}

static long double *parse_num() {
    long long int base = 0, frac = 0, exp_acc = 0;
    long long int sign = 1, exp_sign = 1;
    size_t exp = 1;
    long double *res = NULL;

    if (*s == '-') {
        sign = -1;
        s++;
    }

    if (*s > '0' && *s <= '9') {
        for (; isdigit(*s); s++) {
            base = (base * 10) + (*s - '0');
        }
    } else if (*s == '0') {
        s++;
    } else {
        return NULL;
    }

    if (*s == '.') {
        s++;
        const char *old = s;
        for (; isdigit(*s); s++) {
            frac = (frac * 10) + (*s - '0');
            exp *= 10;
        }

        if (s == old) {
            return NULL;
        }
    }

    if (*s == 'e' || *s == 'E') {
        s++;
        if (*s == '+' || *s == '-') {
            if (*s == '-') {
                exp_sign = -1;
            }
            s++;
        }

        const char *old = s;
        for (; isdigit(*s); s++) {
            exp_acc = (exp_acc * 10) + (*s - '0');
        }

        if (s == old) {
            return NULL;
        }
        exp_acc *= exp_sign;
    }

    res = safe_malloc(sizeof(long double));
    *res = (exp_acc ? pow(10, exp_acc) : 1) * sign * (base
            + frac / (long double) exp);

    return res;
}

static json_entry_t *json_create_generic(entry_type type, size_t size) {
    json_entry_t *entry = safe_malloc(sizeof(json_entry_t));
    entry->type = type;
    entry->item = safe_malloc(size);

    return entry;
}

static json_entry_t *get_value(char outer_end) {

    while (is_ws(*s)) {
        s++;
    }

    if (*s == outer_end) {
        return NULL;
    }

    json_entry_t *entry = NULL;
    char inner_end = '\0';

    switch (*s) {
        case '{':
            entry = json_create_obj();
            json_obj_t *obj = entry->item;
            const char *key_start;
            size_t key_len;

            for (s++; *s; s++) {
                if (*s == '"') {
                    key_start = (s + sizeof(char));
                    if (!validate_string()) {
                        goto FAIL;
                    }
                    key_len = s - key_start - sizeof(char);
                    while (*s != ':') {
                        if (!is_ws(*s)) {
                            goto FAIL;
                        }
                        s++;
                    }
                    s++;
                    json_entry_t *ent = get_value('}');
                    inner_end = *s;
                    if (!ent) {
                        if (inner_end != '}') {
                            goto FAIL;
                        }
                        break;
                    } else {
                        json_insert_obj_entry(obj, key_start, key_len, ent);
                    }
                    if (inner_end == '}') {
                        s++;
                        break;
                    }
                } else if (!is_ws(*s)) {
                    if (inner_end != ',' && *s == '}') {
                        s++;
                        break;
                    } else {
                        goto FAIL;
                    }
                }
            }
            break;
        case '[':
            entry = json_create_array();
            json_array_t *array = entry->item;

            for (s++; *s; s++) {
                json_entry_t *ent = get_value(']');

                if (!ent && inner_end == ',') {
                    goto FAIL;
                }

                if (ent) {
                    json_insert_array_entry(array, ent);
                    free(ent);
                }

                if (*s == ']') {
                    s++;
                    break;
                } else if (*s == ',') {
                    inner_end = ',';
                } else {
                    goto FAIL;
                }
            }
            break;
        case '"':;
            const char *start = (s + sizeof(char));
            if (validate_string()) {
                entry = json_create_string(start, s - start - sizeof(char));
            } else {
                goto FAIL;
            }
            break;
        default:
            if (!strncmp(s, "true", 4)) {
                entry = json_create_bool(true);
                s += 4;
            } else if (!strncmp(s, "false", 5)) {
                entry = json_create_bool(false);
                s += 5;
            } else if (!strncmp(s, "null", 4)) {
                entry = json_create_null();
                s += 4;
            } else {
                long double *ld = parse_num();
                if (ld) {
                    entry = json_create_generic(NUMBER, sizeof(long double));
                    entry->item = ld;
                }
            }
    }

    if (entry) {
        while (*s) {
            if (!is_ws(*s)) {
                if (!outer_end) {
                    goto FAIL;
                } else {
                    break;
                }
            }
            s++;
        }
    }

    return entry;

FAIL:
    json_destroy(entry);
    return NULL;
}

json_entry_t *json_parse(const char *json) {
    s = json;
    json_entry_t *entry = get_value('\0');

    if (!entry) {
        fprintf(stderr, "Invalid JSON!\n");
        return NULL;
    }

    return entry;
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

static char *stringify_num(double long ld, size_t *len) {
    char *json = safe_malloc((DEFAULT_NUMBER_LENGTH + 1) * sizeof(char));
    size_t idx = sprintf(json, "%lld", (long long int) ld);
    ld = fabsl(ld);
    ld = (ld - ((long long int) ld)) + powl(10.0L, -LDBL_DIG);
    size_t level = LDBL_DIG - 1;

    json[idx] = '.';
    size_t end_idx = 0;
    for (size_t i = idx + 1; ld && level;) {
        ld = 10.0L * ld;
        char sig = (char) ld;
        json[i] = '0' + sig;
        ld = ld - sig;
        i++;
        if (sig) {
            end_idx = i;
        }
        level--;
    }

    if (end_idx) {
        idx = end_idx;
    }

    json[idx] = '\0';

    if (idx < DEFAULT_NUMBER_LENGTH) {
        json = safe_realloc(json, idx, sizeof(char));
    }

    *len = idx;

    return json;
}

char *json_stringify(const json_entry_t *entry, size_t *n) {
    char *json = NULL;
    size_t len = 0;
    switch (entry->type) {
        case OBJECT:
            json = safe_malloc(3 * sizeof(char));
            json[0] = '{';
            json_obj_t *obj = entry->item;
            len = 2;
            for (size_t i = 0; i < obj->capacity; i++) {
                entry_t *e = (obj->entries + i);
                if (e->key) {
                    size_t key_len = strlen(e->key) + 2;
                    size_t item_len;
                    char *item_json = json_stringify((json_entry_t *) e->value,
                            &item_len);
                    item_len += 2;
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
                size_t item_len;
                char *item_json = json_stringify((arr->entries + i), &item_len);
                item_len++;
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
            len = strlen(entry->item) + 2;
            json = safe_malloc((len + 1) * sizeof(char));
            sprintf(json, "\"%s\"", entry->item);
            break;
        case NUMBER:
            json = stringify_num(*((long double *) entry->item), &len);
            break;
        case BOOL:
            if (*((bool *) entry->item)) {
                len = 4;
                json = safe_malloc(5 * sizeof(char));
                strcpy(json, "true");
            } else {
                len = 5;
                json = safe_malloc(6 * sizeof(char));
                strcpy(json, "false");
            }
            break;
        case NIL:
            len = 4;
            json = safe_malloc(5 * sizeof(char));
            strcpy(json, "null");
            break;
    }

    if (n) {
        *n = len;
    }

    return json;
}

static void *json_get_item(const json_entry_t *entry,
        entry_type required_type) {
    if (entry->type != required_type) {
        fprintf(stderr, "Incorrect type!\n");
        return NULL;
    }

    return entry->item;
}

json_obj_t *json_get_obj(const json_entry_t *entry) {
    return json_get_item(entry, OBJECT);
}

json_array_t *json_get_array(const json_entry_t *entry) {
    return json_get_item(entry, ARRAY);
}

json_entry_t *json_get_obj_entry(const json_obj_t *obj, const char *key) {
    return hash_search(obj, key);
}

bool json_get_bool(const json_entry_t *entry) {
    return *((bool *) json_get_item(entry, BOOL));
}

char *json_get_string(const json_entry_t *entry) {
    return json_get_item(entry, STRING);
}

long double json_get_number(const json_entry_t *entry) {
    return *((long double *) json_get_item(entry, NUMBER));
}

json_entry_t *json_create_obj() {
    json_entry_t *entry = safe_malloc(sizeof(json_entry_t));
    entry->type = OBJECT;
    entry->item = hash_init();

    return entry;
}

json_entry_t *json_create_array() {
    json_entry_t *entry = safe_malloc(sizeof(json_entry_t));
    entry->type = ARRAY;
    json_array_t *array = safe_malloc(sizeof(json_array_t));
    array->capacity = 1;
    array->entries = safe_malloc(array->capacity * sizeof(json_entry_t));
    array->size = 0;
    entry->item = array;

    return entry;
}

json_entry_t *json_create_string(const char *str, size_t len) {
    json_entry_t *entry = json_create_generic(STRING, (len + 1) * sizeof(char));
    char *new_str = entry->item;
    memcpy(new_str, str, len);
    new_str[len] = '\0';

    return entry;
}

json_entry_t *json_create_number(long double ld) {
    json_entry_t *entry = json_create_generic(NUMBER, sizeof(long double));
    memcpy(entry->item, &ld, sizeof(long double));

    return entry;
}

json_entry_t *json_create_bool(bool b) {
    json_entry_t *entry = json_create_generic(BOOL, sizeof(bool));
    memcpy(entry->item, &b, sizeof(bool));

    return entry;
}

json_entry_t *json_create_null() {
    json_entry_t *entry = json_create_generic(NIL, 0);
    entry->item = NULL;

    return entry;
}

void json_insert_obj_entry(json_obj_t *obj, const char *key, size_t len,
        json_entry_t *entry) {
    hash_insert(obj, key, len, entry);
}

void json_remove_obj_entry(json_obj_t *obj, const char *key) {
    free(hash_remove(obj, key));
}

void json_insert_array_entry(json_array_t *array, const json_entry_t *entry) {
    if (array->size == array->capacity) {
        array->capacity *= 2;
        array->entries = safe_realloc(array->entries, array->capacity,
                sizeof(json_entry_t));
    }

    memcpy((array->entries + array->size), entry, sizeof(json_entry_t));

    array->size++;
}

void json_remove_array_entry(json_array_t *array, size_t index) {
    if (index >= array->size) {
        fprintf(stderr, "json: array out of bounds");
        return;
    }

    if (index + 1 != array->size) {
        memcpy((array->entries + index), (array->entries + index + 1),
                (array->size - index + 1) * sizeof(json_entry_t));
    }

    array->size--;
    if (array->size > 1
            && (array->size / (float) array->capacity) < SHRINK_FACTOR) {
        array->capacity /= 2;
        array->entries = safe_realloc(array->entries, array->capacity,
                sizeof(json_entry_t));
    }
}

void json_nullify_entry(json_entry_t *entry) {
    entry->type = NIL;
    json_destroy(entry->item);
    entry->item = NULL;
}
