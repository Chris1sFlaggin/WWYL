#include "hashmap.h"
#include <stdlib.h>
#include <stdio.h>

// Struttura interna per un nodo (Entry)
typedef struct Entry {
    void *key;
    void *value;
    struct Entry *next;
} Entry;

// Definizione della struttura HashMap
struct HashMap {
    Entry **buckets;       // Array di puntatori a Entry
    size_t size;           // Numero totale di elementi
    size_t capacity;       // Dimensione dell'array buckets
    size_t threshold;      // Soglia per il resize (capacity * 0.75)
    hash_func_t hash_func; // Funzione hash utente
    key_cmp_func_t cmp_func; // Funzione compare utente
};

HashMap* hashmap_create(size_t initial_capacity, hash_func_t hash_func, key_cmp_func_t cmp_func) {
    if (initial_capacity == 0) initial_capacity = 16;

    HashMap *map = malloc(sizeof(HashMap));
    if (!map) return NULL;

    map->buckets = calloc(initial_capacity, sizeof(Entry*));
    if (!map->buckets) {
        free(map);
        return NULL;
    }

    map->size = 0;
    map->capacity = initial_capacity;
    map->threshold = (size_t)(initial_capacity * LOAD_FACTOR);
    map->hash_func = hash_func;
    map->cmp_func = cmp_func;

    return map;
}

// Funzione interna per ridimensionare la mappa
static bool hashmap_resize(HashMap *map) {
    size_t new_capacity = map->capacity * 2;
    Entry **new_buckets = calloc(new_capacity, sizeof(Entry*));
    
    if (!new_buckets) return false; // Fallimento allocazione

    // Rehash di tutti gli elementi
    for (size_t i = 0; i < map->capacity; i++) {
        Entry *entry = map->buckets[i];
        while (entry) {
            Entry *next = entry->next; // Salva il prossimo
            
            // Calcola nuovo indice
            size_t new_index = map->hash_func(entry->key) % new_capacity;
            
            // Inserimento in testa nel nuovo bucket
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;

            entry = next;
        }
    }

    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    map->threshold = (size_t)(new_capacity * LOAD_FACTOR);
    
    return true;
}

bool hashmap_put(HashMap *map, void *key, void *value) {
    // 1. Controlla se serve il resize PRIMA di inserire
    if (map->size >= map->threshold) {
        if (!hashmap_resize(map)) {
            return false; // Resize fallito
        }
    }

    size_t index = map->hash_func(key) % map->capacity;
    Entry *entry = map->buckets[index];

    // 2. Cerca se la chiave esiste già (aggiornamento)
    while (entry) {
        if (map->cmp_func(entry->key, key) == 0) {
            entry->value = value; // Aggiorna valore
            return true;
        }
        entry = entry->next;
    }

    // 3. Crea nuova entry
    Entry *new_entry = malloc(sizeof(Entry));
    if (!new_entry) return false;

    new_entry->key = key;
    new_entry->value = value;
    
    // Inserimento in testa (più veloce)
    new_entry->next = map->buckets[index];
    map->buckets[index] = new_entry;
    
    map->size++;
    return true;
}

void* hashmap_get(HashMap *map, void *key) {
    size_t index = map->hash_func(key) % map->capacity;
    Entry *entry = map->buckets[index];

    while (entry) {
        if (map->cmp_func(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

void* hashmap_remove(HashMap *map, void *key) {
    size_t index = map->hash_func(key) % map->capacity;
    Entry *entry = map->buckets[index];
    Entry *prev = NULL;

    while (entry) {
        if (map->cmp_func(entry->key, key) == 0) {
            void *removed_val = entry->value;

            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }

            free(entry);
            map->size--;
            return removed_val;
        }
        prev = entry;
        entry = entry->next;
    }
    return NULL;
}

size_t hashmap_size(HashMap *map) {
    return map->size;
}

void hashmap_destroy(HashMap *map) {
    for (size_t i = 0; i < map->capacity; i++) {
        Entry *entry = map->buckets[i];
        while (entry) {
            Entry *next = entry->next;
            free(entry); // Libera solo la struttura Entry, non key/value
            entry = next;
        }
    }
    free(map->buckets);
    free(map);
}