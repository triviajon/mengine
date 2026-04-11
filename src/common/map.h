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

// Deletes the entry with the specified key from the map.
// Returns true on success, false on failure.
bool map_del(Map *m, void *key);

// Free the map structure (does NOT free keys or values)
void map_free(Map *m);

// Clear the map and free values, then the map itself.
// Assumes values were heap-allocated.
void map_clear_free_values(Map *m);

// Clear the map and free keys and values, then the map itself.
// Assumes keys and values were heap-allocated.
void map_clear_free_all(Map *m);

// Apply a function to each value in the map, then free and destroy the map.
void map_clear_apply_free(Map *m, void (*free_fn)(void *));

#endif  // MAP_H
