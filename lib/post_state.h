#ifndef POST_STATE_H
#define POST_STATE_H

#include "wwyl.h"
#include "map.h"

// Struttura Nodo Hashmap Post
typedef struct PostStateNode {
    int post_id; 
    PostState state;
    struct PostStateNode *next;
} PostStateNode;

typedef struct {
    PostStateNode **buckets;
    int size;
    int count;
} PostMap;

extern HashMap *global_post_index;

// API Indice
void post_index_init();
void post_index_add(int post_id, const char *author);
void post_index_cleanup();
PostState *post_index_get(int post_id);
int post_index_exists(int post_id);
char *post_index_author(int post_id);

// API Voti
void post_register_commit(int post_id, const char *voter, const char *hash);
int post_verify_commit(int post_id, const char *voter, const char *calculated_hash);
void post_register_reveal(int post_id, const char *voter, int vote_val); 
void post_register_comment(int post_id, const char *author, const char *content, time_t timestamp);

#endif