#include "user.h"

// ----------------------------------------------------------
// HELPER: CONTROLLA SE ESISTE NELLA CHAIN
// ----------------------------------------------------------
int is_user_registered(Block *genesis, const char *pubkey_hex) {
    Block *curr = genesis;
    while (curr != NULL) {
        // Cerchiamo un blocco di tipo REGISTRAZIONE fatto da questa PubKey
        if (curr->type == ACT_REGISTER_USER && strcmp(curr->sender_pubkey, pubkey_hex) == 0) {
            return 1; // Trovato!
        }
        curr = curr->next;
    }
    return 0; // Non trovato
}

// ---------------------------------------------------------
// CREA NUOVO UTENTE (MINING BLOCCO DI REGISTRAZIONE)
// ---------------------------------------------------------
Block *register_user(Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex) {
    if (is_user_registered(prev_block, pubkey_hex)) {
        printf("[USER] Utente già registrato con questa chiave pubblica.\n");
        return NULL;
    }
    const PayloadRegister *input = (const PayloadRegister *)payload;
    PayloadRegister reg_data;
    
    memset(&reg_data, 0, sizeof(PayloadRegister));

    snprintf(reg_data.username, sizeof(reg_data.username), "%s", input->username);
    snprintf(reg_data.bio, sizeof(reg_data.bio), "%s", input->bio);
    snprintf(reg_data.pic_url, sizeof(reg_data.pic_url), "%s", input->pic_url);

    printf("[USER] Registering username: %s\n", reg_data.username);

    return mine_new_block(prev_block, ACT_REGISTER_USER, &reg_data, pubkey_hex, privkey_hex);
}

// ----------------------------------------------------------
// LOGIN UTENTE COMPLETO
// ----------------------------------------------------------
int user_login(Block *genesis, const char *privkey_hex, const char *pubkey_hex) {
    // VERIFICA MATEMATICA
    const char *test_message = "LOGIN_VERIFICATION";
    char test_signature[SIGNATURE_LEN];
    int is_crypto_valid = 0;

    ecdsa_sign(privkey_hex, test_message, test_signature);
    ecdsa_verify(pubkey_hex, test_message, test_signature, &is_crypto_valid);

    if (!is_crypto_valid) {
        printf("[LOGIN] Chiavi non corrispondenti!\n");
        return 0;
    }

    if (!is_user_registered(genesis, pubkey_hex)) {
        printf("[LOGIN] Chiavi valide, ma nessun account trovato sulla blockchain.\n");
        return 0;
    }

    printf("[LOGIN] Benvenuto! Accesso effettuato.\n");
    return 1;
}

Block *user_follow(Block *genesis, Block *prev_block, const void *payload, const char *privkey_hex, const char *pubkey_hex) {
    const PayloadFollow *input = (const PayloadFollow*)payload;
    PayloadFollow reg_data;

    memset(&reg_data, 0, sizeof(PayloadFollow));
    snprintf(reg_data.target_user_pubkey, sizeof(reg_data.target_user_pubkey), "%s", input->target_user_pubkey);

    if (!is_user_registered(genesis, reg_data.target_user_pubkey)) {
        printf("[FOLLOW] Target inesistente.\n");
        return 0;
    }

    return mine_new_block(prev_block, ACT_FOLLOW_USER, &reg_data, pubkey_hex, privkey_hex);
}