#ifndef HASHCONS_MAP_H
#define HASHCONS_MAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Function pointer types for custom hash and equality
typedef uint32_t (*hash_fn_t)(const void *key);
typedef bool (*eq_fn_t)(const void *a, const void *b);

typedef struct HashconsMap HashconsMap;

HashconsMap *hashcons_map_new(hash_fn_t hash_fn, eq_fn_t eq_fn);
void *hashcons_map_get(HashconsMap *m, const void *key);
bool hashcons_map_set(HashconsMap *m, void *key, void *value);
void hashcons_map_remove(HashconsMap *m, const void *key);
void hashcons_map_free(HashconsMap *m);

#endif // HASHCONS_MAP_H
