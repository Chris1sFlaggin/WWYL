#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"
#include "wwyl_config.h" 
#include "user.h"
#include "post_state.h"

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

    const char *my_bio = "@Chris1sflaggin - Hello from the founder of WWYL!";
    snprintf(block->data.post.content, MAX_CONTENT_LEN, "%s", my_bio);

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

int main() {
    // 1. Init
    remove("wwyl_chain.dat"); // Standard C remove invece di system()
    Block *blockchain = load_blockchain();
    Block *last = blockchain;
    while(last->next) last = last->next;

    printf("\n=== WWYL SIMULATION: GAME THEORY TEST ===\n");

    // 2. Creazione Utenti
    char alice_k[2][SIGNATURE_LEN], bob_k[2][SIGNATURE_LEN], charlie_k[2][SIGNATURE_LEN];
    generate_keypair(alice_k[0], alice_k[1]);
    generate_keypair(bob_k[0], bob_k[1]);
    generate_keypair(charlie_k[0], charlie_k[1]);

    Block *b;
    if((b = register_user(last, &(PayloadRegister){.username="Alice"}, alice_k[0], alice_k[1]))) last = b;
    if((b = register_user(last, &(PayloadRegister){.username="Bob"}, bob_k[0], bob_k[1]))) last = b;
    if((b = register_user(last, &(PayloadRegister){.username="Charlie"}, charlie_k[0], charlie_k[1]))) last = b;

    printf("\n--- SCENARIO 1: Alice posta, Bob e Charlie votano ---\n");
    // Alice Posta (-5 Token)
    b = user_post(last, &(PayloadPost){.content="Bitcoin > Gold"}, alice_k[0], alice_k[1]);
    last = b;
    int pid = b->index;
    printf("📝 Alice ha postato (ID: %d). Bal: %d\n", pid, state_get_user(alice_k[1])->token_balance);

    // Bob Vota LIKE (-2 Token)
    PayloadReveal vote_bob = {.target_post_id=pid, .vote_value=1, .salt_secret="bob_secret"};
    b = user_like(last, &vote_bob, bob_k[0], bob_k[1]);
    if(b) last = b;

    // Charlie Vota DISLIKE (-2 Token)
    PayloadReveal vote_charlie = {.target_post_id=pid, .vote_value=-1, .salt_secret="charlie_secret"};
    b = user_like(last, &vote_charlie, charlie_k[0], charlie_k[1]);
    if(b) last = b;

    printf("\n--- TENTATIVO DI REVEAL PREMATURO ---\n");
    // Bob prova a rivelare subito (Dovrebbe fallire)
    b = user_reveal(last, &vote_bob, bob_k[0], bob_k[1]);
    if(b == NULL) printf("✅ Reveal bloccato correttamente (attendere 24h).\n");

    printf("\n--- ⏰ 25 ORE DOPO... ---\n");
    time_travel_hack(pid, 25); // Hack per testare la logica

    // Ora rivelano
    printf("🔓 Bob Rivela (Like)...\n");
    b = user_reveal(last, &vote_bob, bob_k[0], bob_k[1]);
    if(b) last = b;

    printf("🔓 Charlie Rivela (Dislike)...\n");
    b = user_reveal(last, &vote_charlie, charlie_k[0], charlie_k[1]);
    if(b) last = b;

    printf("\n--- FINALIZZAZIONE ---\n");
    finalize_post_rewards(pid);
    // Risultato atteso: 1 Like vs 1 Dislike. Pareggio o vittoria Like (dipende da >=).
    // Se Like vince: Alice prende streak, Bob vince soldi. Charlie perde.

    printf("\n--- SCENARIO 2: Streak Reset ---\n");
    // Alice posta una cosa orribile
    b = user_post(last, &(PayloadPost){.content="I love scams"}, alice_k[0], alice_k[1]);
    last = b;
    int pid2 = b->index;
    
    // Tutti odiano il post
    PayloadReveal vote_hate = {.target_post_id=pid2, .vote_value=-1, .salt_secret="hate"};
    user_like(last, &vote_hate, bob_k[0], bob_k[1]); // Bob dislike
    last = last->next;
    
    time_travel_hack(pid2, 25);
    
    user_reveal(last, &vote_hate, bob_k[0], bob_k[1]);
    last = last->next;
    
    finalize_post_rewards(pid2); // Qui Alice dovrebbe perdere la streak

    // STATISTICHE FINALI
    printf("\n=== CLASSIFICA FINALE ===\n");
    UserState *sa = state_get_user(alice_k[1]);
    UserState *sb = state_get_user(bob_k[1]);
    UserState *sc = state_get_user(charlie_k[1]);

    printf("Alice   | Bal: %2d | Streak: %d (Best: %d)\n", sa->token_balance, sa->current_streak, sa->best_streak);
    printf("Bob     | Bal: %2d | (Vincitore seriale)\n", sb->token_balance);
    printf("Charlie | Bal: %2d | (Sfortunato)\n", sc->token_balance);

    save_blockchain(blockchain);
    state_cleanup();
    post_index_cleanup();
    EVP_cleanup();
    return 0;
}