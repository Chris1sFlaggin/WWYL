#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "wwyl_config.h" 
#include "user.h"

PostMap global_post_index;

// ---------------------------------------------------------
// HASHING INTERI PER MAPPE
// ---------------------------------------------------------
unsigned long hash_int(int key, int size) {
    return key % size;
}

// ---------------------------------------------------------
// RIDIMENSIONA MAPPA POST (Dynamic Growth)
// ---------------------------------------------------------
void post_index_resize() {
    int old_size = global_post_index.size;
    PostStateNode **old_buckets = global_post_index.buckets;
    
    // Double the size
    global_post_index.size = old_size * 2;
    global_post_index.buckets = (PostStateNode**)safe_zalloc(global_post_index.size * sizeof(PostStateNode*));
    global_post_index.count = 0; // Will be recounted during rehashing
    
    printf("[INDEX] Resizing hashmap from %d to %d buckets...\n", old_size, global_post_index.size);
    
    // Rehash all existing entries
    for (int i = 0; i < old_size; i++) {
        PostStateNode *curr = old_buckets[i];
        while (curr) {
            PostStateNode *next = curr->next;
            
            // Recalculate hash with new size
            unsigned long new_idx = hash_int(curr->post_id, global_post_index.size);
            
            // Insert at new position
            curr->next = global_post_index.buckets[new_idx];
            global_post_index.buckets[new_idx] = curr;
            global_post_index.count++;
            
            curr = next;
        }
    }
    
    // Free old bucket array (nodes were moved, not freed)
    free(old_buckets);
    printf("[INDEX] Resize complete. %d posts rehashed.\n", global_post_index.count);
}

// ---------------------------------------------------------
// INIZIALIZZAZIONE MAPPA POST
// ---------------------------------------------------------
void post_index_init() {
    global_post_index.size = INITIAL_POST_MAP_SIZE;
    global_post_index.count = 0;
    global_post_index.buckets = (PostStateNode**)safe_zalloc(global_post_index.size * sizeof(PostStateNode*));
}

// ---------------------------------------------------------
// OTTIENI DIMENSIONE MAPPA POST
// ---------------------------------------------------------
int post_index_size() {
    return global_post_index.count;
}

// ---------------------------------------------------------
// OTTIENI CAPACITÀ MAPPA POST
// ---------------------------------------------------------
int post_index_capacity() {
    return global_post_index.size;
}

// ---------------------------------------------------------
// OTTIENI AUTORE POST DALL'INDICE
// ---------------------------------------------------------
char *post_index_author(int post_id) {
    PostState *state = post_index_get(post_id);
    if (state) {
        return state->author_pubkey;
    }
    return NULL;
}

// ---------------------------------------------------------
// VERIFICA ESISTENZA POST NELL'INDICE
// ---------------------------------------------------------
int post_index_exists(int post_id) {
    unsigned long idx = hash_int(post_id, global_post_index.size);
    PostStateNode *curr = global_post_index.buckets[idx];
    
    while (curr) {
        if (curr->post_id == post_id) return 1;
        curr = curr->next;
    }
    return 0;
}

// ---------------------------------------------------------
// AGGIUNGI POST ALL'INDICE
// ---------------------------------------------------------
void post_index_add(int post_id, const char *author) {
    if (global_post_index.count > 0 && 
        (double)global_post_index.count / global_post_index.size > MAX_CAPACITY_LOAD) {
        post_index_resize();
    }
    
    unsigned long idx = hash_int(post_id, global_post_index.size);
    
    PostStateNode *node = (PostStateNode*)safe_zalloc(sizeof(PostStateNode));
    node->post_id = post_id;
    
    // Setup stato iniziale
    node->state.post_id = post_id;
    snprintf(node->state.author_pubkey, SIGNATURE_LEN, "%s", author);
    node->state.likes = 0;
    node->state.dislikes = 0;
    node->state.is_open = 1; // Aperto alle scommesse
    node->state.created_at = time(NULL);

    // Inserimento in testa
    node->next = global_post_index.buckets[idx];
    global_post_index.buckets[idx] = node;
    global_post_index.count++;

    printf("[INDEX] Post #%d indicizzato. Author: %.8s...\n", post_id, author);
}

