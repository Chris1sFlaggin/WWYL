#ifndef HASHMAP_H
#define HASHMAP_H

#define LOAD_FACTOR 0.75

#include <stddef.h>
#include <stdbool.h>

/* Tipi di puntatori a funzione per genericità */

// Funzione per calcolare l'hash di una chiave
typedef size_t (*hash_func_t)(const void *key);

// Funzione per confrontare due chiavi (restituisce 0 se uguali, non-0 altrimenti)
typedef int (*key_cmp_func_t)(const void *key1, const void *key2);

// Struttura opaca per la Hashmap
typedef struct HashMap HashMap;

/**
 * Crea una nuova hashmap.
 * @param initial_capacity Capacità iniziale (es. 16).
 * @param hash_func Funzione di hashing personalizzata.
 * @param cmp_func Funzione di comparazione chiavi personalizzata.
 * @return Puntatore alla nuova HashMap o NULL in caso di errore.
 */
HashMap* hashmap_create(size_t initial_capacity, hash_func_t hash_func, key_cmp_func_t cmp_func);

/**
 * Inserisce o aggiorna un valore nella mappa.
 * @param map La mappa.
 * @param key La chiave (void*).
 * @param value Il valore (void*).
 * @return true se l'inserimento ha avuto successo, false in caso di errore allocazione.
 */
bool hashmap_put(HashMap *map, void *key, void *value);

/**
 * Recupera un valore dalla mappa.
 * @param map La mappa.
 * @param key La chiave da cercare.
 * @return Il valore associato o NULL se non trovato.
 */
void* hashmap_get(HashMap *map, void *key);

/**
 * Rimuove un elemento dalla mappa.
 * @param map La mappa.
 * @param key La chiave da rimuovere.
 * @return Il valore che era associato alla chiave (utile per fare free), o NULL se non trovato.
 */
void* hashmap_remove(HashMap *map, void *key);

/**
 * Restituisce il numero di elementi correnti.
 */
size_t hashmap_size(HashMap *map);

/**
 * Distrugge la mappa e libera la memoria interna (bucket ed entry).
 * NOTA: Non libera la memoria puntata da key o value (spetta all'utente).
 */
void hashmap_destroy(HashMap *map);

#endif