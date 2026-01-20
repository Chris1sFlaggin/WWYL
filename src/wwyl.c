#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "wwyl_config.h" 
#include "user.h"
#include "post_state.h"

WalletStore global_wallet;
int current_user_idx = -1;

// --- PERSISTENZA WALLET ---
void save_wallet_to_disk() {
    FILE *f = fopen(WALLET_FILE, "wb");
    if (!f) {
        printf("[WALLET] ⚠️ Impossibile salvare il wallet su disco.\n");
        return;
    }
    fwrite(&global_wallet, sizeof(WalletStore), 1, f);
    fclose(f);
    printf("[WALLET] Chiavi salvate in '%s'.\n", WALLET_FILE);
}

void load_wallet_from_disk() {
    FILE *f = fopen(WALLET_FILE, "rb");
    if (!f) {
        printf("[WALLET] Nessun file wallet trovato. Creane uno nuovo.\n");
        global_wallet.count = 0;
        return;
    }
    if (fread(&global_wallet, sizeof(WalletStore), 1, f) != 1) {
        printf("[WALLET] Errore lettura wallet o file vuoto.\n");
        global_wallet.count = 0;
    } else {
        printf("[WALLET] Caricate %d identità da disco.\n", global_wallet.count);
    }
    fclose(f);
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
void serialize_block_content(const Block *block, char *buffer, size_t size) {
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
        case ACT_POST_FINALIZE:
            snprintf(payload_str, sizeof(payload_str), "%d", block->data.finalize.target_post_id);
            break;
        default:
            snprintf(payload_str, sizeof(payload_str), "UNKNOWN");
            break;
    }

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
// CREAZIONE DEL BLOCCO GENESI
// ---------------------------------------------------------
Block *create_genesis_block() {
    Block *block = (Block *)safe_zalloc(sizeof(Block));
    block->index = 0;
    block->timestamp = time(NULL);
    block->type = ACT_REGISTER_USER;
    memset(block->prev_hash, '0', 64);
    block->prev_hash[64] = '\0';

    if (snprintf(block->sender_pubkey, sizeof(block->sender_pubkey), "%s", GOD_PUB_KEY) >= (int)sizeof(block->sender_pubkey)) {
        fprintf(stderr, "[WARN] Genesis Public Key troncata!\n");
    }

    snprintf(block->data.registration.username, sizeof(block->data.registration.username), "Chris1sflaggin");
    snprintf(block->data.registration.bio, sizeof(block->data.registration.bio), "Hello from the founder of WWYL!");
    snprintf(block->data.registration.pic_url, sizeof(block->data.registration.pic_url), "founder.png");

    char raw_data_buffer[2048];
    serialize_block_content(block, raw_data_buffer, sizeof(raw_data_buffer));
    sha256_hash(raw_data_buffer, strlen(raw_data_buffer), block->curr_hash);
    ecdsa_sign(GOD_PRIV_KEY, block->curr_hash, block->signature);

    printf("[GENESIS] Profile Created for: %.16s...\n", block->sender_pubkey);
    printf("[GENESIS] Signature: %.16s...\n", block->signature);

    return block;
}

// ---------------------------------------------------------
// INIT BLOCKCHAIN
// ---------------------------------------------------------
Block *initialize_blockchain() {
    Block *genesis = create_genesis_block();
    printf("[INFO] Blockchain inizializzata. Genesis Address: %p\n", (void*)genesis);
    return genesis;
}

// ---------------------------------------------------------
// PRINT BLOCK
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
    
    // (Opzionale: puoi aggiungere lo switch case per stampare il payload specifico qui)
    
    printf("=========================\n");
}

