#ifndef WWYL_H
#define WWYL_H

#include <stdint.h>
#include <time.h>

// --- COSTANTI DI SICUREZZA ---
#define HASH_LEN 65         // SHA256 hex string + null terminator
#define SIGNATURE_LEN 132   // Firma ECDSA hex
#define MAX_CONTENT_LEN 256 // Limite tweet
#define MAX_NAME_LEN 32

// --- TIPI DI AZIONE (LOGICA COMMIT-REVEAL) ---
typedef enum {
    ACT_REGISTER_USER = 0, // Creazione identità sulla chain
    ACT_POST_CONTENT = 1,  // Creazione Post
    ACT_POST_COMMENT = 2,  // Creazione Commento 
    ACT_VOTE_COMMIT = 3,   // Fase 1: Voto segreto (Hash) 
    ACT_VOTE_REVEAL = 4,    // Fase 2: Voto palese (Like/Dislike + Salt) 
    ACT_FOLLOW_USER = 5    // Segui un altro utente
} ActionType;

// --- STRUTTURE PAYLOAD (I dati specifici per ogni azione) ---
typedef struct {
    char username[32]; // Max 32 caratteri per il nome
    char bio[64];      // Max 64 caratteri per la bio
    char pic_url[128];// URL dell'avatar
} PayloadRegister;

// Payload per quando posti qualcosa
typedef struct {
    char content[MAX_CONTENT_LEN];
} PayloadPost;

// Payload per quando ti impegni a votare (segreto)
typedef struct {
    int target_post_id;       // ID del post che stai votando
    char vote_hash[HASH_LEN]; // Hash(VOTO + SALT)
} PayloadCommit;

// Payload per quando sveli il voto
typedef struct {
    int target_post_id;
    int vote_value;           // 1 = Like, -1 = Dislike
    char salt_secret[32];     // Il segreto usato per generare l'hash
} PayloadReveal;

typedef struct {
    int target_post_id;            // ID del post che stai commentando
    char content[MAX_CONTENT_LEN]; // Contenuto del commento
} PayloadComment;

typedef struct {
    char target_user_pubkey[SIGNATURE_LEN]; // La chiave pubblica dell'utente da seguire
    int follow_action;               // 1 = Segui, -1 = Smetti di seguire
} PayloadFollow;

// --- STRUTTURA BLOCCO / TRANSAZIONE (BLOCKCHAIN) ---
// In questo modello "1 Azione = 1 Blocco" per semplicità
typedef struct Block {
    int index;                // ID progressivo del blocco
    time_t timestamp;
    
    // Header Blockchain (Sicurezza Integrità)
    char prev_hash[HASH_LEN]; // Hash del blocco precedente (La Catena!)
    char curr_hash[HASH_LEN]; // Hash di questo blocco
    
    // Dati Transazione
    ActionType type;
    char sender_pubkey[SIGNATURE_LEN]; // Chi sta agendo (Wallet Address)
    char signature[SIGNATURE_LEN]; // Firma digitale dell'autore (Autenticazione)
    
    // Unione: Un blocco può contenere SOLO UNO di questi payload
    union {
        PayloadPost post;
        PayloadCommit commit;
        PayloadReveal reveal;
        PayloadComment comment;
        PayloadFollow follow;
        PayloadRegister registration;
    } data;

    struct Block *next; // Per la lista concatenata in memoria RAM
} Block;

// --- STRUTTURA STATO UTENTE (IN MEMORIA) ---
// Questo non va su disco/blockchain, è calcolato leggendo i blocchi
typedef struct {
    char wallet_address[SIGNATURE_LEN];
    int token_balance;
    int best_streak;
    int current_streak;

    int followers_count;
    int following_count;
    int total_posts;
} UserState;

Block* initialize_blockchain(void);
void print_block(const Block *block);

#endif