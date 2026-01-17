#ifndef USER_H
#define USER_H

#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "hashmap.h"

Block *register_user(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
int user_login(Block *genesis, const char *privkey_hex, const char *pubkey_hex);

// --- STRUTTURA STATO UTENTE (IN MEMORIA) ---
typedef struct {
    char wallet_address[SIGNATURE_LEN];
    int token_balance;
    int best_streak;
    int current_streak;

    int followers_count;
    int following_count;
    int total_posts;
} UserState;

// Inizializza la HashMap globale degli stati utente
void user_state_init(void);

// Ottiene lo stato di un utente (crea se non esiste)
UserState* user_state_get(const char *wallet_address);

// Aggiorna lo stato di un utente nella HashMap
void user_state_update(const char *wallet_address, UserState *state);

// Libera tutta la memoria della HashMap
void user_state_destroy(void);

// Stampa lo stato di un utente (debug)
void print_user_state(const UserState *state);

#endif