#include "user.h"
#include "post_state.h" 
#include <string.h>
#include "wwyl_config.h"
#include <openssl/rand.h>

HashMap *world_state = NULL; 
RelationNode *relation_map[REL_MAP_SIZE];
long long global_tokens_circulating = 0;

// ------------------------------------------------------------
// MINING TOKENS
// ------------------------------------------------------------
int mineTokens(long long amount) {
    if (global_tokens_circulating + amount > GLOBAL_TOKEN_LIMIT) return 0;
    global_tokens_circulating += amount;
    return 1;
}

// ------------------------------------------------------------
// HASH FUNCTION DJB2
// ------------------------------------------------------------
unsigned long hash_djb2(const char *str, int map_size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c; 
    return hash % map_size;
}

// ------------------------------------------------------------
// HASH FUNCTION RELATION MAP
// ------------------------------------------------------------
unsigned long hash_rel(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) hash = ((hash << 5) + hash) + c;
    return hash % REL_MAP_SIZE;
}

// -----------------------------------------------------------
// INITIALIZE STATE
// -----------------------------------------------------------
void state_init() {
    world_state = map_create(INITIAL_MAP_SIZE, hash_str, cmp_str, free, free);
}

// -----------------------------------------------------------
// GET USER STATE
// -----------------------------------------------------------
UserState *state_get_user(const char *wallet_address) {
    if (!world_state) return NULL;
    return (UserState *)map_get(world_state, wallet_address);
}

// -----------------------------------------------------------
// UPDATE USER STATE
// -----------------------------------------------------------
void state_update_user(const char *wallet_address, const UserState *new_state) {
    UserState *stored_state = safe_zalloc(sizeof(UserState));
    memcpy(stored_state, new_state, sizeof(UserState));

    char *key_dup = strdup(wallet_address);
    
    map_put(world_state, key_dup, stored_state);
}

// -----------------------------------------------------------
// ADD NEW USER
// -----------------------------------------------------------
void state_add_new_user(const char *wallet_address, const char *username, const char *bio, const char *pic) {
    UserState u = {0};
    snprintf(u.wallet_address, SIGNATURE_LEN, "%s", wallet_address);
    if(username) snprintf(u.username, 32, "%s", username);
    if(bio) snprintf(u.bio, 64, "%s", bio);
    if(pic) snprintf(u.pic_url, 128, "%s", pic);
    
    long long initial_balance = 0; // <--- DEFAULT ZERO

    if (strcmp(wallet_address, GOD_PUB_KEY) == 0) {
        initial_balance = GLOBAL_TOKEN_LIMIT / 2; 
        printf("👑 GOD USER DETECTED.\n");
    }

    if (mineTokens(initial_balance)) {
        u.token_balance = initial_balance;
    } else {
        u.token_balance = 0; 
    }

    state_update_user(wallet_address, &u);
    printf("[STATE] New User: %s (Bal: %d)\n", u.username, u.token_balance);
}

// -----------------------------------------------------------
// CLEANUP STATE
// -----------------------------------------------------------
int state_check_follow_status(const char *follower, const char *target) {
    char key[2048];
    snprintf(key, sizeof(key), "%s:%s", follower, target);
    unsigned long idx = hash_rel(key);
    RelationNode *curr = relation_map[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) return 1;
        curr = curr->next;
    }
    return 0;
}

// -----------------------------------------------------------
// TOGGLE FOLLOW STATUS
// -----------------------------------------------------------
void state_toggle_follow(const char *follower, const char *target) {
    char key[2048];
    snprintf(key, sizeof(key), "%s:%s", follower, target);
    unsigned long idx = hash_rel(key);
    UserState *u_follower = state_get_user(follower);
    UserState *u_target = state_get_user(target);
    RelationNode *curr = relation_map[idx];
    RelationNode *prev = NULL;
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            if (prev) prev->next = curr->next;
            else relation_map[idx] = curr->next;
            free(curr);
            if (u_follower && u_follower->following_count > 0) u_follower->following_count--;
            if (u_target && u_target->followers_count > 0) u_target->followers_count--;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    RelationNode *node = (RelationNode*)safe_zalloc(sizeof(RelationNode));
    strcpy(node->key, key);
    node->next = relation_map[idx];
    relation_map[idx] = node;
    if (u_follower) u_follower->following_count++;
    if (u_target) u_target->followers_count++;
}

