#include "user.h"
#include "post_state.h" 
#include <string.h>

// --- VARIABILI GLOBALI ---
StateMap world_state;
RelationNode *relation_map[REL_MAP_SIZE];
long long global_tokens_circulating = 0;

// --- ECONOMY HELPERS ---
int mineTokens(long long amount) {
    if (global_tokens_circulating + amount > GLOBAL_TOKEN_LIMIT) return 0;
    global_tokens_circulating += amount;
    return 1;
}

// --- HASH FUNCTIONS ---
unsigned long hash_djb2(const char *str, int map_size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) hash = ((hash << 5) + hash) + c; 
    return hash % map_size;
}

unsigned long hash_rel(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) hash = ((hash << 5) + hash) + c;
    return hash % REL_MAP_SIZE;
}

// --- STATE MANAGEMENT ---
void state_init() {
    world_state.size = INITIAL_MAP_SIZE;
    world_state.count = 0;
    world_state.buckets = (StateNode **)safe_zalloc(world_state.size * sizeof(StateNode *));
}

UserState *state_get_user(const char *wallet_address) {
    unsigned long index = hash_djb2(wallet_address, world_state.size);
    StateNode *node = world_state.buckets[index];
    while (node != NULL) {
        if (strcmp(node->wallet_address, wallet_address) == 0) return &node->state;
        node = node->next;
    }
    return NULL;
}

void state_update_user(const char *wallet_address, const UserState *new_state) {
    unsigned long index = hash_djb2(wallet_address, world_state.size);
    StateNode *node = world_state.buckets[index];
    while(node) {
        if(strcmp(node->wallet_address, wallet_address) == 0) {
            node->state = *new_state; return;
        }
        node = node->next;
    }
    node = (StateNode *)safe_zalloc(sizeof(StateNode));
    snprintf(node->wallet_address, SIGNATURE_LEN, "%s", wallet_address);
    node->state = *new_state;
    node->next = world_state.buckets[index];
    world_state.buckets[index] = node;
    world_state.count++;
}

void state_add_new_user(const char *wallet_address, const char *username, const char *bio, const char *pic) {
    UserState u = {0};
    snprintf(u.wallet_address, SIGNATURE_LEN, "%s", wallet_address);
    if(username) snprintf(u.username, 32, "%s", username);
    if(bio) snprintf(u.bio, 64, "%s", bio);
    if(pic) snprintf(u.pic_url, 128, "%s", pic);
    
    if (mineTokens(WELCOME_BONUS)) {
        u.token_balance = WELCOME_BONUS;
    } else {
        u.token_balance = 0; 
    }
    state_update_user(wallet_address, &u);
    printf("[STATE] New User: %s (Bal: %d)\n", u.username, u.token_balance);
}

// --- FOLLOW LOGIC ---
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

// --- LOGICA DI GIOCO (REWARDS & STREAK) ---
void finalize_post_rewards(int post_id) {
    PostState *p = post_index_get(post_id);
    if (!p || p->finalized) return;

    // Se Likes >= Dislikes, l'autore "Vince" l'approvazione (Streak)
    int winning_vote = (p->likes >= p->dislikes) ? 1 : -1;
    
    int winners_count = 0;
    RevealNode *curr = p->reveals;
    while(curr) {
        if (curr->vote_value == winning_vote) winners_count++;
        curr = curr->next;
    }

    // 1. GESTIONE AUTORE (Streak Rewards)
    UserState *author = state_get_user(p->author_pubkey);
    if (author) {
        if (winning_vote == 1) { 
            // AUMENTO STREAK
            author->current_streak++;
            if (author->current_streak > author->best_streak) {
                author->best_streak = author->current_streak;
            }

            // PAGAMENTO BONUS (1->1, 2->2, 3->3...)
            // Questi soldi sono CONIATI (Minted), non presi dal pool
            int bonus = author->current_streak;
            if (mineTokens(bonus)) {
                author->token_balance += bonus;
                printf("🔥 [STREAK] Author Streak x%d! Minted +%d Tokens. (Bal: %d)\n", 
                       author->current_streak, bonus, author->token_balance);
            } else {
                printf("⚠️ [STREAK] Global Token Limit Reached! No bonus for streak.\n");
            }

        } else {
            // RESET STREAK
            printf("❄️ [STREAK] Author Streak Reset (Was %d). Post rejected.\n", author->current_streak);
            author->current_streak = 0;
        }
    }

    // 2. GESTIONE PIATTO (Votanti)
    if (winners_count > 0 && p->pull > 0) {
        int reward = p->pull / winners_count;
        curr = p->reveals;
        while(curr) {
            if (curr->vote_value == winning_vote) {
                UserState *u = state_get_user(curr->voter_pubkey);
                // Questi soldi sono RIDISTRIBUITI dal pool (già mintati)
                if (u) {
                    u->token_balance += reward;
                    printf("💰 [PAYOUT] Voter %.8s... won %d tokens!\n", u->wallet_address, reward);
                }
            }
            curr = curr->next;
        }
    } else {
        printf("💀 [PAYOUT] No winners or empty pool. Tokens burned/locked.\n");
    }

    p->finalized = 1;
    p->is_open = 0;
    printf("[ECONOMY] Post #%d Finalized. Pool: %d -> Distributed.\n", post_id, p->pull);
}

