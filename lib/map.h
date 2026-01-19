#ifndef MAP_H
#define MAP_H

#include <stddef.h>

// Nodo generico della Mappa
typedef struct MapEntry {
    void *key;
    void *value;
    struct MapEntry *next;
} MapEntry;

// Tipi di funzione per la personalizzazione
typedef unsigned long (*HashFunc)(const void *key);
typedef int (*CompareFunc)(const void *key1, const void *key2);
typedef void (*FreeFunc)(void *data); // <--- Callback per liberare la memoria

// Struttura Hashmap
typedef struct {
    MapEntry **buckets;
    int size;
    int count;
    HashFunc hash;
    CompareFunc compare;
    FreeFunc free_key;   // Come liberare la chiave (es. free per stringhe duplicate)
    FreeFunc free_val;   // Come liberare il valore (es. custom function per struct complesse)
} HashMap;

// API
HashMap *map_create(int initial_size, HashFunc hash, CompareFunc compare, FreeFunc free_key, FreeFunc free_val);
void map_put(HashMap *map, void *key, void *value);
void *map_get(HashMap *map, const void *key);
void map_destroy(HashMap *map);

// Helpers pronti all'uso
unsigned long hash_str(const void *key);
int cmp_str(const void *k1, const void *k2);

unsigned long hash_int_direct(const void *key);
int cmp_int_direct(const void *k1, const void *k2);

#endif