// ---------------------------------------------------------
// OTTIENI STATO POST DALL'INDICE
// ---------------------------------------------------------
PostState *post_index_get(int post_id) {
    unsigned long idx = hash_int(post_id, global_post_index.size);
    PostStateNode *curr = global_post_index.buckets[idx];
    
    while (curr) {
        if (curr->post_id == post_id) return &curr->state;
        curr = curr->next;
    }
    return NULL;
}

// ---------------------------------------------------------
// AGGIORNA VOTO POST
// ---------------------------------------------------------
void post_index_cleanup() {
    for(int i=0; i<global_post_index.size; i++) {
        PostStateNode *curr = global_post_index.buckets[i];
        while(curr) {
            PostStateNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
    free(global_post_index.buckets);
}

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
            snprintf(payload_str, sizeof(payload_str), "%s",
                     block->data.follow.target_user_pubkey);
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
            printf("# Follow Action: user %s\n", 
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
    state_init(); 
    post_index_init();
    
    FILE *f = fopen("wwyl_chain.dat", "rb");
    if (!f) {
        printf("[INFO] Nessuna chain. Creo Genesi...\n");
        Block *gen = initialize_blockchain();
        rebuild_state_from_chain(gen); 
        return gen;
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

    rebuild_state_from_chain(root);

    return root;
}

// ... (Tutto il codice sopra resta uguale: include, getBlockId, funzioni blockchain ...)

int main() {
    // 1. CARICAMENTO E RICOSTRUZIONE STATO (Hashmap)
    Block *blockchain = load_blockchain();

    // Troviamo la testa della catena per appendere nuovi blocchi
    Block *last = blockchain;
    while (last->next != NULL) {
        last = last->next;
    }

    printf("\n--- NODO AVVIATO ---\n");
    printf("[INFO] Ultimo blocco: #%d\n", last->index);

    // ---------------------------------------------------------
    // TEST 1: CREAZIONE UTENTE 'ALICE'
    // ---------------------------------------------------------
    printf("\n--- [TEST 1] Creazione Utente ALICE ---\n");
    // Usiamo SIGNATURE_LEN per evitare warning del compilatore
    char alice_priv[SIGNATURE_LEN], alice_pub[SIGNATURE_LEN];
    generate_keypair(alice_priv, alice_pub); 

    Block *b_alice = register_user(last, 
        &(PayloadRegister){
            .username="Alice", 
            .bio="Crypto Enthusiast", 
            .pic_url="img/alice.png"
        }, 
        alice_priv, alice_pub
    );

    if (b_alice) {
        print_block(b_alice);
        last = b_alice; // Aggiorniamo la testa
    } else {
        printf("[INFO] Alice (o meglio, questa chiave) era già registrata.\n");
    }

    // ---------------------------------------------------------
    // TEST 1.5: SICUREZZA DOPPIA REGISTRAZIONE
    // ---------------------------------------------------------
    printf("\n--- [TEST 1.5] Tentativo Doppia Registrazione (Replay Attack) ---\n");
    printf("[ACTION] Provo a registrare di nuovo Alice con le stesse chiavi...\n");
    
    Block *b_alice_dup = register_user(last, 
        &(PayloadRegister){.username="Alice_Clone", .bio="Hacker"}, 
        alice_priv, alice_pub); // <--- STESSE CHIAVI DI PRIMA!

    if (b_alice_dup == NULL) {
        printf("✅ [SECURE] Il sistema ha BLOCCATO la registrazione duplicata!\n");
    } else {
        printf("❌ [FAIL] ERRORE CRITICO: L'utente si è registrato due volte!\n");
        last = b_alice_dup;
    }

    // ---------------------------------------------------------
    // TEST 2: CREAZIONE UTENTE 'BOB'
    // ---------------------------------------------------------
    printf("\n--- [TEST 2] Creazione Utente BOB ---\n");
    char bob_priv[SIGNATURE_LEN], bob_pub[SIGNATURE_LEN];
    generate_keypair(bob_priv, bob_pub);

    Block *b_bob = register_user(last, 
        &(PayloadRegister){
            .username="Bob", 
            .bio="Builder", 
            .pic_url="img/bob.png"
        }, 
        bob_priv, bob_pub
    );

    if (b_bob) {
        print_block(b_bob);
        last = b_bob;
    }

    // VERIFICA STATO IN RAM
    UserState *s_alice = state_get_user(alice_pub);
    if (s_alice) {
        printf("\n[STATE CHECK] Alice in RAM: Username='%s', Balance=%d\n", s_alice->username, s_alice->token_balance);
    }

    // Bob pubblica post con api
    Block *bob_content = user_post(last, &(PayloadPost){.content = "Bob's post!"}, bob_priv, bob_pub);
    if (bob_content) {
        print_block(bob_content);
        last = bob_content;
        
        // Verifica stato post nella hashmap
        int bob_post_id = bob_content->index;
        PostState *ps_bob = post_index_get(bob_post_id);
        if (ps_bob) {
            printf("[POST CHECK] Post #%d - Author: %.10s..., Likes: %d, Dislikes: %d, Open: %d\n",
                   ps_bob->post_id, ps_bob->author_pubkey, ps_bob->likes, ps_bob->dislikes, ps_bob->is_open);
        }
    }
    
    // Alice pubblica un secondo post per testare la hashmap
    printf("\n--- [TEST 2.5] Alice crea un post ---\n");
    Block *alice_content = user_post(last, &(PayloadPost){.content = "Alice's first post!"}, alice_priv, alice_pub);
    if (alice_content) {
        print_block(alice_content);
        last = alice_content;
        
        int alice_post_id = alice_content->index;
        PostState *ps_alice = post_index_get(alice_post_id);
        if (ps_alice) {
            printf("[POST CHECK] Post #%d - Author: %.10s..., Likes: %d, Dislikes: %d, Open: %d\n",
                   ps_alice->post_id, ps_alice->author_pubkey, ps_alice->likes, ps_alice->dislikes, ps_alice->is_open);
        }
    }
    
    // Stampa statistiche hashmap
    printf("\n[HASHMAP STATS] Total posts indexed: %d\n", global_post_index.count);
    
    // ---------------------------------------------------------
    // TEST 2.7: BOB COMMENTA IL POST DI ALICE
    // ---------------------------------------------------------
    printf("\n--- [TEST 2.7] Bob commenta il post di Alice ---\n");
    if (alice_content) {
        int target_post = alice_content->index;
        PayloadComment comment_payload;
        comment_payload.target_post_id = target_post;
        snprintf(comment_payload.content, sizeof(comment_payload.content), "Nice post Alice!");
        
        Block *bob_comment = user_comment(last, &comment_payload, bob_priv, bob_pub);
        if (bob_comment) {
            print_block(bob_comment);
            last = bob_comment;
            printf("[COMMENT CHECK] Bob successfully commented on post #%d\n", target_post);
        } else {
            printf("[COMMENT ERROR] Failed to create comment\n");
        }
    }
    
    // ---------------------------------------------------------
    // TEST 3: ALICE SEGUE BOB (Follow)
    // ---------------------------------------------------------
    printf("\n--- [TEST 3] Alice segue Bob ---\n");
    
    PayloadFollow follow_payload;
    memset(&follow_payload, 0, sizeof(PayloadFollow));
    // Importante: Copiare la chiave di Bob nel payload
    snprintf(follow_payload.target_user_pubkey, sizeof(follow_payload.target_user_pubkey), "%s", bob_pub);

    Block *b_follow = user_follow(last, &follow_payload, alice_priv, alice_pub);

    if (b_follow) {
        print_block(b_follow);
        last = b_follow;
    }

    // Verifica immediata contatori
    UserState *s_bob = state_get_user(bob_pub);
    printf("[CHECK] Bob Followers: %d (Dovrebbe essere 1)\n", s_bob ? s_bob->followers_count : -1);


    // ---------------------------------------------------------
    // TEST 4: ALICE SEGUE DI NUOVO BOB (Toggle -> Unfollow)
    // ---------------------------------------------------------
    printf("\n--- [TEST 4] Alice ri-segue Bob (Logica Toggle -> Unfollow) ---\n");
    
    Block *b_unfollow = user_follow(last, &follow_payload, alice_priv, alice_pub);

    if (b_unfollow) {
        print_block(b_unfollow);
        last = b_unfollow;
    }

    // Verifica contatori dopo unfollow
    printf("[CHECK] Bob Followers: %d (Dovrebbe essere 0)\n", s_bob ? s_bob->followers_count : -1);


    // ---------------------------------------------------------
    // CHIUSURA E SALVATAGGIO
    // ---------------------------------------------------------
    save_blockchain(blockchain);
    post_index_cleanup(); 
    state_cleanup(); // Pulisce la Hashmap
    EVP_cleanup();   // Pulisce OpenSSL

    return 0;   
}