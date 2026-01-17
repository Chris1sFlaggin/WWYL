#include "utils.h"

// ---------------------------------------------------------
// GESTIONE ERRORI AVANZATA
// ---------------------------------------------------------
// Permette messaggi formattati e centralizza l'uscita dal programma.
void fatal_error(const char *fmt, ...) {
    va_list args;
    fprintf(stderr, "[FATAL ERROR] "); 
    
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);       
    va_end(args);
    
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

// ---------------------------------------------------------
// MEMORY ALLOCATOR SICURO (Il vero "Flex")
// ---------------------------------------------------------
// PERCHÉ È SICURO:
// 1. Calloc azzera TUTTA la memoria, incluso il "padding" della struct
//    che i cicli for manuali non toccano. Evita data leak (Heap Inspection).
// 2. Controllo NULL se la RAM è finita.
void *safe_zalloc(size_t size) {
    void *ptr = calloc(1, size);
    if (!ptr) {
        // Se calloc fallisce, usiamo la nostra funzione di errore
        fatal_error("Out of memory! Failed to allocate %zu bytes.", size);
    }
    return ptr;
}

void errExit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}