#include "user.h"

// ---------------------------------------------------
// VARIABILI GLOBALI
// ---------------------------------------------------
StateMap world_state;
RelationNode *relation_map[REL_MAP_SIZE];

// ---------------------------------------------------
// INIZIALIZZAZIONE DELLO STATO
// ---------------------------------------------------
void state_init() {
    world_state.size = INITIAL_MAP_SIZE;
    world_state.count = 0;
    world_state.buckets = (StateNode **)safe_zalloc(world_state.size * sizeof(StateNode *));
}

// ---------------------------------------------------
// HASH FUNCTIONS
// ---------------------------------------------------
unsigned long hash_djb2(const char *str, int map_size) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; 
    return hash % map_size;
}

unsigned long hash_rel(const char *key) {
    unsigned long hash = 5381;
    int c;
    while ((c = *key++)) hash = ((hash << 5) + hash) + c;
    return hash % REL_MAP_SIZE;
}

// ---------------------------------------------------
// STATE LOGIC (Follow Toggle)
// ---------------------------------------------------
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
    
    // 1. Cerchiamo se la relazione esiste (Unfollow)
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

    // 2. Non trovata (Follow)
    RelationNode *node = (RelationNode*)safe_zalloc(sizeof(RelationNode));
    strcpy(node->key, key);
    node->next = relation_map[idx];
    relation_map[idx] = node;

    if (u_follower) u_follower->following_count++;
    if (u_target) u_target->followers_count++;
}

// ---------------------------------------------------
// RESIZE MAPPA
// ---------------------------------------------------
void state_resize() {
    int old_size = world_state.size;
    int new_size = old_size * 2;
    
    printf("[STATE] Resizing Map: %d -> %d buckets.\n", old_size, new_size);

    StateNode **new_buckets = (StateNode **)safe_zalloc(new_size * sizeof(StateNode *));

    for (int i = 0; i < old_size; i++) {
        StateNode *curr = world_state.buckets[i];
        while (curr != NULL) {
            StateNode *next = curr->next;
            unsigned long new_index = hash_djb2(curr->wallet_address, new_size);
            curr->next = new_buckets[new_index];
            new_buckets[new_index] = curr;
            curr = next;
        }
    }

    free(world_state.buckets);
    world_state.buckets = new_buckets;
    world_state.size = new_size;
}

// ---------------------------------------------------
// API UTENTE / STATO
// ---------------------------------------------------
UserState *state_get_user(const char *wallet_address) {
    unsigned long index = hash_djb2(wallet_address, world_state.size);
    StateNode *node = world_state.buckets[index];

    while (node != NULL) {
        if (strcmp(node->wallet_address, wallet_address) == 0) {
            return &node->state;
        }
        node = node->next;
    }
    return NULL;
}

void state_update_user(const char *wallet_address, const UserState *new_state) {
    if ((float)world_state.count / world_state.size >= 0.75) {
        state_resize();
    }

    unsigned long index = hash_djb2(wallet_address, world_state.size);
    StateNode *node = world_state.buckets[index];

    while (node != NULL) {
        if (strcmp(node->wallet_address, wallet_address) == 0) {
            node->state = *new_state;
            return;
        }
        node = node->next;
    }

    StateNode *new_node = (StateNode *)safe_zalloc(sizeof(StateNode));
    snprintf(new_node->wallet_address, SIGNATURE_LEN, "%s", wallet_address);
    new_node->state = *new_state;
    new_node->next = world_state.buckets[index];
    world_state.buckets[index] = new_node;
    world_state.count++;
}

// [CORRETTO] Ora salviamo i dati nello stato!
void state_add_new_user(const char *wallet_address, const char *username, const char *bio, const char *pic) {
    UserState u = {0};
    snprintf(u.wallet_address, SIGNATURE_LEN, "%s", wallet_address);
    
    // Copia dei dati anagrafici nello stato
    if (username) snprintf(u.username, sizeof(u.username), "%s", username);
    if (bio) snprintf(u.bio, sizeof(u.bio), "%s", bio);
    if (pic) snprintf(u.pic_url, sizeof(u.pic_url), "%s", pic);

    u.token_balance = WELCOME_BONUS; 
    u.current_streak = 0;
    u.best_streak = 0;
    u.followers_count = 0;
    u.following_count = 0;
    u.total_posts = 0;
    
    state_update_user(wallet_address, &u);
    printf("[STATE] New User State created: %s (@%s)\n", wallet_address, u.username);
}

