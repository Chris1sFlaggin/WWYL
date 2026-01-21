#ifndef USER_H
#define USER_H

#define _GNU_SOURCE  // Add this at the top

#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "map.h"
#include <unistd.h>

#define COSTO_TOKEN_BASE 1 
extern float global_inflation_multiplier; 

// Configurazione Hashmap
#define STATE_MAP_SIZE 1024 
#define INITIAL_MAP_SIZE 16 
#define REL_MAP_SIZE 2048

// --- STRUTTURE STATE MANAGEMENT ---
typedef struct StateNode {
    char wallet_address[SIGNATURE_LEN];
    UserState state;
    struct StateNode *next;
} StateNode;

typedef struct {
    StateNode **buckets; 
    int size; 
    int count;
} StateMap;

typedef struct RelationNode {
    char key[SIGNATURE_LEN * 2]; 
    struct RelationNode *next;
} RelationNode;

extern HashMap *world_state;

// --- API STATE ---
void state_init();
UserState *state_get_user(const char *wallet_address);
void state_update_user(const char *wallet_address, const UserState *new_state);
void state_add_new_user(const char *wallet_address, const char *username, const char *bio, const char *pic);
void rebuild_state_from_chain(Block *genesis);
void state_cleanup();
int state_check_follow_status(const char *follower, const char *target);
void state_toggle_follow(const char *follower, const char *target);

// --- FUNZIONI UTENTE (MINING) ---
Block *register_user(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
int user_login(const char *privkey_hex, const char *pubkey_hex);
Block *user_follow(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_post(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_comment(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_like(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_reveal(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_finalize(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
Block *user_transfer(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);

// --- ECONOMIA ---
void finalize_post_rewards(int post_id);
int mineTokens(long long amount);

#endif