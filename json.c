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

json_entry_t *parse_json(char *json, size_t len) {
    json_entry_t *entry = malloc(sizeof(json_entry_t));
    entry->type = UNKNOWN;
    switch (json[0]) {
        case '{':
            if (json[len - 1] == '}') {
                json_obj_t *obj = malloc(sizeof(json_obj_t));
                obj->tbl = hash_init();
                // TODO: parse object
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
        default:
            if (!regexec(&num_re, json, 0, NULL, 0)) {
                long double num = strtold(json, NULL);
                entry->type = NUMBER;
                entry->item = malloc(sizeof(long double));
                memcpy(entry->item, &num, sizeof(long double));
            } else if (!strcmp(json, "true")) {
                entry->type = BOOL;
                entry->item = malloc(sizeof(char));
                memset(entry->item, 1, sizeof(char));
            } else if (!strcmp(json, "false")) {
                entry->type = BOOL;
                entry->item = malloc(sizeof(char));
                memset(entry->item, 0, sizeof(char));
            } else if (!strcmp(json, "null")) {
                entry->type = NIL;
                entry->item = NULL;
            }
    }

    if (entry->type == UNKNOWN) {
        fprintf(stderr, "Invalid JSON");
        return NULL;
    }

    return entry;
}
