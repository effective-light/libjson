#ifndef _HASHTABLE_H_
#define _HASHTABLE_H_

typedef struct entry {
    char *key;
    void *value;
} entry_t;

typedef struct hashtable {
    entry_t *entries;
    size_t size;
    size_t capacity;
} hashtable_t;

void *safe_malloc(size_t);

hashtable_t *hash_init();
// value should be heap allocated
void hash_insert(hashtable_t *, char *, size_t, void *);
// search key must be null terminated
void *hash_search(hashtable_t *, char *);
void hash_destroy(hashtable_t *);

#endif // _HASHTABLE_H_
