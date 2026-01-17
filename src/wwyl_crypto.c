#include "wwyl_crypto.h"
#include <openssl/err.h>
#include <openssl/pem.h>

// --- HELPER: Padding Hex (Invariato) ---
void pad_hex(const char* input_hex, char* output_fixed_64) {
    int len = strlen(input_hex);
    int padding = 64 - len;
    if (padding < 0) {
        strncpy(output_fixed_64, input_hex, 64);
    } else {
        for (int i = 0; i < padding; i++) output_fixed_64[i] = '0';
        strcpy(output_fixed_64 + padding, input_hex);
    }
    output_fixed_64[64] = '\0';
}

// --- HELPER: Gestione Errori OpenSSL ---
void handle_openssl_error() {
    ERR_print_errors_fp(stderr);
    abort();
}

// 1. Hashing SHA256 (Moderno con EVP)
void sha256_hash(const char *input, size_t len, char *output_hex) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    if(!mdctx) handle_openssl_error();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, input, len);
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);
    EVP_MD_CTX_free(mdctx);

    for(unsigned int i = 0; i < hash_len; i++) {
        sprintf(output_hex + (i * 2), "%02x", hash[i]);
    }
    output_hex[hash_len * 2] = '\0';
}

// --- HELPER: Costruzione Chiave da Hex (La parte difficile di OpenSSL 3.0) ---
// Converte la stringa Hex in un oggetto EVP_PKEY usabile
EVP_PKEY* get_pkey_from_hex(const char *priv_hex, const char *pub_hex) {
    EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
    EVP_PKEY *pkey = NULL;
    OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
    OSSL_PARAM *params = NULL;
    BIGNUM *bn_priv = NULL;
    unsigned char *pub_bytes = NULL;

    OSSL_PARAM_BLD_push_utf8_string(bld, "group", "secp256k1", 0);

    if (priv_hex) {
        BN_hex2bn(&bn_priv, priv_hex);
        OSSL_PARAM_BLD_push_BN(bld, "priv", bn_priv);
    }

    if (pub_hex) {
        // La chiave pubblica in hex deve essere convertita in octet string
        long pub_len = strlen(pub_hex) / 2;
        pub_bytes = OPENSSL_malloc(pub_len);
        for (long i = 0; i < pub_len; i++) {
            sscanf(pub_hex + 2*i, "%2hhx", &pub_bytes[i]);
        }
        OSSL_PARAM_BLD_push_octet_string(bld, "pub", pub_bytes, pub_len);
    }

    params = OSSL_PARAM_BLD_to_param(bld);
    
    EVP_PKEY_fromdata_init(pctx);
    EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_KEYPAIR, params);

    // Cleanup
    if (bn_priv) BN_free(bn_priv);
    if (pub_bytes) OPENSSL_free(pub_bytes);
    OSSL_PARAM_free(params);
    OSSL_PARAM_BLD_free(bld);
    EVP_PKEY_CTX_free(pctx);

    return pkey;
}

// 2. Generazione Keypair (OpenSSL 3.0 Way)
void generate_keypair(char *priv_hex_out, char *pub_hex_out) {
    // Generazione facile in una riga (Feature di OpenSSL 3.0)
    EVP_PKEY *pkey = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "secp256k1");
    if (!pkey) handle_openssl_error();

    // Estrazione Private Key (Big Number)
    BIGNUM *priv_bn = NULL;
    EVP_PKEY_get_bn_param(pkey, "priv", &priv_bn);
    char *hex_priv = BN_bn2hex(priv_bn);
    strcpy(priv_hex_out, hex_priv);

    // Estrazione Public Key (Octet String)
    unsigned char pub_buf[128];
    size_t pub_len = 0;
    // Otteniamo la dimensione
    EVP_PKEY_get_octet_string_param(pkey, "pub", NULL, 0, &pub_len); 
    // Otteniamo i dati
    EVP_PKEY_get_octet_string_param(pkey, "pub", pub_buf, sizeof(pub_buf), &pub_len);

    // Convertiamo buffer binario in Hex String
    for(size_t i=0; i<pub_len; i++) {
        sprintf(pub_hex_out + (i*2), "%02X", pub_buf[i]);
    }
    pub_hex_out[pub_len*2] = '\0';

    // Cleanup
    OPENSSL_free(hex_priv);
    BN_free(priv_bn);
    EVP_PKEY_free(pkey);
}

// 3. Firma ECDSA (EVP Interface)
void ecdsa_sign(const char *private_key_hex, const char *message, char *signature_hex) {
    EVP_PKEY *pkey = get_pkey_from_hex(private_key_hex, NULL);
    if (!pkey) handle_openssl_error();

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    
    // Inizializza firma con SHA256
    EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, pkey);
    
    // Calcola firma (output è in formato DER binario)
    size_t sig_len = 0;
    EVP_DigestSign(mdctx, NULL, &sig_len, (unsigned char*)message, strlen(message));
    unsigned char *sig_buf = OPENSSL_malloc(sig_len);
    EVP_DigestSign(mdctx, sig_buf, &sig_len, (unsigned char*)message, strlen(message));

    // Decodifica DER per estrarre R e S (per avere la stringa fissa 64+64)
    // Usiamo d2i_ECDSA_SIG che non è deprecata per il parsing dei dati grezzi
    const unsigned char *p = sig_buf;
    ECDSA_SIG *ecdsa_sig = d2i_ECDSA_SIG(NULL, &p, sig_len);
    
    const BIGNUM *r = ECDSA_SIG_get0_r(ecdsa_sig);
    const BIGNUM *s = ECDSA_SIG_get0_s(ecdsa_sig);

    char *r_hex = BN_bn2hex(r);
    char *s_hex = BN_bn2hex(s);
    
    char r_fixed[65], s_fixed[65];
    pad_hex(r_hex, r_fixed);
    pad_hex(s_hex, s_fixed);
    
    sprintf(signature_hex, "%s%s", r_fixed, s_fixed);

    // Cleanup
    OPENSSL_free(r_hex); OPENSSL_free(s_hex);
    OPENSSL_free(sig_buf);
    ECDSA_SIG_free(ecdsa_sig);
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
}

// 4. Verifica ECDSA
void ecdsa_verify(const char *public_key_hex, const char *message, const char *signature_hex, int *is_valid) {
    EVP_PKEY *pkey = get_pkey_from_hex(NULL, public_key_hex);
    if (!pkey) { *is_valid = 0; return; }

    // Ricostruzione Firma DER da R e S Hex
    char r_str[65], s_str[65];
    strncpy(r_str, signature_hex, 64); r_str[64] = '\0';
    strncpy(s_str, signature_hex+64, 64); s_str[64] = '\0';

    BIGNUM *r = NULL, *s = NULL;
    BN_hex2bn(&r, r_str);
    BN_hex2bn(&s, s_str);

    ECDSA_SIG *ecdsa_sig = ECDSA_SIG_new();
    ECDSA_SIG_set0(ecdsa_sig, r, s);

    // Encoding DER (necessario per EVP_DigestVerify)
    int der_len = i2d_ECDSA_SIG(ecdsa_sig, NULL);
    unsigned char *der_sig = OPENSSL_malloc(der_len);
    unsigned char *p = der_sig;
    i2d_ECDSA_SIG(ecdsa_sig, &p);

    // Verifica effettiva
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey);
    
    int result = EVP_DigestVerify(mdctx, der_sig, der_len, (unsigned char*)message, strlen(message));
    *is_valid = (result == 1);

    // Cleanup
    OPENSSL_free(der_sig);
    ECDSA_SIG_free(ecdsa_sig);
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);
}