// -----------------------------------------------------------
// FINALIZE POST REWARDS
// -----------------------------------------------------------
void finalize_post_rewards(int post_id) {
    PostState *p = post_index_get(post_id);
    if (!p || p->finalized) return;

    int winning_vote = (p->likes >= p->dislikes) ? 1 : -1;
    int winners_count = 0;
    RevealNode *curr = p->reveals;
    while(curr) {
        if (curr->vote_value == winning_vote) winners_count++;
        curr = curr->next;
    }

    UserState *author = state_get_user(p->author_pubkey);
    if (author) {
        if (winning_vote == 1) { 
            author->current_streak++;
            if (author->current_streak > author->best_streak) author->best_streak = author->current_streak;
            int bonus = author->current_streak*2;
            if (mineTokens(bonus)) {
                author->token_balance += bonus;
                printf("🔥 [STREAK] Author Streak x%d! Minted +%d Tokens. (Bal: %d)\n", 
                       author->current_streak, bonus, author->token_balance);
            }
        } else {
            printf("❄️ [STREAK] Author Streak Reset. Post rejected.\n");
            author->current_streak = 0;
        }
    }

    if (winners_count > 0 && p->pull > 0) {
        int reward = p->pull / winners_count;
        curr = p->reveals;
        while(curr) {
            if (curr->vote_value == winning_vote) {
                UserState *u = state_get_user(curr->voter_pubkey);
                if (u) {
                    u->token_balance += reward;
                    printf("💰 [PAYOUT] Voter %.8s... won %d tokens!\n", u->wallet_address, reward);
                }
            }
            curr = curr->next;
        }
    }
    p->finalized = 1;
    p->is_open = 0;
    printf("[ECONOMY] Post #%d Finalized. Pool: %d.\n", post_id, p->pull);
}

// -----------------------------------------------------------
// REBUILD STATE FROM CHAIN
// -----------------------------------------------------------
void rebuild_state_from_chain(Block *genesis) {
    printf("[STATE] 🔄 Replaying Blockchain History con Prezzi Dinamici...\n");
    
    // 1. Reset totale dell'economia
    global_tokens_circulating = 0; 
    
    Block *curr = genesis;
    while(curr != NULL) {
        // Calcoliamo il moltiplicatore che c'era IN QUEL MOMENTO
        // Basato sui token circolanti prima di processare questo blocco
        float historical_mult = get_economy_multiplier();

        if (curr->type == ACT_REGISTER_USER) {
            PayloadRegister *reg = &curr->data.registration;
            // Se è il blocco genesi o una registrazione normale
            if (curr->index == 0) {
                 // Nota: state_add_new_user aggiorna global_tokens_circulating
                state_add_new_user(curr->sender_pubkey, reg->username, reg->bio, reg->pic_url);
            } else {
                // Per utenti successivi al genesi
                 state_add_new_user(curr->sender_pubkey, reg->username, reg->bio, reg->pic_url);
            }
        }
        else if (curr->type == ACT_POST_CONTENT) {
            post_index_add(curr->index, curr->sender_pubkey);
            UserState *u = state_get_user(curr->sender_pubkey);
            
            // Calcolo il costo storico!
            int historical_cost = (int)(COSTO_POST * historical_mult);

            if(u && u->token_balance >= historical_cost) {
                u->token_balance -= historical_cost;
                
                PostState *p = post_index_get(curr->index);
                if(p) { 
                    p->pull += historical_cost; // Il pool cresce col prezzo pagato
                    p->created_at = curr->timestamp; 
                }
            }
        }
        else if (curr->type == ACT_VOTE_COMMIT) {
            int pid = curr->data.commit.target_post_id;
            post_register_commit(pid, curr->sender_pubkey, curr->data.commit.vote_hash);
            UserState *u = state_get_user(curr->sender_pubkey);
            
            // Calcolo il costo storico!
            int historical_cost = (int)(COSTO_VOTO * historical_mult);

            if(u && u->token_balance >= historical_cost) {
                u->token_balance -= historical_cost;
                
                PostState *p = post_index_get(pid);
                if(p) p->pull += historical_cost;
            }
        }
        else if (curr->type == ACT_VOTE_REVEAL) {
            int pid = curr->data.reveal.target_post_id;
            post_register_reveal(pid, curr->sender_pubkey, curr->data.reveal.vote_value);
        }
        else if (curr->type == ACT_FOLLOW_USER) {
            state_toggle_follow(curr->sender_pubkey, curr->data.follow.target_user_pubkey);
        }
        else if (curr->type == ACT_POST_FINALIZE) {
            int pid = curr->data.finalize.target_post_id;
            // Questa funzione al suo interno chiama mineTokens() per i bonus streak,
            // quindi aggiorna global_tokens_circulating correttamente per i blocchi successivi.
            finalize_post_rewards(pid);
        }
        else if (curr->type == ACT_POST_COMMENT) {
            int pid = curr->data.comment.target_post_id;
            post_register_comment(pid, curr->sender_pubkey, curr->data.comment.content, curr->timestamp);
       }
       else if (curr->type == ACT_TRANSFER) {
            UserState *sender = state_get_user(curr->sender_pubkey);
            UserState *receiver = state_get_user(curr->data.transfer.target_pubkey);
            int amount = curr->data.transfer.amount;
            
            if (sender && receiver && sender->token_balance >= amount) {
                sender->token_balance -= amount;
                receiver->token_balance += amount;
            }
        }
        
        curr = curr->next;
    }
    printf("[STATE] ✅ Replay Complete. Circulating Supply: %lld\n", global_tokens_circulating);
}

