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

// --- ECONOMIA ---
#define COSTO_VOTO 2
#define COSTO_POST 4
#define WELCOME_BONUS 10
#define GLOBAL_TOKEN_LIMIT 200000 
#define MAX_CAPACITY_LOAD 0.75

#define WALLET_FILE "wwyl.wallet"

// --- TIPI DI AZIONE ---
typedef enum {
    ACT_REGISTER_USER = 0, 
    ACT_POST_CONTENT = 1,  
    ACT_POST_COMMENT = 2,  
    ACT_VOTE_COMMIT = 3,   
    ACT_VOTE_REVEAL = 4,   
    ACT_FOLLOW_USER = 5,
    ACT_POST_FINALIZE = 6,
    ACT_TRANSFER = 7
} ActionType;

// --- STRUTTURE PAYLOAD (Dati su Disco) ---
typedef struct {
    char username[32]; 
    char bio[64];      
    char pic_url[128]; 
} PayloadRegister;

typedef struct {
    char target_pubkey[SIGNATURE_LEN];
    int amount;
} PayloadTransfer;

typedef struct {
    int target_post_id;
} PayloadFinalize;

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
        PayloadFinalize finalize;
        PayloadTransfer transfer;
    } data;

    struct Block *next; 
} Block;

// --- STRUTTURA STATO UTENTE (RAM) ---
typedef struct {
    char wallet_address[SIGNATURE_LEN];
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

typedef struct {
    char username[32];
    char priv[SIGNATURE_LEN];
    char pub[SIGNATURE_LEN];
    int registered; // 1 se l'utente è già registrato sulla blockchain
} WalletEntry;

typedef struct {
    WalletEntry entries[10]; // Supportiamo fino a 10 profili locali
    int count;
} WalletStore;

// --- STRUTTURE POST STATE (RAM) ---

// Nodo per i voti segreti (Commit)
typedef struct CommitNode {
    char voter_pubkey[SIGNATURE_LEN];
    char vote_hash[HASH_LEN];
    struct CommitNode *next;
} CommitNode;

// Nodo per i voti svelati (Reveal)
typedef struct RevealNode {
    char voter_pubkey[SIGNATURE_LEN];
    int vote_value;
    struct RevealNode *next;
} RevealNode;

typedef struct CommentNode {
    char author_pubkey[SIGNATURE_LEN];
    char content[MAX_CONTENT_LEN];
    time_t timestamp;
    struct CommentNode *next;
} CommentNode;

// Stato Mutabile del Post
typedef struct {
    int post_id;
    char author_pubkey[SIGNATURE_LEN];
    
    int likes;
    int dislikes;
    
    CommitNode *commits; // Lista chi ha committato
    RevealNode *reveals; // Lista chi ha rivelato
    CommentNode *comments; // Lista commenti
    
    int pull;       // Il piatto (Token)
    int is_open;    // Scommessa aperta
    int finalized;  // 1 se pagato
    time_t created_at;
} PostState;

// --- PROTOTIPI GLOBALI ---
Block* initialize_blockchain(void);
void print_block(const Block *block);
Block *mine_new_block(Block *prev_block, ActionType type, const void *payload_data, const char *sender_pubkey, const char *sender_privkey);
int integrity_check(Block *prev, Block *curr); 
void serialize_block_content(const Block *block, char *buffer, size_t size);
void save_blockchain(Block *genesis);
Block *load_blockchain();

#endif