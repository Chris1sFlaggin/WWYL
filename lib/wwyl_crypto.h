#ifndef WWYL_CRYPTO_H
#define WWYL_CRYPTO_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/ec.h>

// Funzione di hashing SHA256 (restituisce hash esadecimale)
void sha256_hash(const char *input, size_t len, char *output_hex);

// Funzione di firma ECDSA (restituisce firma esadecimale)
void ecdsa_sign(const char *private_key_hex, const char *message, char *signature_hex);

// Funzione di verifica ECDSA (restituisce 1 se valida, 0 altrimenti)
void ecdsa_verify(const char *public_key_hex, const char *message, const char *signature_hex, int *is_valid);

// Funzione di generazione coppia di chiavi (privata e pubblica in esadecimale)
void generate_keypair(char *priv_hex_out, char *pub_hex_out);

#endif