// ---------------------------------------------------------
// CHECK 24 ORE
// ---------------------------------------------------------
int check24hrs(time_t post_timestamp, time_t current_time) {
    double diff = difftime(current_time, post_timestamp);
    return (diff >= 0 && diff <= 86400);
}

// ---------------------------------------------------------
// HASH VOTO
// ---------------------------------------------------------
char *hashVote(int post_id, int vote_val, const char *salt, const char *pubkey_hex) {
    char combined[1024]; 
    snprintf(combined, sizeof(combined), "%d:%d:%s:%s", post_id, vote_val, salt, pubkey_hex);
    char *hash_output = safe_zalloc(HASH_LEN);
    sha256_hash(combined, strlen(combined), hash_output);
    return hash_output;
}

// ---------------------------------------------------------
// REGISTER USER
// ---------------------------------------------------------
Block *register_user(Block *prev, const void *payload, const char *priv, const char *pub) {
    if (state_get_user(pub)) {
        printf("⚠️ Utente già registrato.\n");
        return NULL;
    }
    // Nessun controllo sponsor. Chiunque può entrare.
    Block *b = mine_new_block(prev, ACT_REGISTER_USER, payload, pub, priv);
    if(b) {
        const PayloadRegister *reg = (const PayloadRegister*)payload;
        // La funzione state_add_new_user gestirà il saldo a 0 (tranne per GOD)
        state_add_new_user(pub, reg->username, reg->bio, reg->pic_url);
    }
    return b;
}

// ---------------------------------------------------------
// POST CONTENT
// ---------------------------------------------------------
Block *user_post(Block *prev, const void *payload, const char *priv, const char *pub) {
    UserState *u = state_get_user(pub);
    
    int current_cost = (int)(COSTO_POST * get_economy_multiplier());
    
    if (!u || u->token_balance < current_cost) {
        printf("[ECONOMY] ❌ Fondi insufficienti. Costo attuale: %d (Inflazione: %.2fx)\n", 
               current_cost, get_economy_multiplier());
        return NULL;
    }
    Block *b = mine_new_block(prev, ACT_POST_CONTENT, payload, pub, priv);
    if (b) {
        u->token_balance -= current_cost;
        post_index_add(b->index, pub);
        PostState *p = post_index_get(b->index);
        if(p) { p->pull += current_cost; p->created_at = b->timestamp; }
    }
    return b;
}

// ---------------------------------------------------------
// COMMIT VOTE
// ---------------------------------------------------------
Block *user_like(Block *prev, const void *payload, const char *priv, const char *pub) {
    UserState *u = state_get_user(pub);
    
    int current_cost = (int)(COSTO_POST * get_economy_multiplier());
    
    if (!u || u->token_balance < current_cost) {
        printf("[ECONOMY] ❌ Fondi insufficienti. Costo attuale: %d (Inflazione: %.2fx)\n", 
               current_cost, get_economy_multiplier());
        return NULL;
    }
    const PayloadReveal *raw = (const PayloadReveal *)payload; 
    PostState *post = post_index_get(raw->target_post_id);
    if (!post) { printf("Post non trovato.\n"); return NULL; }
    if (!check24hrs(post->created_at, time(NULL))) { printf("[TIME] Scaduto.\n"); return NULL; }

    PayloadCommit c_data = {0};
    c_data.target_post_id = raw->target_post_id;
    char *h = hashVote(raw->target_post_id, raw->vote_value, raw->salt_secret, pub);
    snprintf(c_data.vote_hash, HASH_LEN, "%s", h);
    
    Block *b = mine_new_block(prev, ACT_VOTE_COMMIT, &c_data, pub, priv);
    if (b) {
        u->token_balance -= current_cost;
        post->pull += current_cost;
        post_register_commit(raw->target_post_id, pub, h);
    }
    free(h);
    return b;
}

