#ifndef LINEAR_MAP_H
#define LINEAR_MAP_H

typedef struct {
    void *key;
    void *val;
} LinearMapItem;

typedef struct {
    int size;
    LinearMapItem *items;
} LinearMap;

LinearMap *linear_map_new(void);
void *linear_map_get(LinearMap *m, void *key);
int linear_map_set(LinearMap *m, void *key, void *value);
void linear_map_free(LinearMap *m);
void linear_map_clear_free(LinearMap *m);

#endif /* LINEAR_MAP_H */
