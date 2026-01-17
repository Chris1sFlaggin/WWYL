#ifndef WWYL_H
#define WWYL_H

#include <stdint.h>
#include <time.h>

// --- COSTANTI DI SICUREZZA ---
#define HASH_LEN 65         
#define SIGNATURE_LEN 132   
#define MAX_CONTENT_LEN 256 
#define MAX_NAME_LEN 32
#define INITIAL_POST_MAP_SIZE 128

// --- TIPI DI AZIONE ---
typedef enum {
    ACT_REGISTER_USER = 0, 
    ACT_POST_CONTENT = 1,  
    ACT_POST_COMMENT = 2,  
    ACT_VOTE_COMMIT = 3,   
    ACT_VOTE_REVEAL = 4,   
    ACT_FOLLOW_USER = 5    
} ActionType;

// --- STRUTTURE PAYLOAD ---
typedef struct {
    char username[32]; 
    char bio[64];      
    char pic_url[128]; 
} PayloadRegister;

typedef struct {
    char content[MAX_CONTENT_LEN];
} PayloadPost;

typedef struct {
    int target_post_id;       
    char vote_hash[HASH_LEN]; 
} PayloadCommit;

typedef struct {
    int target_post_id;
    int vote_value;           
    char salt_secret[32];     
} PayloadReveal;

typedef struct {
    int target_post_id;            
    char content[MAX_CONTENT_LEN]; 
} PayloadComment;

typedef struct {
    char target_user_pubkey[SIGNATURE_LEN]; 
} PayloadFollow;

// --- STRUTTURA BLOCCO ---
typedef struct Block {
    int index;                
    time_t timestamp;
    char prev_hash[HASH_LEN]; 
    char curr_hash[HASH_LEN]; 
    ActionType type;
    char sender_pubkey[SIGNATURE_LEN]; 
    char signature[SIGNATURE_LEN]; 
    
    union {
        PayloadPost post;
        PayloadCommit commit;
        PayloadReveal reveal;
        PayloadComment comment;
        PayloadFollow follow;
        PayloadRegister registration;
    } data;

    struct Block *next; 
} Block;

// --- STRUTTURA STATO UTENTE (Aggiornata!) ---
typedef struct {
    char wallet_address[SIGNATURE_LEN];
    // NUOVI CAMPI AGGIUNTI
    char username[32];
    char bio[64];
    char pic_url[128];

    int token_balance;
    int best_streak;
    int current_streak;
    int followers_count;
    int following_count;
    int total_posts;
} UserState;

// === PROTOTIPI ===
Block* initialize_blockchain(void);
void print_block(const Block *block);
Block *mine_new_block(Block *prev_block, ActionType type, const void *payload_data, const char *sender_pubkey, const char *sender_privkey);
int integrity_check(Block *prev, Block *curr); 
void serialize_block_content(const Block *block, char *buffer, size_t size);
void save_blockchain(Block *genesis);
Block *load_blockchain();

// Stato Mutabile del Post (in RAM)
typedef struct {
    int post_id;
    char author_pubkey[SIGNATURE_LEN];
    
    // Contatori per la Game Theory
    int likes;
    int dislikes;
    
    // Stato della scommessa
    int is_open;       // 1 = Voting Active, 0 = Closed/Payout
    time_t created_at;
} PostState;

// Nodo della Hashmap
typedef struct PostStateNode {
    int post_id; // Chiave (Intero invece di stringa!)
    PostState state;
    struct PostStateNode *next;
} PostStateNode;

// Hashmap specifica per i Post
typedef struct {
    PostStateNode **buckets;
    int size;
    int count;
} PostMap;

extern PostMap global_post_index;

// API
void post_index_init();
void post_index_add(int post_id, const char *author);
void post_index_vote(int post_id, int vote_val); // +1 Like, -1 Dislike
PostState *post_index_get(int post_id);
void post_index_cleanup();
int post_index_exists(int post_id);
char *post_index_author(int post_id);

#endif