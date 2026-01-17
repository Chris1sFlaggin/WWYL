#ifndef USER_H
#define USER_H

#include "wwyl.h"
#include "utils.h"
#include "wwyl_crypto.h"

Block *register_user(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex);
int user_login(Block *genesis, const char *privkey_hex, const char *pubkey_hex);

#endif