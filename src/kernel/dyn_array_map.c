#include <string.h>
#include <stdlib.h>
#include "dyn_array_map.h"

/** @brief Allocates and initializes a new map */
Map *map_new(void) {
    Map *map = malloc(sizeof(Map));
	map->size = 0;
	map->items = NULL;
    return map;
}

/** @brief Retrieves a value from the map. Returns null if the key is not valid. 
 * 
 * @param m The map to operate upon.
 * @param key The key of the specified item you wish to retrieve.
 * @return The element at key, or NULL if the key is not valid.
 */
void *map_get(Map *m, void *key) {
    if (key == NULL) {
        return NULL;

    }
    
    for (int i = 0; i < m->size; i++) {
        if ((m->items + i)->key == key) {
            return (m->items + i)->val;
        }
    }

    return NULL;
}

/** @brief Sets the value of a key.
 * 
 * Takes a map, key, and pointer to an item and adds it to the map.
 * 
 * @param m The map to operate upon.
 * @param key The key to access.
 * @param item The value to set for key.
 * @return 1 if the operation succeeded, or 0 if it failed.
 */
int map_set(Map *m, void *key, void *value) {    
    if (key == NULL || value == NULL) {
        return 0;
    }

    for (int i = 0; i < m->size; i++) {
        if ((m->items + i) == key) {
            (m->items + i)->val = value;
            return 1;
        }
    }
    
    m->items = realloc(m->items, sizeof(MapItem) * (m->size + 1));
    (m->items + m->size)->key = key; 
    (m->items + m->size)->val = value; 
    m->size++;

    return 1;
}

/** @brief Deletes the map. 
 * 
 * BEWARE: In addition to freeing all the elements in the map, it also frees the map itself.
 * Do not call free() on the Map* after this function, it does it for you!!
 * 
 * If you do not wish for all the elements in the map to be freed, call map_remove on the elements you do not wish to be freed before calling this function.
 */
void map_free(Map *m) {
    for (int i = 0; i < m->size; i++) {
        free((m->items + i)->key);
        free((m->items + i)->val);
    }
    free(m->items);
    free(m);
}
