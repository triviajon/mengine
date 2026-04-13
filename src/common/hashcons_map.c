#include "src/common/hashcons_map.h"
#include <stdlib.h>
#include <string.h>

#ifndef HCMAP_INITIAL_CAPACITY
#define HCMAP_INITIAL_CAPACITY 1024
#endif
#ifndef HCMAP_LOAD_FACTOR_NUM
#define HCMAP_LOAD_FACTOR_NUM 7
#endif
#ifndef HCMAP_LOAD_FACTOR_DEN
#define HCMAP_LOAD_FACTOR_DEN 10
#endif

typedef struct {
    void *key;
    void *value;
    uint32_t hash;
    int in_use;
} HashconsMapEntry;

struct HashconsMap {
    size_t capacity;
    size_t size;
    HashconsMapEntry *entries;
    hash_fn_t hash_fn;
    eq_fn_t eq_fn;
};

static HashconsMapEntry *hcmap_entries_new(size_t capacity) {
    return calloc(capacity, sizeof(HashconsMapEntry));
}

static bool hcmap_should_grow(HashconsMap *m) {
    return (m->size + 1) * HCMAP_LOAD_FACTOR_DEN > m->capacity * HCMAP_LOAD_FACTOR_NUM;
}

static bool hcmap_resize(HashconsMap *m, size_t new_capacity) {
    HashconsMapEntry *old_entries = m->entries;
    size_t old_capacity = m->capacity;
    HashconsMapEntry *new_entries = hcmap_entries_new(new_capacity);
    if (!new_entries) return false;
    m->entries = new_entries;
    m->capacity = new_capacity;
    m->size = 0;
    for (size_t i = 0; i < old_capacity; i++) {
        HashconsMapEntry *e = &old_entries[i];
        if (!e->in_use) continue;
        size_t idx = e->hash & (new_capacity - 1);
        while (1) {
            HashconsMapEntry *dst = &new_entries[idx];
            if (!dst->in_use) {
                *dst = *e;
                dst->in_use = 1;
                m->size++;
                break;
            }
            idx = (idx + 1) & (new_capacity - 1);
        }
    }
    free(old_entries);
    return true;
}

HashconsMap *hashcons_map_new(hash_fn_t hash_fn, eq_fn_t eq_fn) {
    HashconsMap *m = malloc(sizeof(HashconsMap));
    if (!m) return NULL;
    m->capacity = HCMAP_INITIAL_CAPACITY;
    m->size = 0;
    m->entries = hcmap_entries_new(m->capacity);
    m->hash_fn = hash_fn;
    m->eq_fn = eq_fn;
    return m;
}

void *hashcons_map_get(HashconsMap *m, const void *key) {
    if (!m || m->size == 0) return NULL;
    uint32_t hash = m->hash_fn(key);
    size_t idx = hash & (m->capacity - 1);
    while (1) {
        HashconsMapEntry *e = &m->entries[idx];
        if (!e->in_use) return NULL;
        if (e->hash == hash && m->eq_fn(e->key, key)) return e->value;
        idx = (idx + 1) & (m->capacity - 1);
    }
}

bool hashcons_map_set(HashconsMap *m, void *key, void *value) {
    if (!m) return false;
    if (hcmap_should_grow(m)) {
        if (!hcmap_resize(m, m->capacity * 2)) return false;
    }
    uint32_t hash = m->hash_fn(key);
    size_t idx = hash & (m->capacity - 1);
    while (1) {
        HashconsMapEntry *e = &m->entries[idx];
        if (!e->in_use) {
            e->key = key;
            e->value = value;
            e->hash = hash;
            e->in_use = 1;
            m->size++;
            return true;
        }
        if (e->hash == hash && m->eq_fn(e->key, key)) {
            e->value = value;
            return true;
        }
        idx = (idx + 1) & (m->capacity - 1);
    }
}

void hashcons_map_remove(HashconsMap *m, const void *key) {
    if (!m || m->size == 0) return;
    uint32_t hash = m->hash_fn(key);
    size_t cap = m->capacity;
    size_t idx = hash & (cap - 1);

    // Locate the entry.
    while (1) {
        HashconsMapEntry *e = &m->entries[idx];
        if (!e->in_use) return;  // not found
        if (e->hash == hash && m->eq_fn(e->key, key)) break;
        idx = (idx + 1) & (cap - 1);
    }

    // Backward-shift deletion: clear the slot, then pull forward any entries
    // whose probe chain passes through the now-empty slot.
    m->entries[idx].in_use = 0;
    m->size--;

    size_t empty_slot = idx;
    size_t probe = (idx + 1) & (cap - 1);
    while (m->entries[probe].in_use) {
        size_t nat = m->entries[probe].hash & (cap - 1);
        size_t dist_to_probe = (probe - nat + cap) & (cap - 1);
        size_t dist_to_empty = (empty_slot - nat + cap) & (cap - 1);
        if (dist_to_empty <= dist_to_probe) {
            m->entries[empty_slot] = m->entries[probe];
            m->entries[probe].in_use = 0;
            empty_slot = probe;
        }
        probe = (probe + 1) & (cap - 1);
    }
}

void hashcons_map_free(HashconsMap *m) {
    if (!m) return;
    free(m->entries);
    free(m);
}
