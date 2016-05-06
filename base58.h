#ifndef __BRAINFLAYER_BASE58_H_
#define __BRAINFLAYER_BASE58_H_

#ifndef COIN_VERSION
    // Network version from src/chainparams.cpp
    //   look for base58Prefixes[PUBKEY_ADDRESS]

    // Bitcoin main:    0
    // Bitcoin test:  111 (0x6F)
    // Litecoin main:  48 (0x30)
    #error Network version must be defined
#endif

#ifndef SECRET_KEY
    // The wallet import format secret key from src/chainparams.cpp
    //   look for base58Prefixes[SECRET_KEY]

    // Bitcoin main:  128 (0x80)
    // Bitcoin test:  239 (0xEF)
    // Litecoin main: 176 (0xB0)
    #error Secret key byte must be defined
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <gmp.h>

// Converts a binary string in_str of length str_len to base58
// The base58 string will be returned from an internal buffer
// Be sure to strcpy() the result if you expect it to not change
unsigned char *base58(unsigned char *in_str, int str_len);

// Takes a 20-byte RIPEMD160 hash (pubkey hash) and converts it to base58 address
// The base58 string will be returned from an internal buffer
// Be sure to strcpy() the result if you expect it to not change
unsigned char *hash160_to_address(const unsigned char *pubkey_hash);

// Takes a 32-byte binary string (privkey secret) and converts it to base58 wallet import format
// The base58 string will be returned from an internal buffer
// Be sure to strcpy() the result if you expect it to not change
unsigned char *hash256_to_wif(const unsigned char *privkey_secret, int compressed);


// Print the contents of str as hex to stream, optionally adding a newline
// Useful for debugging intermediate hashes for correctness
void bytes_to_stream(FILE *stream, const unsigned char *str, int str_len, int add_newline);


// Convert the input string to ASCII hex
// Returns a pointer to a buffer with the hex string
// If out_str is not NULL, store it there
// Useful for debugging intermediate hashes for correctness
char* bytes_to_str(unsigned char *binary_str, int str_len, unsigned char *out_str);


// Converts a single hex character into a byte value
unsigned char hexchr(unsigned char c);

// Converts an ASCII hex string to a byte array
void hex_to_bytes(unsigned char *hex_str, int str_len, unsigned char *out_str);

#endif