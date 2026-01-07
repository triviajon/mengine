#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct Map Map;

// Create a new empty map
Map *map_new(void);

// Retrieve the value associated with key, or NULL if not present
void *map_get(Map *m, void *key);

// Insert or update key → value.
// Returns true on success, false on failure.
bool map_set(Map *m, void *key, void *value);

// Free the map structure (does NOT free keys or values)
void map_free(Map *m);

// Clear the map and free keys and values.
// Assumes keys and values were heap-allocated.
void map_clear_free(Map *m);

#endif  // MAP_H
