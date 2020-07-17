#include <ctype.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "json.h"

static const size_t DEFAULT_NUMBER_SIZE =
    ((size_t) ceill(logl(powl(2, sizeof(long double) * 8 - 1)) / logl(10.0L)))
    + 2;

static bool is_ws(char c) {
    switch (c) {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            return 1;
    }

    return 0;
}

static bool is_hex(char c) {
    return (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')
        || (c >= '0' && c <= '9');
}

static int is_valid_escape(const char *s) {
    switch (*s) {
        case 'u':
            for (int i = 1; i < 5; i++) {
                if (!is_hex(s[i])) {
                    return 0;
                }
            }
            return 6;
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

static size_t validate_string(const char *s, size_t start_idx, size_t n) {
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

static long double *parse_num(const char *s, size_t start_idx, size_t *end_idx,
        entry_type *type, size_t n) {
    long long int base = 0, frac = 0, exp_acc = 0;
    long long int sign = 1, exp_sign = 1;
    size_t exp = 1;
    long double *res = NULL;

    if (s[start_idx] == '-') {
        sign = -1;
        start_idx++;
    }

    size_t i = start_idx;
    if (s[start_idx] > '0' && s[start_idx] <= '9') {
        for (; i < n && isdigit(s[i]); i++) {
            if (!isdigit(s[i])) {
                break;
            }
            base = (base * 10) + (s[i] - '0');
        }
    } else if (s[start_idx] == '0') {
        i++;
    } else {
        return NULL;
    }

    if (s[i] == '.') {
        i++;
        size_t old = i;
        for (; i < n && isdigit(s[i]); i++) {
            frac = (frac * 10) + (s[i] - '0');
            exp *= 10;
        }

        if (i == old) {
            return NULL;
        }
    }

    if (s[i] == 'e' || s[i] == 'E') {
        i++;
        if (s[i] == '+' || s[i] == '-') {
            if (s[i] == '-') {
                exp_sign = -1;
            }
            i++;
        }

        size_t old = i;
        for (; i < n && isdigit(s[i]); i++) {
            exp_acc = (exp_acc * 10) + (s[i] - '0');
        }

        if (i == old) {
            return NULL;
        }
        exp_acc *= exp_sign;
    }

   *type = NUMBER;
   *end_idx = i;
   res = safe_malloc(sizeof(long double));
   *res = (exp_acc ? pow(10, exp_acc) : 1) * sign * (base
           + frac / (long double) exp);

   return res;

}

static json_entry_t *get_entry(const char *, size_t, size_t *, size_t);

static json_entry_t *get_value(const char *s, size_t start_idx,
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
        while (*end_idx < n) {
            if (!is_ws(s[*end_idx])) {
                if (end_c == '\0') {
                    json_destroy(entry);
                    return NULL;
                } else {
                    break;
                }
            }
            (*end_idx)++;
        }
    }

    return entry;
}

static json_entry_t *get_entry(const char *s, size_t start_idx, size_t *end_idx,
        size_t len) {
    json_entry_t *entry = safe_malloc(sizeof(json_entry_t));
    entry->type = UNKNOWN;

    char *json = (s + start_idx);
    bool fail = false;
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
                        fail = true;
                        break;
                    }
                    size_t value_start = key_end + 1;
                    while (value_start < len && s[value_start] != ':') {
                        if (!is_ws(s[value_start])) {
                            fail = true;
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
                            fail = true;
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
                    fail = true;
                    if (end_c != ',' && s[i] == '}') {
                        end_c = '}';
                        i++;
                        fail = false;
                    }
                    break;
                }
            }

            if (end_c != '}') {
                fail = true;
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
                    fail = true;
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
                    fail = true;
                    break;
                }
            }

            if (end_c != ']') {
                fail = true;
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
            *end_idx = validate_string(s, start_idx + 1, len);
            if (*end_idx) {
                (*end_idx)++;
                size_t n = *end_idx - start_idx;
                char *string = safe_malloc((n - 1) * sizeof(char));
                strncpy(string, (json + sizeof(char)), n - 2);
                string[n - 2] = '\0';
                entry->type = STRING;
                entry->item = string;
            }
            break;
        default:
            if (!strncmp(json, "true", 4)) {
                entry->type = BOOL;
                entry->item = safe_malloc(sizeof(bool));
                memset(entry->item, true, sizeof(bool));
                *end_idx += 4;
            } else if (!strncmp(json, "false", 5)) {
                entry->type = BOOL;
                entry->item = safe_malloc(sizeof(bool));
                memset(entry->item, false, sizeof(bool));
                *end_idx += 5;
            } else if (!strncmp(json, "null", 4)) {
                entry->type = NIL;
                entry->item = NULL;
                *end_idx += 4;
            } else {
                entry->item = parse_num(s, start_idx, end_idx,
                        &(entry->type), len);
            }

    }

    if (entry->type == UNKNOWN || fail) {
        json_destroy(entry);
        return NULL;
    }

    return entry;
}

json_entry_t *json_parse(const char *json, size_t len) {
    size_t end_idx;
    json_entry_t *entry = get_value(json, 0, &end_idx, len, '\0');

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
        case NUMBER:;
            long double ld = *((long double *) entry->item);
            json = safe_malloc((DEFAULT_NUMBER_SIZE + 1) * sizeof(char));
            len = sprintf(json, "%.*Lg", LDBL_DIG, ld);
            if (len < DEFAULT_NUMBER_SIZE) {
                json = safe_realloc(json, len + 1, sizeof(char));
            }
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

    *n = len;

    return json;
}

static void *get_json_item(const json_entry_t *entry,
        entry_type required_type) {
    if (entry->type != required_type) {
        fprintf(stderr, "Incorrect type!\n");
        return NULL;
    }

    return entry->item;
}

json_obj_t *get_json_obj(const json_entry_t *entry) {
    return get_json_item(entry, OBJECT);
}

json_array_t *get_json_array(const json_entry_t *entry) {
    return get_json_item(entry, ARRAY);
}

json_entry_t *get_json_obj_entry(const json_obj_t *obj, const char *key) {
    return hash_search(obj, key);
}

bool get_json_bool(const json_entry_t *entry) {
    return *((bool *) get_json_item(entry, BOOL));
}

char *get_json_string(const json_entry_t *entry) {
    return get_json_item(entry, STRING);
}

long double get_json_number(const json_entry_t *entry) {
    return *((long double *) get_json_item(entry, NUMBER));
}
