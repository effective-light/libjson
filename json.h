#ifndef _JSON_H_
#define _JSON_H_

#include "hashtable.h"

typedef enum entry_type {
    STRING,
    NUMBER,
    OBJECT,
    ARRAY,
    BOOL,
    NIL,
    UNKNOWN
} entry_type;

typedef struct json_entry {
    void *item;
    entry_type type;
} json_entry_t;

typedef struct json_obj {
    hashtable_t *tbl;
} json_obj_t;

typedef struct json_array {
    json_entry_t *entries;
    size_t size;
} json_array_t;

void json_init();
// string must be null terminated
json_entry_t *parse_json(char *, size_t);

#endif // _JSON_H_
