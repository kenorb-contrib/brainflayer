#include "base58.h"
#include <assert.h>

#define DEBUG(...) do { fprintf(stderr, "[?] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while (0);

unsigned char *base58(unsigned char *in_str, int str_len) {
    static const unsigned char *base58_chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    static unsigned char *out_str = NULL;

    // log(256) / log(58) = ~1.4 output bytes per input byte
    int buffer_len = (str_len * 14) / 10 + 1;
    if ( out_str == NULL ) {
        out_str = malloc(buffer_len);

    } else {
        out_str = realloc(out_str, buffer_len);
    }

    // Blank the whole out_str, we'll be reversing this later
    memset(out_str, 0, buffer_len);

    // We need to keep count of the leading zero bytes
    // Bitcoin's base58 prepends a '1' for each leading zero on the input
    int leading_zeroes = 0;
    while ( in_str[leading_zeroes] == 0 ) { leading_zeroes++; }


    // Load the passed string as if it were base 256 with in_str[0] being the most significant byte
    mpz_t bignum;
    mpz_init(bignum);

    // GMP doesn't externally support loading base 256, so we'll simulate it
    int in_pos, out_pos;
    for (in_pos = 0; in_pos < str_len; in_pos++) {
        mpz_mul_ui(bignum, bignum, 256);
        mpz_add_ui(bignum, bignum, (unsigned int)in_str[in_pos]);
    }

    // Convert to base 58 by repeated division
    // This actually builds the output backwards, so we'll need to reverse it later
    unsigned long int rem;
    out_pos = 0;
    while ( mpz_cmp_ui(bignum, 0) > 0 ) {
        rem = mpz_tdiv_q_ui(bignum, bignum, 58);
        out_str[out_pos] = base58_chars[rem];
        out_pos++;
    }

    // Append those '1's we counted at the start
    for ( ; leading_zeroes > 0; leading_zeroes--) {
        out_str[out_pos] = '1';
        out_pos++;
    }


    // The string output is currently backwards, so reverse the result
    unsigned char temp;
    int front = 0, back = out_pos - 1;
    while ( front < back ) {
        temp = out_str[front];
        out_str[front] = out_str[back];
        out_str[back] = temp;
        front++; back--;
    }

    mpz_clear(bignum);
    return out_str;
}


unsigned char *hash160_to_address(const unsigned char *pubkey_hash) {
    static unsigned char sha_buffer[SHA256_DIGEST_LENGTH];
    // Network byte, 20-byte pubkey hash, 4-byte checksum
    static unsigned char out_str[1 + RIPEMD160_DIGEST_LENGTH + 4];

    // Add the network byte, pull in the pubkey hash
    out_str[0] = COIN_VERSION;
    memcpy(out_str + 1, pubkey_hash, RIPEMD160_DIGEST_LENGTH);

    // Double SHA256 to create the check digits
    SHA256(out_str, RIPEMD160_DIGEST_LENGTH + 1, sha_buffer);
    SHA256(sha_buffer, SHA256_DIGEST_LENGTH, sha_buffer);

    // Append the check digits
    memcpy(out_str + 1 + RIPEMD160_DIGEST_LENGTH, sha_buffer, 4);

    return base58(out_str, 1 + RIPEMD160_DIGEST_LENGTH + 4);
}


unsigned char *hash256_to_wif(const unsigned char *privkey_secret, int compressed) {
    static unsigned char sha_buffer[SHA256_DIGEST_LENGTH];
    // Secret byte, 32-byte secret, possible compression byte, 4-byte checksum
    static unsigned char out_str[1 + 32 + 1 + 4];

    // Force compressed to either 0/1 so we can deal with it in length calculations
    if ( compressed != 0 ) { compressed = 1; }

    memset(out_str, 0, 1 + 32 + 1 + 4);

    // Regardless of compression, the input always starts
    //   with the 1-byte secret key and the 32-byte privkey secret
    out_str[0] = SECRET_KEY;
    memcpy(out_str + 1, privkey_secret, 32);

    // If the privkey results in a compressed key, we need to append a 0x01
    if ( compressed ) {
        out_str[1 + 32] = 0x01;
        SHA256(out_str, 1 + 32 + 1, sha_buffer);
        SHA256(sha_buffer, SHA256_DIGEST_LENGTH, sha_buffer);

    } else {
        SHA256(out_str, 1 + 32, sha_buffer);
        SHA256(sha_buffer, SHA256_DIGEST_LENGTH, sha_buffer);
    }

    // 4-byte checksum location is 33 or 34 depending on compression setting
    memcpy(out_str + 1 + 32 + compressed, sha_buffer, 4);
    return base58(out_str, 1 + 32 + compressed + 4);
}


void hex_to_stream(FILE *stream, const unsigned char *str, int str_len, int add_newline) {
    int i = 0;
    for (i = 0; i < str_len; i++) {
        fprintf(stream, "%02x", str[i]);
    }

    if ( add_newline ) { fprintf(stream, "\n"); }
}


char* hex_to_str(unsigned char *binary_str, int str_len, unsigned char *out_str) {
    static unsigned char *hex_buffer = NULL;

    if ( !hex_buffer ) {
        hex_buffer = malloc(256);
    }

    // If an output buffer is not provided, use our static buffer
    // This ruins thread safety, so caveat emptor
    if ( !out_str ) {
        hex_buffer = realloc(hex_buffer, 2*str_len + 1);
        out_str = hex_buffer;
    }

    int pos;
    for (pos = 0; pos < str_len; pos++) {
        sprintf(out_str + 2*pos, "%02x", binary_str[pos]);
    }
    out_str[2*pos + 1] = 0;

    return out_str;
}