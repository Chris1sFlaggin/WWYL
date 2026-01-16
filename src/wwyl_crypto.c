#include "wwyl_crypto.h"

// --- HELPER PRIVATO: Padding degli Hex ---
// Serve per garantire che le stringhe hex siano lunghe esattamente 64 caratteri (32 byte)
// Riempiendo di '0' a sinistra se il numero è piccolo.
void pad_hex(const char* input_hex, char* output_fixed_64) {
    int len = strlen(input_hex);
    int padding = 64 - len;
    
    if (padding < 0) { 
        // Caso raro: numero più grande di 256 bit (non dovrebbe succedere con secp256k1)
        strncpy(output_fixed_64, input_hex, 64);
    } else {
        // Aggiungi zeri
        for (int i = 0; i < padding; i++) output_fixed_64[i] = '0';
        // Copia il numero
        strcpy(output_fixed_64 + padding, input_hex);
    }
    output_fixed_64[64] = '\0';
}

// 1. Hashing SHA256 Sicuro
void sha256_hash(const char *input, size_t len, char *output_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input, len, hash);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }
    output_hex[64] = '\0'; 
}

// 2. Firma ECDSA (Gestione corretta della memoria e del padding)
void ecdsa_sign(const char *private_key_hex, const char *message, char *signature_hex) {
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
    
    // Converti Hex PrivKey -> BIGNUM
    BIGNUM *priv_bn = NULL;
    BN_hex2bn(&priv_bn, private_key_hex);
    EC_KEY_set_private_key(key, priv_bn);

    // Hashing del messaggio
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)message, strlen(message), hash);

    // Firma
    ECDSA_SIG *sig = ECDSA_do_sign(hash, SHA256_DIGEST_LENGTH, key);
    if (sig == NULL) {
        fprintf(stderr, "[CRYPTO ERROR] Firma fallita\n");
        return;
    }

    const BIGNUM *r, *s;
    ECDSA_SIG_get0(sig, &r, &s);

    // Conversione BN -> Hex String
    char *r_raw = BN_bn2hex(r);
    char *s_raw = BN_bn2hex(s);

    // Padding per avere lunghezza fissa
    char r_fixed[65], s_fixed[65];
    pad_hex(r_raw, r_fixed);
    pad_hex(s_raw, s_fixed);

    // Concatenazione finale (64 + 64 = 128 chars)
    sprintf(signature_hex, "%s%s", r_fixed, s_fixed);

    // Pulizia memoria (Fondamentale in C!)
    OPENSSL_free(r_raw);
    OPENSSL_free(s_raw);
    ECDSA_SIG_free(sig);
    BN_free(priv_bn);
    EC_KEY_free(key);
}

// 3. Verifica ECDSA (Ricostruzione corretta della chiave pubblica)
void ecdsa_verify(const char *public_key_hex, const char *message, const char *signature_hex, int *is_valid) {
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
    const EC_GROUP *group = EC_KEY_get0_group(key);
    EC_POINT *pub_point = EC_POINT_new(group);

    // Converti Hex PubKey -> EC_POINT
    // Nota: La PubKey deve essere in formato HEX non compresso (inizia con 04...)
    if (!EC_POINT_hex2point(group, public_key_hex, pub_point, NULL)) {
        fprintf(stderr, "[CRYPTO ERROR] Formato chiave pubblica errato\n");
        *is_valid = 0;
        goto cleanup;
    }
    EC_KEY_set_public_key(key, pub_point);

    // Parsing della firma (Split a metà esatta: 64 char per R, 64 char per S)
    char r_str[65], s_str[65];
    strncpy(r_str, signature_hex, 64);      r_str[64] = '\0';
    strncpy(s_str, signature_hex + 64, 64); s_str[64] = '\0';

    BIGNUM *r = NULL, *s = NULL;
    BN_hex2bn(&r, r_str);
    BN_hex2bn(&s, s_str);

    ECDSA_SIG *sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(sig, r, s); // set0 trasferisce la proprietà della memoria, non serve BN_free dopo

    // Verifica
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)message, strlen(message), hash);

    int result = ECDSA_do_verify(hash, SHA256_DIGEST_LENGTH, sig, key);
    *is_valid = (result == 1) ? 1 : 0;

    ECDSA_SIG_free(sig); // Libera anche r e s

cleanup:
    EC_POINT_free(pub_point);
    EC_KEY_free(key);
}

// 4. Generatore di Chiavi (Ti serve per creare utenti!)
void generate_keypair(char *priv_hex_out, char *pub_hex_out) {
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_generate_key(key);

    const BIGNUM *priv = EC_KEY_get0_private_key(key);
    const EC_POINT *pub = EC_KEY_get0_public_key(key);

    char *priv_raw = BN_bn2hex(priv);
    char *pub_raw = EC_POINT_point2hex(EC_KEY_get0_group(key), pub, POINT_CONVERSION_UNCOMPRESSED, NULL);

    strcpy(priv_hex_out, priv_raw);
    strcpy(pub_hex_out, pub_raw); // La PubKey inizierà con 04...

    OPENSSL_free(priv_raw);
    OPENSSL_free(pub_raw);
    EC_KEY_free(key);
}