// ---------------------------------------------------------
// MINE NEW BLOCK
// ---------------------------------------------------------
Block *mine_new_block(Block *prev_block, ActionType type, const void *payload_data, const char *sender_pubkey, const char *sender_privkey) {
    if (!prev_block) { fatal_error("Previous block is NULL!"); }
    
    Block *new_block = (Block *)safe_zalloc(sizeof(Block));
    new_block->index = prev_block->index + 1;
    new_block->timestamp = time(NULL);
    snprintf(new_block->prev_hash, sizeof(new_block->prev_hash), "%s", prev_block->curr_hash);
    new_block->type = type;

    switch (type) {
    case ACT_POST_CONTENT:
        snprintf(new_block->data.post.content, MAX_CONTENT_LEN, "%s", ((PayloadPost *)payload_data)->content);
        break;
    case ACT_POST_COMMENT:
        new_block->data.comment.target_post_id = ((PayloadComment *)payload_data)->target_post_id;
        snprintf(new_block->data.comment.content, MAX_CONTENT_LEN, "%s", ((PayloadComment *)payload_data)->content);
        break;
    case ACT_VOTE_COMMIT:
        new_block->data.commit.target_post_id = ((PayloadCommit *)payload_data)->target_post_id;
        snprintf(new_block->data.commit.vote_hash, HASH_LEN, "%s", ((PayloadCommit *)payload_data)->vote_hash);
        break;
    case ACT_VOTE_REVEAL:
        new_block->data.reveal.target_post_id = ((PayloadReveal *)payload_data)->target_post_id;
        new_block->data.reveal.vote_value = ((PayloadReveal *)payload_data)->vote_value;
        snprintf(new_block->data.reveal.salt_secret, sizeof(new_block->data.reveal.salt_secret), "%s", ((PayloadReveal *)payload_data)->salt_secret);
        break;
    case ACT_FOLLOW_USER:
        snprintf(new_block->data.follow.target_user_pubkey, SIGNATURE_LEN, "%s", ((PayloadFollow *)payload_data)->target_user_pubkey);
        break;
    case ACT_REGISTER_USER:
        snprintf(new_block->data.registration.username, 32, "%s", ((PayloadRegister *)payload_data)->username);
        snprintf(new_block->data.registration.bio, 64, "%s", ((PayloadRegister *)payload_data)->bio);
        snprintf(new_block->data.registration.pic_url, 128, "%s", ((PayloadRegister *)payload_data)->pic_url);
        break;
    case ACT_POST_FINALIZE:
        new_block->data.finalize.target_post_id = ((PayloadFinalize *)payload_data)->target_post_id;
        break;
    default:
        printf("[WARN] Unknown block type during mining.\n");
        break;
    }

    if (snprintf(new_block->sender_pubkey, sizeof(new_block->sender_pubkey), "%s", sender_pubkey) >= (int)sizeof(new_block->sender_pubkey)) {
        fprintf(stderr, "[WARN] Sender Public Key troncata!\n");
    }

    char raw_data_buffer[2048];
    serialize_block_content(new_block, raw_data_buffer, sizeof(raw_data_buffer));
    sha256_hash(raw_data_buffer, strlen(raw_data_buffer), new_block->curr_hash);
    ecdsa_sign(sender_privkey, new_block->curr_hash, new_block->signature);

    prev_block->next = new_block;
    printf("[MINED] Block #%d (Type: %d) mined by %.10s...\n", new_block->index, new_block->type, new_block->sender_pubkey);

    if (!integrity_check(prev_block, new_block)) {
        prev_block->next = NULL; 
        free(new_block);         
        return NULL;             
    }

    return new_block;
}

// ---------------------------------------------------------
// VERIFICA CATENA
// ---------------------------------------------------------
int verifyFullChain(Block *genesis) {
    if (!genesis) return 0;
    Block *prev = genesis;
    Block *curr = genesis->next;
    char temp_hash[HASH_LEN + 1];
    char raw_buffer[2048];
    int count = 1;

    printf("\n[SECURITY] Avvio verifica integrità blockchain...\n");
    while (curr != NULL) {
        if (integrity_check(prev, curr) == 0) return 0;
        serialize_block_content(curr, raw_buffer, sizeof(raw_buffer));
        sha256_hash(raw_buffer, strlen(raw_buffer), temp_hash);
        if (strcmp(temp_hash, curr->curr_hash) != 0) {
            fprintf(stderr, "[ALERT] DATA TAMPERING at Block #%d!\n", curr->index);
            return 0; 
        }
        prev = curr;
        curr = curr->next;
        count++;
    }
    printf("[SECURITY] Chain Verified. %d blocks checked. Status: SECURE.\n", count);
    return 1; 
}

// ---------------------------------------------------------
// SALVATAGGIO
// ---------------------------------------------------------
void save_blockchain(Block *genesis) {
    FILE *f = fopen("wwyl_chain.dat", "wb"); 
    if (!f) { perror("[ERR] Cannot save blockchain"); return; }
    Block *curr = genesis;
    int count = 0;
    while (curr != NULL) {
        fwrite(curr, sizeof(Block), 1, f);
        curr = curr->next;
        count++;
    }
    fclose(f);
    printf("[DISK] Blockchain saved! (%d blocks written)\n", count);
}

// ---------------------------------------------------------
// CARICAMENTO
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
        if (fread(curr, sizeof(Block), 1, f) != 1) {
            free(curr); 
            break; 
        }
        curr->next = NULL; 
        if (root == NULL) root = curr; 
        else prev->next = curr; 
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