void rebuild_state_from_chain(Block *genesis) {
    printf("[STATE] Rebuilding World State from Ledger...\n");
    Block *curr = genesis;
    int count = 0;

    while(curr != NULL) {
        if (curr->type == ACT_REGISTER_USER) {
            state_add_new_user(
                curr->sender_pubkey, 
                curr->data.registration.username, 
                curr->data.registration.bio, 
                curr->data.registration.pic_url
            );
            count++;
        }
        if (curr->type == ACT_FOLLOW_USER) {
            state_toggle_follow(curr->sender_pubkey, curr->data.follow.target_user_pubkey);
        }
        if (curr->type == ACT_POST_CONTENT) {
            post_index_add(curr->index, curr->sender_pubkey);
        }
        if (curr->type == ACT_VOTE_REVEAL) {
            post_index_vote(curr->data.reveal.target_post_id, curr->data.reveal.vote_value);
        }
        curr = curr->next;
    }
    printf("[STATE] State Rebuilt. %d users active.\n", count);
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

// ---------------------------------------------------
// FUNZIONI BLOCCO UTENTE
// ---------------------------------------------------

Block *register_user(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex) {
    if (state_get_user(pubkey_hex) != NULL) {
        printf("[USER] Utente già registrato (Map Check).\n");
        return NULL;
    }

    const PayloadRegister *input = (const PayloadRegister *)payload;
    PayloadRegister reg_data;
    memset(&reg_data, 0, sizeof(PayloadRegister));

    snprintf(reg_data.username, sizeof(reg_data.username), "%s", input->username);
    snprintf(reg_data.bio, sizeof(reg_data.bio), "%s", input->bio);
    snprintf(reg_data.pic_url, sizeof(reg_data.pic_url), "%s", input->pic_url);

    printf("[USER] Mining registration block for: %s\n", reg_data.username);

    Block *new_block = mine_new_block(prev_block, ACT_REGISTER_USER, &reg_data, pubkey_hex, privkey_hex);

    if (new_block) {
        state_add_new_user(pubkey_hex, reg_data.username, reg_data.bio, reg_data.pic_url);
    }
    return new_block;
}

int user_login(const char *privkey_hex, const char *pubkey_hex) {
    const char *test_message = "LOGIN_VERIFICATION";
    char test_signature[SIGNATURE_LEN];
    int is_crypto_valid = 0;

    ecdsa_sign(privkey_hex, test_message, test_signature);
    ecdsa_verify(pubkey_hex, test_message, test_signature, &is_crypto_valid);

    if (!is_crypto_valid) {
        printf("[LOGIN] Chiavi non corrispondenti!\n");
        return 0;
    }

    UserState *u = state_get_user(pubkey_hex);
    if (u == NULL) {
        printf("[LOGIN] Chiavi valide, ma utente non trovato nello Stato.\n");
        return 0;
    }

    printf("[LOGIN] Benvenuto %s! Balance: %d Token.\n", u->username, u->token_balance);
    return 1;
}

Block *user_follow(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex) {
    const PayloadFollow *input = (const PayloadFollow*)payload;
    
    if (!state_get_user(input->target_user_pubkey)) {
        printf("[FOLLOW] Errore: Target inesistente.\n");
        return NULL;
    }
    
    if (strcmp(input->target_user_pubkey, pubkey_hex) == 0) {
        printf("[FOLLOW] Errore: Auto-follow non permesso.\n");
        return NULL;
    }

    PayloadFollow follow_data;
    memset(&follow_data, 0, sizeof(PayloadFollow));
    snprintf(follow_data.target_user_pubkey, sizeof(follow_data.target_user_pubkey), "%s", input->target_user_pubkey);

    Block *new_block = mine_new_block(prev_block, ACT_FOLLOW_USER, &follow_data, pubkey_hex, privkey_hex);

    if (new_block) {
        state_toggle_follow(pubkey_hex, follow_data.target_user_pubkey);
        int is_following = state_check_follow_status(pubkey_hex, follow_data.target_user_pubkey);
        printf("[FOLLOW] Success! Status: %s\n", is_following ? "FOLLOWING" : "UNFOLLOWED");
    }

    return new_block;
}