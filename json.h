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

typedef hashtable_t json_obj_t;

typedef struct json_array {
    json_entry_t *entries;
    size_t size;
} json_array_t;

// string must be null terminated, also the returned value is heap allocated
json_entry_t *json_parse(char *, size_t);
// returned value is heap allocated
char *json_stringify(json_entry_t *);
void json_destroy(json_entry_t *);
json_obj_t *get_json_obj(json_entry_t *);
json_array_t *get_json_array(json_entry_t *);
// key must be null terminated
json_entry_t *get_json_obj_entry(json_obj_t *, char *);

#endif // _JSON_H_