// ---------------------------------------------------------
// REVEAL VOTE
// ---------------------------------------------------------
Block *user_reveal(Block *prev, const void *payload, const char *priv, const char *pub) {
    const PayloadReveal *raw = (const PayloadReveal *)payload;
    PostState *post = post_index_get(raw->target_post_id);
    if (!post) return NULL;
    if (check24hrs(post->created_at, time(NULL))) { printf("[TIME] Troppo presto.\n"); return NULL; }

    char *h = hashVote(raw->target_post_id, raw->vote_value, raw->salt_secret, pub);
    if (!post_verify_commit(raw->target_post_id, pub, h)) {
        printf("[REVEAL] ❌ Hash mismatch!\n");
        free(h);
        return NULL;
    }
    free(h);

    Block *b = mine_new_block(prev, ACT_VOTE_REVEAL, payload, pub, priv);
    if (b) {
        post_register_reveal(raw->target_post_id, pub, raw->vote_value);
    }
    return b;
}

// ---------------------------------------------------------
// FOLLOW USER
// ---------------------------------------------------------
Block *user_follow(Block *prev, const void *payload, const char *priv, const char *pub) {
    if (!payload) return NULL;
    
    const PayloadFollow *req = (const PayloadFollow*)payload;

    if (!state_get_user(req->target_user_pubkey)) {
        printf("[FOLLOW] ❌ Errore: L'utente target non esiste.\n");
        return NULL;
    }

    if (strcmp(req->target_user_pubkey, pub) == 0) {
        printf("[FOLLOW] ❌ Non puoi seguire te stesso (Narcisismo non ammesso).\n");
        return NULL;
    }

    Block *b = mine_new_block(prev, ACT_FOLLOW_USER, payload, pub, priv);

    if (b) {
        state_toggle_follow(pub, req->target_user_pubkey);
        
        printf("[FOLLOW] ✅ Ora segui (o hai smesso di seguire) l'utente.\n");
    }

    return b;
}

// ---------------------------------------------------------
// COMMENTO POST
// ---------------------------------------------------------
Block *user_comment(Block *prev, const void *payload, const char *priv, const char *pub) {
    const PayloadComment *req = (const PayloadComment*)payload;

    if (!post_index_exists(req->target_post_id)) {
        printf("[COMMENT] ❌ Errore: Post #%d non trovato.\n", req->target_post_id);
        return NULL;
    }

    Block *b = mine_new_block(prev, ACT_POST_COMMENT, payload, pub, priv);

    if (b) {
        printf("[COMMENT] 💬 Commento registrato sul blocco #%d.\n", b->index);
        
        const PayloadComment *req = (const PayloadComment*)payload;
        post_register_comment(req->target_post_id, pub, req->content, b->timestamp);
    }
    return b;
}

