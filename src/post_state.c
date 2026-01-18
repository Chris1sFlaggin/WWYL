#include "post_state.h"
#include "utils.h" 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

PostMap global_post_index;

unsigned long hash_int(int key, int size) { return key % size; }

void post_index_init() {
    global_post_index.size = INITIAL_POST_MAP_SIZE;
    global_post_index.count = 0;
    global_post_index.buckets = (PostStateNode**)safe_zalloc(global_post_index.size * sizeof(PostStateNode*));
}

void post_index_add(int post_id, const char *author) {
    unsigned long idx = hash_int(post_id, global_post_index.size);
    PostStateNode *node = (PostStateNode*)safe_zalloc(sizeof(PostStateNode));
    
    node->state.post_id = post_id;
    snprintf(node->state.author_pubkey, SIGNATURE_LEN, "%s", author);
    node->state.likes = 0;
    node->state.dislikes = 0;
    node->state.commits = NULL; 
    node->state.reveals = NULL;
    node->state.pull = 0;       
    node->state.is_open = 1;
    node->state.finalized = 0;
    node->state.created_at = time(NULL);

    node->next = global_post_index.buckets[idx];
    global_post_index.buckets[idx] = node;
    global_post_index.count++;
}

PostState *post_index_get(int post_id) {
    unsigned long idx = hash_int(post_id, global_post_index.size);
    PostStateNode *curr = global_post_index.buckets[idx];
    while (curr) {
        if (curr->state.post_id == post_id) return &curr->state;
        curr = curr->next;
    }
    return NULL;
}

int post_index_exists(int post_id) {
    return post_index_get(post_id) != NULL;
}

char *post_index_author(int post_id) {
    PostState *p = post_index_get(post_id);
    return p ? p->author_pubkey : NULL;
}

void post_register_commit(int post_id, const char *voter, const char *hash) {
    PostState *p = post_index_get(post_id);
    if (!p) return;

    CommitNode *curr = p->commits;
    while(curr) {
        if (strcmp(curr->voter_pubkey, voter) == 0) return; 
        curr = curr->next;
    }

    CommitNode *node = (CommitNode*)safe_zalloc(sizeof(CommitNode));
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

    RevealNode *node = (RevealNode*)safe_zalloc(sizeof(RevealNode));
    snprintf(node->voter_pubkey, SIGNATURE_LEN, "%s", voter);
    node->vote_value = vote_val;
    node->next = p->reveals;
    p->reveals = node;
}

void post_index_cleanup() {
    for(int i=0; i<global_post_index.size; i++) {
        PostStateNode *curr = global_post_index.buckets[i];
        while(curr) {
            CommitNode *c = curr->state.commits;
            while(c) { CommitNode *t = c; c = c->next; free(t); }
            RevealNode *r = curr->state.reveals;
            while(r) { RevealNode *t = r; r = r->next; free(t); }
            
            PostStateNode *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
    }
    free(global_post_index.buckets);
}