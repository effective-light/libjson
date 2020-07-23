#ifndef _JSON_H_
#define _JSON_H_

#include <stdbool.h>

#include "hashtable.h"

typedef enum entry_type {
    UNKNOWN,
    NIL,
    BOOL,
    STRING,
    NUMBER,
    ARRAY,
    OBJECT
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
json_entry_t *json_parse(const char *);
// returned value is heap allocated
char *json_stringify(const json_entry_t *, size_t *);
void json_destroy(json_entry_t *);
json_obj_t *get_json_obj(const json_entry_t *);
json_array_t *get_json_array(const json_entry_t *);
// key must be null terminated
json_entry_t *get_json_obj_entry(const json_obj_t *, const char *);
bool get_json_bool(const json_entry_t *);
char *get_json_string(const json_entry_t *);
long double get_json_number(const json_entry_t *);

#endif // _JSON_H_
