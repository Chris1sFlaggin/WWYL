#include "map.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#define LOAD_FACTOR 0.75

// --- CREAZIONE ---
HashMap *map_create(int initial_size, HashFunc hash, CompareFunc compare, FreeFunc free_key, FreeFunc free_val) {
    HashMap *map = safe_zalloc(sizeof(HashMap));
    map->size = initial_size;
    map->count = 0;
    map->buckets = safe_zalloc(map->size * sizeof(MapEntry*));
    map->hash = hash;
    map->compare = compare;
    map->free_key = free_key;
    map->free_val = free_val;
    return map;
}

// --- RESIZING (INTERNO) ---
static void map_resize(HashMap *map) {
    int old_size = map->size;
    int new_size = old_size * 2;
    MapEntry **new_buckets = safe_zalloc(new_size * sizeof(MapEntry*));

    for (int i = 0; i < old_size; i++) {
        MapEntry *curr = map->buckets[i];
        while (curr) {
            MapEntry *next = curr->next;
            
            // Ricalcola hash per nuova dimensione
            unsigned long h = map->hash(curr->key) % new_size;
            
            // Sposta nodo (senza riallocare)
            curr->next = new_buckets[h];
            new_buckets[h] = curr;
            
            curr = next;
        }
    }
    
    free(map->buckets);
    map->buckets = new_buckets;
    map->size = new_size;
    // printf("[MAP] Resized to %d buckets.\n", new_size); // Debug opzionale
}

// --- INSERIMENTO ---
void map_put(HashMap *map, void *key, void *value) {
    if ((double)map->count / map->size >= LOAD_FACTOR) {
        map_resize(map);
    }

    unsigned long h = map->hash(key) % map->size;
    
    // Cerca se esiste già (Update)
    MapEntry *curr = map->buckets[h];
    while (curr) {
        if (map->compare(curr->key, key) == 0) {
            // Aggiorna valore
            if (map->free_val) map->free_val(curr->value); // Libera vecchio valore
            if (map->free_key) map->free_key(key);         // Libera chiave dupl. passata (non serve più)
            curr->value = value;
            return;
        }
        curr = curr->next;
    }

    // Nuovo inserimento in testa
    MapEntry *node = safe_zalloc(sizeof(MapEntry));
    node->key = key;
    node->value = value;
    node->next = map->buckets[h];
    map->buckets[h] = node;
    map->count++;
}

// --- RECUPERO ---
void *map_get(HashMap *map, const void *key) {
    unsigned long h = map->hash(key) % map->size;
    MapEntry *curr = map->buckets[h];
    while (curr) {
        if (map->compare(curr->key, key) == 0) return curr->value;
        curr = curr->next;
    }
    return NULL;
}

// --- CLEANUP ---
void map_destroy(HashMap *map) {
    if (!map) return;
    
    for (int i = 0; i < map->size; i++) {
        MapEntry *curr = map->buckets[i];
        while (curr) {
            MapEntry *next = curr->next;
            
            // Qui avviene la magia: chiama le funzioni di pulizia custom
            if (map->free_key) map->free_key(curr->key);
            if (map->free_val) map->free_val(curr->value);
            
            free(curr); // Libera il nodo della mappa
            curr = next;
        }
    }
    free(map->buckets);
    free(map);
}

// --- HELPERS ---

unsigned long hash_str(const void *key) {
    const char *str = (const char *)key;
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

int cmp_str(const void *k1, const void *k2) {
    return strcmp((const char*)k1, (const char*)k2);
}

// Hashing diretto del puntatore (per interi castati a void*)
unsigned long hash_int_direct(const void *key) {
    return (unsigned long)(uintptr_t)key;
}

int cmp_int_direct(const void *k1, const void *k2) {
    return (k1 == k2) ? 0 : 1;
}