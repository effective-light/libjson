#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

#define LOAD_FACTOR 0.75f
#define INITIAL_SIZE 10

static void *safe_calloc(size_t nmemb, size_t size) {
    void *mem = calloc(nmemb, size);

    if (!mem) {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }

    return mem;
}

void *safe_malloc(size_t size) {
    void *mem = malloc(size);

    if (!mem && size) {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }

    return mem;
}

void *safe_realloc(void *ptr, size_t nmemb, size_t size) {
    void *mem = realloc(ptr, nmemb * size);

    if (!mem && nmemb) {
        fprintf(stderr, "out of memory!\n");
        exit(1);
    }

    return mem;
}

hashtable_t *hash_init() {
    hashtable_t *tbl = safe_malloc(sizeof(hashtable_t));
    tbl->capacity = INITIAL_SIZE;
    tbl->size = 0;
    tbl->entries = safe_calloc(tbl->capacity, sizeof(entry_t));

    return tbl;
}

// djb2
static size_t hash(const char *key, size_t capacity, size_t i) {
    size_t hash = 5381;
    int c;

    while (c = *key++) {
        hash = ((hash << 5) + hash) + c;
    }

    return (hash + i) % capacity;
}

static void insert_entry(entry_t *entries, entry_t *entry, size_t capacity) {
    size_t i = 0;
    do {
        size_t j = hash(entry->key, capacity, i);
        entry_t *cur_entry = (entries + j);
        if (!cur_entry->key || !(*(cur_entry->key))) {
            free(cur_entry->key);
            cur_entry->key = entry->key;
            cur_entry->value = entry->value;
            break;
        } else {
            i++;
        }
    } while (i < capacity);
}

static void rehash(hashtable_t *tbl) {
    size_t capacity = 2 * tbl->capacity;
    entry_t *entries = safe_calloc(capacity, sizeof(entry_t));
    for (size_t i = 0; i < tbl->capacity; i++) {
        entry_t *entry = (tbl->entries + i);
        if (entry->key) {
            insert_entry(entries, entry, capacity);
        }
    }

    free(tbl->entries);
    tbl->entries = entries;
    tbl->capacity = capacity;
}

void hash_insert(hashtable_t *tbl, const char *key, size_t key_size,
        void *value) {
    if ((tbl->size / (float) tbl->capacity) > LOAD_FACTOR) {
        rehash(tbl);
    }

    entry_t entry;
    entry.key = safe_malloc((key_size + 1) * sizeof(char));
    strncpy(entry.key, key, key_size);
    entry.key[key_size] = '\0';
    entry.value = value;

    insert_entry(tbl->entries, &entry, tbl->capacity);
    tbl->size++;
}

static entry_t *_hash_search(const hashtable_t *tbl, const char *key) {
    size_t i = 0;
    do {
        size_t j = hash(key, tbl->capacity, i);
        entry_t *entry = (tbl->entries + j);
        if (entry->key && !strcmp(entry->key, key)) {
            return entry;
        }

        i++;
    } while (i < tbl->capacity);

    return NULL;
}

void *hash_search(const hashtable_t *tbl, const char *key) {
    return _hash_search(tbl, key)->value;
}

void *hash_remove(hashtable_t *tbl, const char *key) {
    entry_t *entry = _hash_search(tbl, key);
    entry->key = safe_realloc(entry->key, 1, sizeof(char));
    entry->key = '\0';
    void *tmp = entry->value;
    entry->value = NULL;
    return tmp;
}

void hash_destroy(hashtable_t *tbl) {
    for (size_t i = 0; i < tbl->capacity; i++) {
        entry_t *entry = (tbl->entries + i);
        free(entry->key);
        free(entry->value);
    }

    free(tbl->entries);
    free(tbl);
}
