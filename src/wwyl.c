#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "wwyl_config.h" 

// ---------------------------------------------------------
// HELPER: Serializzazione per Hashing
// ---------------------------------------------------------
// Per calcolare l'hash di un blocco, dobbiamo trasformare i suoi campi 
// in un'unica stringa. Questa funzione prepara il buffer.
void serialize_block_content(const Block *block, char *buffer, size_t size) {
    // Includiamo Index, PrevHash, Sender, Type, Timestamp, Payload
    
    char payload_str[MAX_CONTENT_LEN + 20]; 
    
    switch (block->type) {
        case ACT_POST_CONTENT:
            snprintf(payload_str, sizeof(payload_str), "%s", block->data.post.content);
            break;
        case ACT_REGISTER_USER:
            snprintf(payload_str, sizeof(payload_str), "REGISTRATION");
            break;
        case ACT_POST_COMMENT:
            snprintf(payload_str, sizeof(payload_str), "%d:%s",
                     block->data.comment.target_post_id,
                     block->data.comment.content);
            break;
        case ACT_VOTE_COMMIT:
            snprintf(payload_str, sizeof(payload_str), "%d:%s",
                     block->data.commit.target_post_id,
                     block->data.commit.vote_hash);
            break;
        case ACT_VOTE_REVEAL:
            snprintf(payload_str, sizeof(payload_str), "%d:%d:%s",      
                     block->data.reveal.target_post_id,
                     block->data.reveal.vote_value,
                     block->data.reveal.salt_secret);
            break;
        case ACT_FOLLOW_USER:
            snprintf(payload_str, sizeof(payload_str), "%s:%d",
                     block->data.follow.target_user_pubkey,
                     block->data.follow.follow_action);
            break;
        default:
            snprintf(payload_str, sizeof(payload_str), "UNKNOWN");
            break;
    }

    // Formato: INDEX:TIMESTAMP:PREV_HASH:SENDER:TYPE:PAYLOAD
    int len = snprintf(buffer, size, "%u:%ld:%s:%s:%d:%s",
             block->index,
             block->timestamp,
             block->prev_hash,
             block->sender_pubkey,
             block->type,
             payload_str);
             
    if (len >= size) {
        fatal_error("Block serialization buffer overflow!");
    }
}

// ---------------------------------------------------------
// CREAZIONE DEL BLOCCO GENESI (Crypto-Enabled)
// ---------------------------------------------------------
Block *create_genesis_block() {
    // Allocazione
    Block *block = (Block *)safe_zalloc(sizeof(Block));

    // Metadati
    block->index = 0;
    block->timestamp = time(NULL); // O metti un timestamp fisso se vuoi che l'hash sia eterno
    
    // AZIONE: REGISTRAZIONE UTENTE (Il tuo profilo)
    block->type = ACT_REGISTER_USER;

    // Prev Hash a zero
    memset(block->prev_hash, '0', 64);
    block->prev_hash[64] = '\0';

    // Imposto il mittente
    // Usiamo la costante hardcodata
    memcpy(block->sender_pubkey, GOD_PUB_KEY, SIGNATURE_LEN);

    // Payload: Dato che è una REGISTRAZIONE, uso il campo Post 
    // per mettere una "Bio" o lasciarlo vuoto, a seconda di come hai definito ACT_REGISTER_USER.
    // Assumiamo che per la registrazione usiamo il payload per un messaggio di benvenuto o bio.
    const char *my_bio = "@Chris1sflaggin - Hello from the founder of WWYL!";
    
    if (snprintf(block->data.post.content, MAX_CONTENT_LEN, "%s", my_bio) >= MAX_CONTENT_LEN) {
        fprintf(stderr, "[WARN] Bio troncata.\n");
    }

    // Serializzazione & Hashing
    char raw_data_buffer[2048];
    serialize_block_content(block, raw_data_buffer, sizeof(raw_data_buffer));
    
    // Calcola Hash del blocco (Questo sarà l'ID del blocco 0)
    sha256_hash(raw_data_buffer, strlen(raw_data_buffer), block->curr_hash);

    // FIRMA DIGITALE (Proof of Authority)
    // Firmo il blocco con la mia chiave privata hardcodata.
    ecdsa_sign(GOD_PRIV_KEY, block->curr_hash, block->signature);

    printf("[GENESIS] Profile Created for: %.16s...\n", block->sender_pubkey);
    printf("[GENESIS] Signature: %.16s...\n", block->signature);

    return block;
}
// ---------------------------------------------------------
// INIZIALIZZAZIONE BLOCKCHAIN
// ---------------------------------------------------------
Block *initialize_blockchain() {
    Block *genesis = create_genesis_block();
    printf("[INFO] Blockchain inizializzata. Genesis Address: %p\n", (void*)genesis);
    return genesis;
}

// ---------------------------------------------------------
// FUNZIONI DI DEBUG
// ---------------------------------------------------------
void print_block(const Block *block) {
    if (!block) {
        printf("[DEBUG] Block is NULL\n");
        return;
    }
    printf("=== Block #%d Details ===\n", block->index);
    printf("# Timestamp: %ld\n", block->timestamp);
    printf("# Action Type: %d\n", block->type);
    printf("# Sender PubKey: %s\n", block->sender_pubkey);
    printf("# Signature: %s\n", block->signature);
    printf("# Previous Hash: %s\n", block->prev_hash);
    printf("# Current Hash: %s\n", block->curr_hash);
    printf("# Sender PubKey: %s\n", block->sender_pubkey);
    switch (block->type) {
        case ACT_REGISTER_USER:
            printf("# User Registration\n");
            break;
        case ACT_POST_CONTENT:
            printf("# Post Content: %s\n", block->data.post.content);
            break;
        case ACT_POST_COMMENT:
            printf("# Comment on Post ID %d: %s\n", 
                   block->data.comment.target_post_id, 
                   block->data.comment.content);
            break;
        case ACT_VOTE_COMMIT:
            printf("# Vote Commit on Post ID %d, Hash: %s\n", 
                   block->data.commit.target_post_id, 
                   block->data.commit.vote_hash);
            break;
        case ACT_VOTE_REVEAL:
            printf("# Vote Reveal on Post ID %d, Value: %d, Salt: %s\n", 
                   block->data.reveal.target_post_id, 
                   block->data.reveal.vote_value, 
                   block->data.reveal.salt_secret);
            break;
        case ACT_FOLLOW_USER:
            printf("# Follow Action: %s user %s\n", 
                   (block->data.follow.follow_action == 1) ? "Follow" : "Unfollow",
                   block->data.follow.target_user_pubkey);
            break;
        default:
            printf("# Unknown action type\n");
            break;
    }
    printf("=========================\n");
}

