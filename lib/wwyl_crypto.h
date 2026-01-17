#ifndef WWYL_CRYPTO_H
#define WWYL_CRYPTO_H

#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/param_build.h>
#include <openssl/ec.h>
#include <openssl/err.h>  
#include <openssl/pem.h>  
#include <string.h>
#include <stdio.h>

#define SHA256_DIGEST_LENGTH 32

void sha256_hash(const char *input, size_t len, char *output_hex);
void generate_keypair(char *priv_hex_out, char *pub_hex_out);
void ecdsa_sign(const char *private_key_hex, const char *message, char *signature_hex);
void ecdsa_verify(const char *public_key_hex, const char *message, const char *signature_hex, int *is_valid);

#endif