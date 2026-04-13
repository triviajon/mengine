#ifndef HASH_H
#define HASH_H


#include <stdint.h>
#include <stddef.h>


uint32_t map_hash_key(void *key);
uint32_t hash_bytes(const void *data, size_t len, uint32_t seed);
uint32_t hash_string(const char *s, uint32_t seed);

#endif  // HASH_H