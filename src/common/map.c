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

typedef struct {
    void *key;
    void *value;
    uint32_t hash;
    int in_use;
} MapEntry;

struct Map {
    size_t capacity;
    size_t size;
    MapEntry *entries;
};

static MapEntry *map_entries_new(size_t capacity) {
    MapEntry *entries = calloc(capacity, sizeof(MapEntry));
    return entries;
}

static bool map_should_grow(Map *m) {
    return (m->size + 1) * MAP_LOAD_FACTOR_DEN > m->capacity * MAP_LOAD_FACTOR_NUM;
}

static bool map_resize(Map *m, size_t new_capacity) {
    // We need to make a new entries array and reinsert in order to resize
    MapEntry *old_entries = m->entries;
    size_t old_capacity = m->capacity;

    MapEntry *new_entries = map_entries_new(new_capacity);
    if (!new_entries) {
        return false;
    }

    m->entries = new_entries;
    m->capacity = new_capacity;
    m->size = 0;

    for (size_t i = 0; i < old_capacity; i++) {
        MapEntry *e = &old_entries[i];
        if (!e->in_use) {
            continue;
        }

        // if we keep new_capacity as a power of two, then this is a fast modulo
        size_t idx = e->hash & (new_capacity - 1);
        while (true) {
            MapEntry *dst = &new_entries[idx];
            if (!dst->in_use) {
                *dst = *e;
                dst->in_use = 1;
                m->size++;
                break;
            }
            // if dst is taken, use open addressing to find a new slot.
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

    // since we're implementing a hash table using open addressing with a linear probe, we need to
    // scan until we have an empty slot (or find the key)
    while (true) {
        MapEntry *e = &m->entries[idx];

        if (!e->in_use) {
            return NULL;
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

    while (true) {
        MapEntry *e = &m->entries[idx];

        if (!e->in_use) {
            e->key = key;
            e->value = value;
            e->hash = hash;
            e->in_use = 1;
            m->size++;
            return true;
        }

        if (e->hash == hash && e->key == key) {
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

        if (!e->in_use) {
            return false;  // we didn't find it
        }

        if (e->hash == hash && e->key == key) {
            // found it
            e->key = 0;
            e->value = 0;
            e->hash = 0;
            e->in_use = 0;
            m->size--;
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

void map_clear_free_values(Map *m) {
    if (!m) {
        return;
    }

    for (size_t i = 0; i < m->capacity; i++) {
        MapEntry *e = &m->entries[i];
        if (e->in_use) {
            free(e->value);
        }
    }

    map_free(m);
}

void map_clear_free_all(Map *m) {
    if (!m) {
        return;
    }

    for (size_t i = 0; i < m->capacity; i++) {
        MapEntry *e = &m->entries[i];
        if (e->in_use) {
            free(e->key);
            free(e->value);
        }
    }

    map_free(m);
}

void map_clear_apply_free(Map *m, void (*free_fn)(void *)) {
    if (!m) {
        return;
    }

    for (size_t i = 0; i < m->capacity; i++) {
        MapEntry *e = &m->entries[i];
        if (e->in_use) {
            free_fn(e->value);
        }
    }

    map_free(m);
}
