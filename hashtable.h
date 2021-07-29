/* SPDX-License-Identifier: LGPL-3.0-or-later */

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
void *safe_realloc(void *, size_t, size_t);

hashtable_t *hash_init();
// value should be heap allocated
void hash_insert(hashtable_t *, const char *, size_t, void *);
// search key must be null terminated
void *hash_search(const hashtable_t *, const char *);
void hash_destroy(hashtable_t *);
void *hash_remove(hashtable_t *, const char *);

#endif // _HASHTABLE_H_
