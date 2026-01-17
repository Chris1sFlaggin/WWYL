#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "wwyl_config.h" 
#include "user.h"

// ---------------------------------------------------------
// OTTIENI ID BLOCCO
// ---------------------------------------------------------
int getBlockId(Block block) {
    return block.index;
}

// ---------------------------------------------------------
// VERIFICA INTEGRITÀ LINK TRA DUE BLOCCHI
// ---------------------------------------------------------
int integrity_check(Block *prev, Block *curr) {
    if (strcmp(curr->prev_hash, prev->curr_hash) != 0) {
        fprintf(stderr, "[ALERT] BROKEN CHAIN at Block #%d!\n", curr->index);
        fprintf(stderr, "        Expected Prev: %s\n", prev->curr_hash);
        fprintf(stderr, "        Found Prev:    %s\n", curr->prev_hash);
        return 0; // Fail
    }
    return 1; // Success
}

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
            snprintf(payload_str, sizeof(payload_str), "%s:%s:%s", 
                     block->data.registration.username, 
                     block->data.registration.bio,
                     block->data.registration.pic_url);
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
             
    if (len < 0 || (size_t)len >= size) {
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
    if (snprintf(block->sender_pubkey, sizeof(block->sender_pubkey), "%s", GOD_PUB_KEY) >= (int)sizeof(block->sender_pubkey)) {
        fprintf(stderr, "[WARN] Genesis Public Key troncata (buffer troppo piccolo)!\n");
    }

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
    switch (block->type) {
        case ACT_REGISTER_USER:
            printf("# Registration : %s: %s: %s\n", 
            block->data.registration.username, 
            block->data.registration.bio,
            block->data.registration.pic_url);
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

Block *mine_new_block(Block *prev_block, ActionType type, const void *payload_data, const char *sender_pubkey, const char *sender_privkey) {
    // Validazione input base
    if (!prev_block) { fatal_error("Previous block is NULL!"); }
    if (!payload_data && type != ACT_REGISTER_USER) { 
        // ACT_REGISTER_USER potrebbe non avere payload se è il genesis, 
        // ma per blocchi normali ci aspettiamo dati.
        fatal_error("Payload data is NULL!"); 
    }

    // Allocazione
    Block *new_block = (Block *)safe_zalloc(sizeof(Block));

    // Metadati e Collegamento
    // [MIGLIORAMENTO] L'indice è sempre quello precedente + 1
    new_block->index = prev_block->index + 1;
    new_block->timestamp = time(NULL);

    // [SICUREZZA] snprintf al posto di strncpy per l'hash precedente
    snprintf(new_block->prev_hash, sizeof(new_block->prev_hash), "%s", prev_block->curr_hash);

    // Gestione Payload (con snprintf per sicurezza)
    new_block->type = type;

    switch (type) {
    case ACT_POST_CONTENT:
        snprintf(new_block->data.post.content, MAX_CONTENT_LEN, "%s", 
                 ((PayloadPost *)payload_data)->content);
        break;

    case ACT_POST_COMMENT:
        new_block->data.comment.target_post_id = ((PayloadComment *)payload_data)->target_post_id;
        snprintf(new_block->data.comment.content, MAX_CONTENT_LEN, "%s", 
                 ((PayloadComment *)payload_data)->content);
        break;

    case ACT_VOTE_COMMIT:
        new_block->data.commit.target_post_id = ((PayloadCommit *)payload_data)->target_post_id;
        snprintf(new_block->data.commit.vote_hash, HASH_LEN, "%s", 
                 ((PayloadCommit *)payload_data)->vote_hash);
        break;

    case ACT_VOTE_REVEAL:
        new_block->data.reveal.target_post_id = ((PayloadReveal *)payload_data)->target_post_id;
        new_block->data.reveal.vote_value = ((PayloadReveal *)payload_data)->vote_value;
        // Salt secret è fisso a 32 byte o meno? Meglio usare sizeof
        snprintf(new_block->data.reveal.salt_secret, sizeof(new_block->data.reveal.salt_secret), "%s", 
                 ((PayloadReveal *)payload_data)->salt_secret);
        break;

    case ACT_FOLLOW_USER:
        snprintf(new_block->data.follow.target_user_pubkey, SIGNATURE_LEN, "%s", 
                 ((PayloadFollow *)payload_data)->target_user_pubkey);
        new_block->data.follow.follow_action = ((PayloadFollow *)payload_data)->follow_action;
        break;

    case ACT_REGISTER_USER:
        snprintf(new_block->data.registration.username, 32, "%s", 
                 ((PayloadRegister *)payload_data)->username);
        snprintf(new_block->data.registration.bio, 64, "%s", 
                 ((PayloadRegister *)payload_data)->bio);
        break;
        
    default:
        printf("[WARN] Unknown block type during mining.\n");
        break;
    }

    // Imposto il mittente 
    if (snprintf(new_block->sender_pubkey, sizeof(new_block->sender_pubkey), "%s", sender_pubkey) >= (int)sizeof(new_block->sender_pubkey)) {
        fprintf(stderr, "[WARN] Sender Public Key troncata!\n");
    }

    // Mining (HASHING)
    char raw_data_buffer[2048];
    serialize_block_content(new_block, raw_data_buffer, sizeof(raw_data_buffer));
    sha256_hash(raw_data_buffer, strlen(raw_data_buffer), new_block->curr_hash);

    // FIRMA (Proof of Authority / Identity)
    ecdsa_sign(sender_privkey, new_block->curr_hash, new_block->signature);

    // Aggiornamento catena
    prev_block->next = new_block;

    printf("[MINED] Block #%d (Type: %d) mined by %.10s...\n", new_block->index, new_block->type, new_block->sender_pubkey);

    // Sanity Check finale
    if (!integrity_check(prev_block, new_block)) {
        prev_block->next = NULL; // Scolleghiamo il blocco corrotto
        free(new_block);         // Liberiamo la memoria
        return NULL;             // Segnaliamo errore
    }

    return new_block;
}

// ---------------------------------------------------------
// VERIFICA INTEGRITÀ BLOCKCHAIN
// ---------------------------------------------------------
int verifyFullChain(Block *genesis) {
    if (!genesis) return 0; // Catena vuota o nulla

    Block *prev = genesis;
    Block *curr = genesis->next;
    char temp_hash[HASH_LEN + 1];
    char raw_buffer[2048];
    int count = 1;

    printf("\n[SECURITY] Avvio verifica integrità blockchain...\n");

    // Loop su tutti i blocchi successivi al Genesi
    while (curr != NULL) {
        
        if (integrity_check(prev, curr) == 0) {
            return 0; // Fail
        }

        serialize_block_content(curr, raw_buffer, sizeof(raw_buffer));
        sha256_hash(raw_buffer, strlen(raw_buffer), temp_hash);

        if (strcmp(temp_hash, curr->curr_hash) != 0) {
            fprintf(stderr, "[ALERT] DATA TAMPERING at Block #%d!\n", curr->index);
            fprintf(stderr, "        Stored Hash:      %s\n", curr->curr_hash);
            fprintf(stderr, "        Calculated Hash:  %s\n", temp_hash);
            return 0; // Fail
        }

        prev = curr;
        curr = curr->next;
        count++;
    }

    printf("[SECURITY] Chain Verified. %d blocks checked. Status: SECURE.\n", count);
    return 1; 
}

// ---------------------------------------------------------
// PERSISTENZA: SALVATAGGIO SU FILE
// ---------------------------------------------------------
void save_blockchain(Block *genesis) {
    FILE *f = fopen("wwyl_chain.dat", "wb"); // Write Binary
    if (!f) {
        perror("[ERR] Cannot save blockchain");
        return;
    }

    Block *curr = genesis;
    int count = 0;
    while (curr != NULL) {
        // Scriviamo la struttura del blocco grezza su disco
        fwrite(curr, sizeof(Block), 1, f);
        curr = curr->next;
        count++;
    }

    fclose(f);
    printf("[DISK] Blockchain saved! (%d blocks written)\n", count);
}

// ---------------------------------------------------------
// PERSISTENZA: CARICAMENTO SICURO (Il 3° Controllo!)
// ---------------------------------------------------------
Block *load_blockchain() {
    FILE *f = fopen("wwyl_chain.dat", "rb"); // Read Binary
    if (!f) {
        printf("[INFO] Nessuna blockchain trovata su disco. Creazione Genesi...\n");
        return initialize_blockchain(); // Se non c'è file, partiamo da zero
    }

    printf("[DISK] Loading blockchain from file...\n");

    Block *root = NULL;
    Block *prev = NULL;
    Block *curr = NULL;
    int count = 0;

    while (1) {
        curr = (Block *)safe_zalloc(sizeof(Block));
        
        // Leggiamo un blocco alla volta
        if (fread(curr, sizeof(Block), 1, f) != 1) {
            free(curr); // Fine del file o errore
            break; 
        }

        // Ricostruiamo i puntatori in RAM
        curr->next = NULL; // Importante pulire il next letto dal disco (che è vecchio)

        if (root == NULL) {
            root = curr; // Il primo letto è il Genesi
        } else {
            prev->next = curr; // Colleghiamo la lista
        }

        prev = curr;
        count++;
    }
    fclose(f);
    printf("[DISK] Loaded %d blocks.\n", count);

    if (!verifyFullChain(root)) {
        fatal_error("CORRUPTED CHAIN DETECTED ON DISK! REFUSING TO START.");
    }

    return root;
}

int main() {
    Block *blockchain = load_blockchain();

    // Troviamo l'ultimo blocco
    Block *last = blockchain;
    while (last->next != NULL) {
        last = last->next;
    }

    printf("\n--- NODO AVVIATO ---\n");
    print_block(last); 

    printf("\n[ACTION] Aggiungo un nuovo post...\n");
    Block *new_b = mine_new_block(last, ACT_POST_CONTENT, 
                                 &(PayloadPost){ .content = "Persistence check!" }, 
                                 GOD_PUB_KEY, GOD_PRIV_KEY);
    
    if (new_b) {
        print_block(new_b);
        last = new_b; 
    }

    char my_priv[128];
    char my_pub[256];
    generate_keypair(my_priv, my_pub); 

    printf("Nuove chiavi generate!\nPriv: %s\nPub: %s\n", my_priv, my_pub);

    Block *reg_block = register_user(
        last, 
        &(PayloadRegister){.username="NuovoUser"}, 
        my_priv,
        my_pub
    );

    if (reg_block) {
        printf("[SUCCESS] Utente registrato nel blocco #%d\n", reg_block->index);
        print_block(reg_block); 
        last = reg_block;       
    }
    
    save_blockchain(blockchain);

    EVP_cleanup();
    return 0;   
}