// --- REBUILD ---
void rebuild_state_from_chain(Block *genesis) {
    printf("[STATE] Replaying Blockchain History...\n");
    global_tokens_circulating = 0; 
    
    Block *curr = genesis;
    while(curr != NULL) {
        if (curr->type == ACT_REGISTER_USER) {
            state_add_new_user(curr->sender_pubkey, curr->data.registration.username, NULL, NULL);
        }
        else if (curr->type == ACT_POST_CONTENT) {
            post_index_add(curr->index, curr->sender_pubkey);
            UserState *u = state_get_user(curr->sender_pubkey);
            if(u && u->token_balance >= COSTO_POST) {
                u->token_balance -= COSTO_POST;
                PostState *p = post_index_get(curr->index);
                if(p) {
                    p->pull += COSTO_POST;
                    p->created_at = curr->timestamp; 
                }
            }
        }
        else if (curr->type == ACT_VOTE_COMMIT) {
            int pid = curr->data.commit.target_post_id;
            post_register_commit(pid, curr->sender_pubkey, curr->data.commit.vote_hash);
            UserState *u = state_get_user(curr->sender_pubkey);
            if(u && u->token_balance >= COSTO_VOTO) {
                u->token_balance -= COSTO_VOTO;
                PostState *p = post_index_get(pid);
                if(p) p->pull += COSTO_VOTO;
            }
        }
        else if (curr->type == ACT_VOTE_REVEAL) {
            int pid = curr->data.reveal.target_post_id;
            post_register_reveal(pid, curr->sender_pubkey, curr->data.reveal.vote_value);
        }
        else if (curr->type == ACT_FOLLOW_USER) {
            state_toggle_follow(curr->sender_pubkey, curr->data.follow.target_user_pubkey);
        }
        curr = curr->next;
    }
    printf("[STATE] Replay Complete.\n");
}

int check24hrs(time_t post_timestamp, time_t current_time) {
    double diff = difftime(current_time, post_timestamp);
    return (diff >= 0 && diff <= 86400);
}

// Hash senza timestamp (per test)
char *hashVote(int post_id, int vote_val, const char *salt, const char *pubkey_hex) {
    char combined[1024]; 
    snprintf(combined, sizeof(combined), "%d:%d:%s:%s", 
             post_id, vote_val, salt, pubkey_hex);
    char *hash_output = (char *)safe_zalloc(HASH_LEN);
    sha256_hash(combined, strlen(combined), hash_output);
    return hash_output;
}

// --- AZIONI MINING ---

Block *register_user(Block *prev, const void *payload, const char *priv, const char *pub) {
    if (state_get_user(pub)) return NULL;
    Block *b = mine_new_block(prev, ACT_REGISTER_USER, payload, pub, priv);
    if(b) {
        const PayloadRegister *reg = (const PayloadRegister*)payload;
        state_add_new_user(pub, reg->username, reg->bio, reg->pic_url);
    }
    return b;
}