// ---------------------------------------------------------
// MAIN
// ---------------------------------------------------------
// Funzione helper per simulare il passaggio del tempo
void time_travel_hack(int post_id, int hours_forward) {
    PostState *p = post_index_get(post_id);
    if(p) {
        p->created_at -= (hours_forward * 3600); // Spostiamo la creazione nel passato
        printf("⏰ [HACK] Time Travel! Spostato Post #%d indietro di %d ore.\n", post_id, hours_forward);
    }
}

void free_blockchain(Block *genesis) {
    Block *curr = genesis;
    while (curr != NULL) {
        Block *next = curr->next; // Salviamo il prossimo prima di distruggere il corrente
        free(curr);
        curr = next;
    }
    printf("[MEM] Blockchain memory freed successfully.\n");
}

// Helper per pulire chiavi sensibili (Security Best Practice)
void secure_memzero(void *ptr, size_t size) {
    volatile unsigned char *p = ptr;
    while (size--) *p++ = 0;
}

// Esempio di snippet per stampare i commenti di un post
void print_post_comments(int post_id) {
    PostState *p = post_index_get(post_id);
    if (!p) return;

    printf("\n--- COMMENTI (%d) ---\n", post_id);
    CommentNode *curr = p->comments;
    while(curr) {
        printf("@%.8s... dice: %s\n", curr->author_pubkey, curr->content);
        curr = curr->next;
    }
}

void print_cli() {
    printf("\n=== WWYL NODE CLI ===\n");
    if (current_user_idx >= 0) {
        // Recuperiamo lo stato fresco dalla blockchain per mostrare il saldo reale
        UserState *u = state_get_user(global_wallet.entries[current_user_idx].pub);
        printf("👤 Utente: %s\n", global_wallet.entries[current_user_idx].username);
        printf("💰 Saldo: %d | 🔥 Streak: %d\n", u ? u->token_balance : 0, u ? u->current_streak : 0);
        printf("🔑 PubKey: %s...\n", global_wallet.entries[current_user_idx].pub);
    } else {
        printf("👤 Utente: OSPITE (Login necessario)\n");
    }
    printf("---------------------\n");
    printf("[1] 🔑 Crea Nuova Identità (Keygen)\n");
    printf("[2] 📝 Registra Utente su Blockchain (Bonus 10 Token)\n");
    printf("[3] 👤 Login (Cambia Utente)\n");
    printf("[4] 📢 Posta Contenuto (Costo: %d)\n", COSTO_POST);
    printf("[5] 🗳️ Vota Post (Commit) (Costo: %d)\n", COSTO_VOTO);
    printf("[6] 🔓 Rivela Voto (Reveal)\n");
    printf("[7] 🏁 Finalizza Post (Distribuisci Premi)\n");
    printf("[8] 📊 Mostra Stato Globale\n");
    printf("[9] ⏰ [DEBUG] Time Travel (-25h)\n");
    printf("[10] 👑 Importa GOD Wallet (Admin Only)\n"); 
    printf("[11] Commenta su Post\n");
    printf("[12] Segui Utente\n");
    printf("[13] Mostra Commenti di un Post\n");
    printf("[0] 💾 Esci e Salva Tutto\n");
    printf("> ");
}

