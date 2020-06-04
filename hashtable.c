#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"

#define LOAD_FACTOR 0.75f
#define INITIAL_SIZE 10

static entry_t *init_entries(size_t capacity) {
    entry_t *entries = calloc(sizeof(entry_t), capacity);
    for (size_t i = 0; i < capacity; i++) {
        entry_t *entry = (entries + i);
        entry->key = NULL;
        entry->value = NULL;
    }

    return entries;
}

hashtable_t *hash_init() {
    hashtable_t *tbl = malloc(sizeof(hashtable_t));
    tbl->capacity = INITIAL_SIZE;
    tbl->size = 0;
    tbl->entries = init_entries(tbl->capacity);

    return tbl;
}

// djb2
static size_t hash(char *key, size_t capacity, size_t i) {
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
        if (!cur_entry->key) {
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
    entry_t *entries = init_entries(capacity);
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

void hash_insert(hashtable_t *tbl, char *key, size_t key_size, void *value) {
    if ((tbl->size / (float) tbl->capacity) > LOAD_FACTOR) {
        rehash(tbl);
    }

    entry_t entry;
    entry.key = malloc(key_size + 1);
    strncpy(entry.key, key, key_size);
    entry.key[key_size] = '\0';
    entry.value = value;

    insert_entry(tbl->entries, &entry, tbl->capacity);
    tbl->size++;
}


void *hash_search(hashtable_t *tbl, char *key) {
    size_t i = 0;
    do {
        size_t j = hash(key, tbl->capacity, i);
        entry_t *entry = (tbl->entries + j);
        if (entry->key && !strcmp(entry->key, key)) {
            return entry->value;
        }

        i++;
    } while (i < tbl->capacity);

    return NULL;
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