Block *user_post(Block *prev, const void *payload, const char *priv, const char *pub) {
    UserState *u = state_get_user(pub);
    if (!u || u->token_balance < COSTO_POST) {
        printf("[ECONOMY] ❌ Fondi insufficienti per postare (Hai %d, serve %d).\n", u ? u->token_balance : 0, COSTO_POST);
        return NULL;
    }
    Block *b = mine_new_block(prev, ACT_POST_CONTENT, payload, pub, priv);
    if (b) {
        u->token_balance -= COSTO_POST;
        post_index_add(b->index, pub);
        PostState *p = post_index_get(b->index);
        if(p) {
            p->pull += COSTO_POST;
            p->created_at = b->timestamp;
        }
    }
    return b;
}

Block *user_like(Block *prev, const void *payload, const char *priv, const char *pub) {
    UserState *u = state_get_user(pub);
    if (!u || u->token_balance < COSTO_VOTO) {
        printf("[ECONOMY] ❌ Fondi insufficienti per votare.\n");
        return NULL;
    }
    
    const PayloadReveal *raw = (const PayloadReveal *)payload; 
    PostState *post = post_index_get(raw->target_post_id);
    if (!post) { printf("Post non trovato.\n"); return NULL; }

    if (!check24hrs(post->created_at, time(NULL))) {
        printf("[TIME] ⏳ Tempo scaduto per votare (Commit phase ended).\n");
        return NULL;
    }

    PayloadCommit c_data = {0};
    c_data.target_post_id = raw->target_post_id;
    // Hash senza timestamp
    char *h = hashVote(raw->target_post_id, raw->vote_value, raw->salt_secret, pub);
    snprintf(c_data.vote_hash, HASH_LEN, "%s", h);
    
    Block *b = mine_new_block(prev, ACT_VOTE_COMMIT, &c_data, pub, priv);
    if (b) {
        u->token_balance -= COSTO_VOTO;
        post->pull += COSTO_VOTO;
        post_register_commit(raw->target_post_id, pub, h);
    }
    free(h);
    return b;
}

Block *user_reveal(Block *prev, const void *payload, const char *priv, const char *pub) {
    const PayloadReveal *raw = (const PayloadReveal *)payload;
    PostState *post = post_index_get(raw->target_post_id);
    if (!post) return NULL;

    if (check24hrs(post->created_at, time(NULL))) {
        printf("[TIME] ⏳ Troppo presto per rivelare! Le scommesse sono ancora aperte.\n");
        return NULL;
    }

    char *h = hashVote(raw->target_post_id, raw->vote_value, raw->salt_secret, pub);
    if (!post_verify_commit(raw->target_post_id, pub, h)) {
        printf("[REVEAL] ❌ Hash mismatch! Stai mentendo sul tuo voto originale.\n");
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

Block *user_follow(Block *prev, const void *payload, const char *priv, const char *pub) {
    return mine_new_block(prev, ACT_FOLLOW_USER, payload, pub, priv);
}

Block *user_comment(Block *prev, const void *payload, const char *priv, const char *pub) {
    return mine_new_block(prev, ACT_POST_COMMENT, payload, pub, priv);
}

void state_cleanup() {
    for (int i = 0; i < world_state.size; i++) {
        StateNode *curr = world_state.buckets[i];
        while (curr != NULL) {
            StateNode *temp = curr;
            curr = curr->next;
            free(temp);
        }
    }
    free(world_state.buckets);
    
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
    // Controllo Esistenza Utente
    UserState *u = state_get_user(pubkey_hex);
    if (!u) {
        printf("[LOGIN] ❌ Accesso Negato: Utente non registrato.\n");
        return 0;
    }

    // Sfida Crittografica (Challenge)
    // Dovrebbe cambiare ogni volta
    const char *challenge_msg = "WWYL_LOGIN_VERIFICATION_TOKEN";
    char signature[SIGNATURE_LEN];
    int is_valid = 0;

    // Firma il messaggio con la PrivKey fornita
    ecdsa_sign(privkey_hex, challenge_msg, signature);

    // Verifica la firma contro la PubKey dell'utente
    ecdsa_verify(pubkey_hex, challenge_msg, signature, &is_valid);

    if (is_valid) {
        printf("[LOGIN] ✅ Benvenuto @%s! (Identity Verified)\n", u->username);
        return 1; // Success
    } else {
        printf("[LOGIN] ❌ Accesso Negato: Chiave Privata errata per questo utente!\n");
        return 0; // Fail
    }
}
