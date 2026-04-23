#include "src/common/hash.h"

#include <stddef.h>
#include <string.h>

// This implement closely follows https://en.wikipedia.org/wiki/MurmurHash#Algorithm

static inline uint32_t murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

uint32_t murmur3_32(const uint8_t *key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    uint32_t k;

    for (size_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);

        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = (h * 5) + 0xe6546b64;
    }

    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    h ^= murmur_32_scramble(k);

    h ^= (uint32_t)len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}


#ifndef MAP_HASH_SEED
#define MAP_HASH_SEED 0x6D656E67
#endif

uint32_t hash_bytes(const void *data, size_t len, uint32_t seed) {
    return murmur3_32((const uint8_t *)data, len, seed);
}

uint32_t hash_string(const char *s, uint32_t seed) {
    return hash_bytes(s, strlen(s), seed);
}

uint32_t map_hash_key(void *key) {
    /*
     * Keys are compared by pointer identity, so hash the pointer value
     * itself. This is stable, fast, and consistent with map_get/map_set.
     */
    return murmur3_32((const uint8_t *)&key, sizeof(void *), MAP_HASH_SEED);
}