// ---------------------------------------------------------
// PULIZIA MEMORIA
// ---------------------------------------------------------
void state_cleanup() {
    if (world_state) {
        map_destroy(world_state); 
        world_state = NULL;
    }
    
    for (int i = 0; i < REL_MAP_SIZE; i++) {
        RelationNode *curr = relation_map[i];
        while (curr) {
            RelationNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
    printf("[STATE] Memory cleaned up.\n");
}

// ---------------------------------------------------------
// AUTENTICAZIONE UTENTE (Challenge-Response)
// ---------------------------------------------------------
int user_login(const char *privkey_hex, const char *pubkey_hex) {
    UserState *u = state_get_user(pubkey_hex);
    if (!u) {
        printf("[LOGIN] ❌ Accesso Negato: Utente non registrato.\n");
        return 0;
    }

    char challenge_msg[512] = {0}; 

    for (int i = 0; i < 5; i++) {
        char *temp_word = getRandomWord(); 
        
        strncat(challenge_msg, temp_word, sizeof(challenge_msg) - strlen(challenge_msg) - 1);
        free(temp_word); 
    }

    printf("[DEBUG] Challenge: %s\n", challenge_msg);

    char signature[SIGNATURE_LEN];
    int is_valid = 0;
    
    // Firma e Verifica
    ecdsa_sign(privkey_hex, challenge_msg, signature);
    ecdsa_verify(pubkey_hex, challenge_msg, signature, &is_valid);

    if (is_valid) { 
        printf("[LOGIN] ✅ Benvenuto @%s!\n", u->username); 
        return 1; 
    } else { 
        printf("[LOGIN] ❌ Firma invalida.\n"); 
        return 0; 
    }
}

// ---------------------------------------------------------
// FINALIZZAZIONE POST E DISTRIBUZIONE PREMI
// ---------------------------------------------------------
Block *user_finalize(Block *prev, const void *payload, const char *priv, const char *pub) {
    const PayloadFinalize *req = (const PayloadFinalize*)payload;
    
    // Controlliamo se il post esiste e non è già chiuso
    PostState *post = post_index_get(req->target_post_id);
    if (!post) { printf("Post non trovato.\n"); return NULL; }
    if (post->finalized) { printf("Post già finalizzato.\n"); return NULL; }

    // Minamo il blocco
    Block *b = mine_new_block(prev, ACT_POST_FINALIZE, payload, pub, priv);
    
    if (b) {
        // Applichiamo subito l'effetto in RAM
        finalize_post_rewards(req->target_post_id);
    }
    return b;
}

// ---------------------------------------------------------
// TRASFERIMENTO TOKEN TRA UTENTI
// ---------------------------------------------------------
Block *user_transfer(Block *prev, const void *payload, const char *priv, const char *pub) {
    const PayloadTransfer *req = (const PayloadTransfer*)payload;
    
    // [FIX] Anti-Auto-Bonifico
    if (strcmp(pub, req->target_pubkey) == 0) {
        printf("❌ Non puoi inviare token a te stesso (inutile spam!).\n");
        return NULL;
    }
    UserState *sender = state_get_user(pub);
    UserState *receiver = state_get_user(req->target_pubkey);

    if (!sender || sender->token_balance < req->amount) {
        printf("❌ Fondi insufficienti (Hai: %d, Vuoi inviare: %d)\n", sender ? sender->token_balance : 0, req->amount);
        return NULL;
    }
    if (!receiver) {
        printf("❌ Destinatario non trovato sulla blockchain.\n");
        return NULL;
    }
    if (req->amount <= 0) {
        printf("❌ L'importo deve essere positivo.\n");
        return NULL;
    }

    Block *b = mine_new_block(prev, ACT_TRANSFER, payload, pub, priv);
    if (b) {
        sender->token_balance -= req->amount;
        receiver->token_balance += req->amount;
        printf("💸 Trasferimento completato! %d token da @%s a @%s.\n", req->amount, sender->username, receiver->username);
    }
    return b;
}

float get_economy_multiplier() {
    // Se siamo all'inizio, costo base
    if (global_tokens_circulating == 0) return 1.0;

    // Calcolo: (Token Circolanti / Token Totali)
    // Esempio: Se il 50% dei token sono fuori, il prezzo raddoppia (o aumenta secondo logica)
    float scarcity_ratio = (float)global_tokens_circulating / (float)GLOBAL_TOKEN_LIMIT;
    
    // Formula: 1.0 + (Ratio * 4). 
    // Se siamo al 0% -> 1x
    // Se siamo al 50% -> 3x
    // Se siamo al 100% -> 5x
    float multiplier = 1.0 + (scarcity_ratio * 4.0); 
    
    return multiplier;
}

void buy_tokens_sim(const char *user_pubkey, int amount_tokens) {
    UserState *u = state_get_user(user_pubkey);
    UserState *god = state_get_user(GOD_PUB_KEY);

    if (!u || !god) {
        printf("❌ Errore critico: Wallet non trovati.\n");
        return;
    }

    if (god->token_balance < amount_tokens) {
        printf("❌ La Banca Centrale (GOD) ha finito i token! Mercato chiuso.\n");
        return;
    }

    // Trasferimento Atomico (Simulato in RAM, poi andrebbe minato un blocco ACT_TRANSFER)
    god->token_balance -= amount_tokens;
    u->token_balance += amount_tokens;
    
    // Aggiorniamo il circolante (tecnicamente sono già "mintati" nel God wallet, 
    // ma per la tua logica di scarsità potresti considerare "circolanti" solo quelli degli utenti)
    global_tokens_circulating += amount_tokens; 

    printf("✅ Transazione approvata! Hai ricevuto %d Token.\n", amount_tokens);
}