int main() {
    // 1. Caricamento Blockchain (Ledger Pubblico)
    Block *blockchain = load_blockchain();
    Block *last = blockchain;
    while(last->next) last = last->next;

    // 2. Caricamento Wallet (Chiavi Private Locali)
    load_wallet_from_disk();

    int choice;
    char buffer[MAX_CONTENT_LEN];
    int target_id, vote_val;

    while(1) {
        print_cli();
        if (scanf("%d", &choice) != 1) { while(getchar() != '\n'); continue; }
        getchar(); // Consuma newline

        switch(choice) {
            case 1: { // KEYGEN
                if (global_wallet.count >= 10) { printf("Wallet pieno (max 10)!\n"); break; }
                
                WalletEntry *w = &global_wallet.entries[global_wallet.count];
                
                printf("Inserisci Username locale: ");
                if (fgets(w->username, sizeof(w->username), stdin) == NULL) {
                    printf("Errore input.\n"); break;
                }
                w->username[strcspn(w->username, "\n")] = 0;
                
                generate_keypair(w->priv, w->pub);
                w->registered = 0;
                
                // Auto-login
                current_user_idx = global_wallet.count;
                global_wallet.count++;
                
                printf("🔑 Chiavi generate! Ricordati di registrarti [2].\n");
                save_wallet_to_disk(); // Salvataggio automatico
                break;
            }

            case 2: { // REGISTER
                if (current_user_idx < 0) { printf("Devi prima creare un'identità o fare login.\n"); break; }
                WalletEntry *w = &global_wallet.entries[current_user_idx];
                
                if (state_get_user(w->pub)) {
                    printf("⚠️ Utente già registrato sulla blockchain.\n");
                    w->registered = 1;
                    break;
                }

                PayloadRegister reg;
                snprintf(reg.username, 32, "%s", w->username);
                snprintf(reg.bio, 64, "CLI User");
                snprintf(reg.pic_url, 128, "default.png");

                Block *b = register_user(last, &reg, w->priv, w->pub);
                if (b) {
                    last = b;
                    w->registered = 1;
                    printf("✅ Registrazione confermata nel blocco #%d\n", b->index);
                    save_wallet_to_disk(); // Aggiorna lo stato "registered" su disco
                }
                break;
            }

            case 3: { // LOGIN
                printf("\n--- PORTAFOGLIO LOCALE ---\n");
                for(int i=0; i<global_wallet.count; i++) {
                    printf("[%d] %s %s\n", i, global_wallet.entries[i].username, 
                           global_wallet.entries[i].registered ? "✅" : "❌");
                }
                printf("Seleziona ID: ");
                int id;
                if (scanf("%d", &id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                if (id >= 0 && id < global_wallet.count) {
                    // Verifica Crittografica (Anti-Spoofing locale)
                    if (user_login(global_wallet.entries[id].priv, global_wallet.entries[id].pub)) {
                        current_user_idx = id;
                    }
                } else {
                    printf("ID non valido.\n");
                }
                break;
            }

            case 4: { // POST
                if (current_user_idx < 0) break;
                printf("Contenuto del post: ");
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf("Errore input.\n"); break;
                }
                buffer[strcspn(buffer, "\n")] = 0;

                WalletEntry *w = &global_wallet.entries[current_user_idx];
                PayloadPost p;
                snprintf(p.content, MAX_CONTENT_LEN, "%s", buffer);
                
                Block *b = user_post(last, &p, w->priv, w->pub);
                if (b) {
                    last = b;
                    printf("📝 Post pubblicato! ID: %d\n", b->index);
                }
                break;
            }
            
            case 5: { // VOTE
                if (current_user_idx < 0) break;
                printf("ID Post: ");
                if (scanf("%d", &target_id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                printf("Voto (1=Like, -1=Dislike): ");
                if (scanf("%d", &vote_val) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                printf("Salt Segreto: "); getchar(); 
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf("Errore input.\n"); break;
                }
                buffer[strcspn(buffer, "\n")] = 0;
                // Limit buffer to 31 chars to prevent truncation
                buffer[31] = '\0';
                WalletEntry *w = &global_wallet.entries[current_user_idx];
                PayloadReveal rev = {.target_post_id=target_id, .vote_value=vote_val};
                snprintf(rev.salt_secret, sizeof(rev.salt_secret), "%.31s", buffer);
                Block *b = user_like(last, &rev, w->priv, w->pub);
                if (b) last = b;
                break;
            }
            case 6: { // REVEAL
                if (current_user_idx < 0) break;
                printf("ID Post: ");
                if (scanf("%d", &target_id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                printf("Voto Originale: ");
                if (scanf("%d", &vote_val) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                printf("Salt Originale: "); getchar(); 
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf("Errore input.\n"); break;
                }
                buffer[strcspn(buffer, "\n")] = 0;
                // Limit buffer to 31 chars to prevent truncation
                buffer[31] = '\0';
                WalletEntry *w = &global_wallet.entries[current_user_idx];
                PayloadReveal rev = {.target_post_id=target_id, .vote_value=vote_val};
                snprintf(rev.salt_secret, sizeof(rev.salt_secret), "%.31s", buffer);
                Block *b = user_reveal(last, &rev, w->priv, w->pub);
                if (b) last = b;
                break;
            }
            case 7: { // FINALIZE
                if (current_user_idx < 0) break;
                printf("ID Post: ");
                if (scanf("%d", &target_id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                // Creiamo payload
                WalletEntry *w = &global_wallet.entries[current_user_idx];
                PayloadFinalize fin = { .target_post_id = target_id };
                
                // Chiamiamo la funzione che mina il blocco
                Block *b = user_finalize(last, &fin, w->priv, w->pub);
                if (b) last = b;
                break;
            }
            case 8: { // STATUS
                 printf("\n--- UTENTI NELLA BLOCKCHAIN ---\n");
                 // Qui iteriamo sul wallet locale per vedere i saldi dei nostri utenti
                 for(int i=0; i<global_wallet.count; i++) {
                     UserState *u = state_get_user(global_wallet.entries[i].pub);
                     if(u) printf("@%-10s | Bal: %3d | Streak: %d\n", u->username, u->token_balance, u->current_streak);
                 }
                 break;
            }
            case 9: { // HACK
                printf("ID Post: ");
                if (scanf("%d", &target_id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                time_travel_hack(target_id, 25);
                break;
            }

            case 10: { // IMPORT GOD WALLET
                if (global_wallet.count >= 10) { 
                    printf("Wallet pieno! Impossibile importare.\n"); 
                    break; 
                }
                
                WalletEntry *w = &global_wallet.entries[global_wallet.count];
                
                // Copiamo i dati hardcodati da wwyl_config.h
                snprintf(w->username, 32, "THE_CREATOR");
                snprintf(w->priv, SIGNATURE_LEN, "%s", GOD_PRIV_KEY);
                snprintf(w->pub, SIGNATURE_LEN, "%s", GOD_PUB_KEY);
                
                // Il GOD wallet è GIA' registrato nel blocco genesi, quindi settiamo a 1
                w->registered = 1; 
                
                // Selezioniamo subito questo utente
                current_user_idx = global_wallet.count;
                global_wallet.count++;
                
                save_wallet_to_disk();
                printf("👑 Wallet GOD importato con successo! Sei loggato come Creator.\n");
                break;
            }
            case 11: { // COMMENT
                if (current_user_idx < 0) break;
                printf("ID Post da commentare: ");
                if (scanf("%d", &target_id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                getchar(); // Consuma newline
                printf("Contenuto del commento: ");
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf("Errore input.\n"); break;
                }
                buffer[strcspn(buffer, "\n")] = 0;

                WalletEntry *w = &global_wallet.entries[current_user_idx];
                PayloadComment p;
                p.target_post_id = target_id;
                snprintf(p.content, MAX_CONTENT_LEN, "%s", buffer);
                
                Block *b = user_comment(last, &p, w->priv, w->pub);
                if (b) {
                    last = b;
                    printf("💬 Commento pubblicato! ID: %d\n", b->index);
                }
                break;
            }
            case 12: { // FOLLOW
                if (current_user_idx < 0) break;
                printf("PubKey Utente da seguire: ");
                if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                    printf("Errore input.\n"); break;
                }
                buffer[strcspn(buffer, "\n")] = 0;

                WalletEntry *w = &global_wallet.entries[current_user_idx];
                PayloadFollow p;
                
                // FIX WARNING: Usiamo "%.*s" per limitare esplicitamente la lettura
                // Legge al massimo (SIGNATURE_LEN - 1) caratteri da buffer.
                snprintf(p.target_user_pubkey, SIGNATURE_LEN, "%.*s", SIGNATURE_LEN - 1, buffer);
                
                Block *b = user_follow(last, &p, w->priv, w->pub);
                if (b) {
                    last = b;
                    printf("➕ Ora segui l'utente con PubKey: %.16s...\n", buffer);
                }
                break;
            }
            case 13: { // MOSTRA COMMENTI
                printf("ID Post per mostrare commenti: ");
                if (scanf("%d", &target_id) != 1) {
                    printf("Input non valido.\n");
                    while(getchar() != '\n');
                    break;
                }
                
                PostState *p = post_index_get(target_id);
                if (!p) {
                    printf("❌ Post #%d non trovato.\n", target_id);
                    break;
                }

                printf("\n--- COMMENTI SU POST #%d ---\n", target_id);
                
                CommentNode *curr = p->comments;
                if (!curr) {
                    printf("(Nessun commento presente)\n");
                }
                
                while(curr) {
                    // Recuperiamo l'username se possibile
                    UserState *u = state_get_user(curr->author_pubkey);
                    char *name = u ? u->username : "Unknown";
                    
                    printf("💬 @%s: %s\n", name, curr->content);
                    curr = curr->next;
                }
                printf("----------------------------\n");
                break;
            }
            case 0: // EXIT
                save_blockchain(blockchain); // Salva Ledger
                save_wallet_to_disk();       // Salva Chiavi
                
                // Cleanup Memoria
                free_blockchain(blockchain);
                state_cleanup();
                post_index_cleanup();
                EVP_cleanup();
                
                // Pulisce le chiavi in RAM prima di uscire (Security)
                secure_memzero(&global_wallet, sizeof(WalletStore));
                
                printf("👋 Bye!\n");
                return 0;
        }
    }
}