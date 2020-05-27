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

hashtable_t *hash_init();
void hash_insert(hashtable_t *, char *, void *, size_t);
void *hash_search(hashtable_t *, char *);
void hash_destory(hashtable_t *);

#endif // _HASHTABLE_H_
