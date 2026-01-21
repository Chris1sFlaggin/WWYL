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

char *getRandomWord(void) {
    char *path = "words.txt";
    FILE *file = fopen(path, "r");
    if (!file) {
        fatal_error("Could not open words file: %s", path);
    }

    // 1. Conta le linee totali (Primo passaggio)
    unsigned int line_count = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file)) {
        line_count++;
    }

    if (line_count == 0) {
        fclose(file);
        fatal_error("Words file is empty: %s", path);
    }

    // 2. Genera un indice crittograficamente sicuro [0, line_count - 1]
    unsigned int random_index = 0;
    unsigned int range_max = UINT_MAX;
    
    // Calcoliamo il limite per evitare il "Modulo Bias"
    // (Se UINT_MAX non è divisibile perfettamente per line_count, i primi numeri uscirebbero più spesso)
    unsigned int secure_limit = range_max - (range_max % line_count);

    do {
        if (RAND_bytes((unsigned char*)&random_index, sizeof(random_index)) != 1) {
             fclose(file);
             fatal_error("RAND_bytes failed during word selection.");
        }
    } while (random_index >= secure_limit); // Scarta i numeri fuori range (Rejection Sampling)
    
    random_index %= line_count; // Ora il modulo è sicuro e uniforme

    // 3. Recupera la parola corrispondente all'indice (Secondo passaggio)
    rewind(file);
    unsigned int current = 0;
    char *selected_word = NULL;
    
    while (fgets(buffer, sizeof(buffer), file)) {
        if (current == random_index) {
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len - 1] == '\n') {
                buffer[len - 1] = '\0'; // Rimuove newline
            }
            selected_word = strdup(buffer);
            break;
        }
        current++;
    }

    fclose(file);
    
    if (!selected_word) {
        fatal_error("Error retrieving word at index %u", random_index);
    }
    
    return selected_word;
}