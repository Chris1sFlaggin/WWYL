#ifndef USER_H
#define USER_H

#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"

// Configurazione Hashmap
#define STATE_MAP_SIZE 1024 
#define INITIAL_MAP_SIZE 16 
#define REL_MAP_SIZE 2048
#define WELCOME_BONUS 10
#define MAX_CAPACITY_LOAD 0.75

// --- PROTOTIPI FUNZIONI UTENTE ---

// Corretto: Rimosso 'genesis', aggiunto 'user_follow'
Block *register_user(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
int user_login(const char *privkey_hex, const char *pubkey_hex);
Block *user_follow(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_post(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_comment(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);

// --- STRUTTURE STATE MANAGEMENT ---

// Nodo della Hashmap (Bucket Chain)
typedef struct StateNode {
    char wallet_address[SIGNATURE_LEN];
    UserState state;
    struct StateNode *next;
} StateNode;

// Struttura della Hashmap Dinamica
typedef struct {
    StateNode **buckets; // Array di puntatori ai nodi
    int size;            // Numero attuale di bucket (Capacity)
    int count;           // Numero di elementi inseriti (Load)
} StateMap;

// Nodo per tracciare le relazioni (Follow)
typedef struct RelationNode {
    char key[SIGNATURE_LEN * 2]; // "FollowerPubKey:TargetPubKey"
    struct RelationNode *next;
} RelationNode;

// Variabile Globale
extern StateMap world_state;

// --- API STATE MANAGEMENT ---
void state_init();
UserState *state_get_user(const char *wallet_address);
void state_update_user(const char *wallet_address, const UserState *new_state);
void state_add_new_user(const char *wallet_address, const char *username, const char *bio, const char *pic);
void rebuild_state_from_chain(Block *genesis);
void state_cleanup();
int state_check_follow_status(const char *follower, const char *target);
void state_toggle_follow(const char *follower, const char *target);

#endif