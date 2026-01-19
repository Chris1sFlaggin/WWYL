#include "post_state.h"
#include "utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h> 

HashMap *global_post_index = NULL;

// --- DISTRUTTORE CUSTOM PER LA MAPPA ---
// Questa funzione viene chiamata automaticamente da map_destroy per ogni post
void free_post_state_wrapper(void *data) {
    PostState *p = (PostState*)data;
    if (!p) return;

    // 1. Libera la lista dei Commit
    CommitNode *c = p->commits;
    while (c) {
        CommitNode *temp = c;
        c = c->next;
        free(temp);
    }

    // 2. Libera la lista dei Reveal
    RevealNode *r = p->reveals;
    while (r) {
        RevealNode *temp = r;
        r = r->next;
        free(temp);
    }

    // 3. Libera la struttura principale
    free(p);
}

// --- INIT ---
void post_index_init() {
    // Configurazione Mappa:
    // Key: (void*)int (ID Post) -> Nessuna free necessaria (NULL)
    // Val: PostState* -> Usiamo il wrapper custom per pulire le liste!
    global_post_index = map_create(INITIAL_POST_MAP_SIZE, hash_int_direct, cmp_int_direct, NULL, free_post_state_wrapper);
}

// --- CLEANUP ---
void post_index_cleanup() {
    // Ora basta chiamare map_destroy e lei chiamerà free_post_state_wrapper per ogni elemento
    if (global_post_index) {
        map_destroy(global_post_index);
        global_post_index = NULL;
    }
}

// --- LOGICA AGGIUNTA/GET ---
void post_index_add(int post_id, const char *author) {
    PostState *p = safe_zalloc(sizeof(PostState));
    p->post_id = post_id;
    snprintf(p->author_pubkey, SIGNATURE_LEN, "%s", author);
    p->is_open = 1;
    p->created_at = time(NULL);
    
    // Cast dell'int a void* per usarlo come chiave
    map_put(global_post_index, (void*)(uintptr_t)post_id, p);
}

PostState *post_index_get(int post_id) {
    if (!global_post_index) return NULL;
    return (PostState *)map_get(global_post_index, (void*)(uintptr_t)post_id);
}

int post_index_exists(int post_id) {
    return post_index_get(post_id) != NULL;
}

char *post_index_author(int post_id) {
    PostState *p = post_index_get(post_id);
    return p ? p->author_pubkey : NULL;
}

// --- LOGICA VOTI ---
void post_register_commit(int post_id, const char *voter, const char *hash) {
    PostState *p = post_index_get(post_id);
    if (!p) return;

    // Check duplicati
    CommitNode *curr = p->commits;
    while(curr) {
        if (strcmp(curr->voter_pubkey, voter) == 0) return; 
        curr = curr->next;
    }

    CommitNode *node = safe_zalloc(sizeof(CommitNode));
    snprintf(node->voter_pubkey, SIGNATURE_LEN, "%s", voter);
    snprintf(node->vote_hash, HASH_LEN, "%s", hash);
    node->next = p->commits;
    p->commits = node;
}

int post_verify_commit(int post_id, const char *voter, const char *calculated_hash) {
    PostState *p = post_index_get(post_id);
    if (!p) return 0;

    CommitNode *curr = p->commits;
    while(curr) {
        if (strcmp(curr->voter_pubkey, voter) == 0) {
            return (strncmp(curr->vote_hash, calculated_hash, HASH_LEN) == 0);
        }
        curr = curr->next;
    }
    return 0;
}

void post_register_reveal(int post_id, const char *voter, int vote_val) {
    PostState *p = post_index_get(post_id);
    if (!p || !p->is_open) return;

    if (vote_val == 1) p->likes++;
    else if (vote_val == -1) p->dislikes++;

    RevealNode *node = safe_zalloc(sizeof(RevealNode));
    snprintf(node->voter_pubkey, SIGNATURE_LEN, "%s", voter);
    node->vote_value = vote_val;
    node->next = p->reveals;
    p->reveals = node;
}