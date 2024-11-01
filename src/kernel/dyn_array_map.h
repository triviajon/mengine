#ifndef DYN_ARRAY_MAP_H
#define DYN_ARRAY_MAP_H

typedef struct {
	void* key;
	void* val;
} MapItem;

typedef struct {
    int size;
    MapItem *items;
} Map;

Map *map_new(void);
void *map_get(Map *m, void *key);
int map_set(Map *m, void *key, void *value);
void map_free(Map *m);

#endif /* DYN_ARRAY_MAP_H */
