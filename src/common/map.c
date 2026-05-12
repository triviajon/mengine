#include "src/common/map.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/hash.h"

#ifndef MAP_INITIAL_CAPACITY
#define MAP_INITIAL_CAPACITY 16
#endif
#ifndef MAP_LOAD_FACTOR_NUM
#define MAP_LOAD_FACTOR_NUM 7
#endif
#ifndef MAP_LOAD_FACTOR_DEN
#define MAP_LOAD_FACTOR_DEN 10
#endif

// in_use values: 0 = empty, 1 = occupied, 2 = tombstone (deleted).
// Tombstones are needed for open-addressing correctness: deleting a slot by
// zeroing it breaks probe chains for keys that were displaced past it during
// insertion.  A tombstone tells map_get to keep probing rather than stop.
typedef struct {
    void *key;
    void *value;
    uint32_t hash;
    int in_use;
} MapEntry;

struct Map {
    size_t capacity;
    size_t size;        // occupied entries
    size_t tombstones;  // deleted (tombstone) entries
    MapEntry *entries;
};

static MapEntry *map_entries_new(size_t capacity) { return calloc(capacity, sizeof(MapEntry)); }

// Size+tombstones together determine slot pressure.
static bool map_should_grow(Map *m) {
    return (m->size + m->tombstones + 1) * MAP_LOAD_FACTOR_DEN > m->capacity * MAP_LOAD_FACTOR_NUM;
}

static bool map_resize(Map *m, size_t new_capacity) {
    MapEntry *old_entries = m->entries;
    size_t old_capacity = m->capacity;

    MapEntry *new_entries = map_entries_new(new_capacity);
    if (!new_entries) {
        return false;
    }

    m->entries = new_entries;
    m->capacity = new_capacity;
    m->size = 0;
    m->tombstones = 0;  // tombstones are not copied to the new table

    for (size_t i = 0; i < old_capacity; i++) {
        MapEntry *e = &old_entries[i];
        if (e->in_use != 1) {
            continue;  // skip empty and tombstones
        }

        size_t idx = e->hash & (new_capacity - 1);
        while (true) {
            MapEntry *dst = &new_entries[idx];
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

Map *map_new(void) {
    Map *m = malloc(sizeof(Map));
    if (!m) {
        return NULL;
    }
    m->capacity = MAP_INITIAL_CAPACITY;
    m->size = 0;
    m->tombstones = 0;
    m->entries = map_entries_new(m->capacity);
    if (!m->entries) {
        free(m);
        return NULL;
    }
    return m;
}

Map *map_new_with_capacity(size_t initial_capacity) {
    Map *m = malloc(sizeof(Map));
    if (!m) {
        return NULL;
    }
    m->capacity = initial_capacity;
    m->size = 0;
    m->tombstones = 0;
    m->entries = map_entries_new(m->capacity);
    if (!m->entries) {
        free(m);
        return NULL;
    }
    return m;
}

void *map_get(Map *m, void *key) {
    if (!m || m->size == 0) {
        return NULL;
    }
    uint32_t hash = map_hash_key(key);
    size_t idx = hash & (m->capacity - 1);
    while (true) {
        MapEntry *e = &m->entries[idx];
        if (e->in_use == 0) {
            return NULL;  // empty: stop
        }
        if (e->in_use == 2) {  // tombstone: skip
            idx = (idx + 1) & (m->capacity - 1);
            continue;
        }
        if (e->hash == hash && e->key == key) {
            return e->value;
        }
        idx = (idx + 1) & (m->capacity - 1);
    }
}

bool map_set(Map *m, void *key, void *value) {
    if (!m) {
        return false;
    }
    if (map_should_grow(m)) {
        if (!map_resize(m, m->capacity * 2)) {
            return false;
        }
    }
    uint32_t hash = map_hash_key(key);
    size_t idx = hash & (m->capacity - 1);
    size_t tombstone_idx = (size_t)-1;

    while (true) {
        MapEntry *e = &m->entries[idx];
        if (e->in_use == 0) {
            // Empty slot: key not present. Insert at first tombstone if seen.
            size_t ins = (tombstone_idx != (size_t)-1) ? tombstone_idx : idx;
            MapEntry *slot = &m->entries[ins];
            slot->key = key;
            slot->value = value;
            slot->hash = hash;
            slot->in_use = 1;
            m->size++;
            if (tombstone_idx != (size_t)-1) {
                m->tombstones--;
            }
            return true;
        }
        if (e->in_use == 2) {
            // Tombstone: remember first one, keep probing for existing key.
            if (tombstone_idx == (size_t)-1) {
                tombstone_idx = idx;
            }
        } else if (e->hash == hash && e->key == key) {
            e->value = value;
            return true;
        }
        idx = (idx + 1) & (m->capacity - 1);
    }
}

bool map_del(Map *m, void *key) {
    if (!m) {
        return false;
    }
    uint32_t hash = map_hash_key(key);
    size_t idx = hash & (m->capacity - 1);
    while (true) {
        MapEntry *e = &m->entries[idx];
        if (e->in_use == 0) {
            return false;  // empty: not found
        }
        if (e->in_use == 2) {  // tombstone: skip
            idx = (idx + 1) & (m->capacity - 1);
            continue;
        }
        if (e->hash == hash && e->key == key) {
            e->in_use = 2;  // tombstone
            e->key = NULL;
            e->value = NULL;
            m->size--;
            m->tombstones++;
            return true;
        }
        idx = (idx + 1) & (m->capacity - 1);
    }
}

void map_free(Map *m) {
    if (!m) {
        return;
    }
    free(m->entries);
    free(m);
}

void map_reset(Map *m) {
    if (!m || (m->size == 0 && m->tombstones == 0)) {
        return;
    }
    memset(m->entries, 0, m->capacity * sizeof(MapEntry));
    m->size = 0;
    m->tombstones = 0;
}

void map_clear_free_values(Map *m) {
    if (!m) {
        return;
    }
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].in_use == 1) {
            free(m->entries[i].value);
        }
    }
    map_free(m);
}

void map_clear_free_all(Map *m) {
    if (!m) {
        return;
    }
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].in_use == 1) {
            free(m->entries[i].key);
            free(m->entries[i].value);
        }
    }
    map_free(m);
}

void map_clear_apply_free(Map *m, void (*free_fn)(void *)) {
    if (!m) {
        return;
    }
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].in_use == 1) {
            free_fn(m->entries[i].value);
        }
    }
    map_free(m);
}

void map_for_each(Map *m, void (*fn)(void *key, void *value, void *ud), void *ud) {
    if (!m) {
        return;
    }
    for (size_t i = 0; i < m->capacity; i++) {
        if (m->entries[i].in_use == 1) {
            fn(m->entries[i].key, m->entries[i].value, ud);
        }
    }
}
