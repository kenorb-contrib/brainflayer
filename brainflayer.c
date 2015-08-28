/* Copyright (c) 2015 Ryan Castellucci, All Rights Reserved */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <openssl/sha.h>
#include <openssl/ripemd.h>

#include <omp.h>

#include "secp256k1/include/secp256k1.h"

#include "hash160.h"
#include "base58.h"
#include "timer.h"
#include "bloom.h"
#include "warpwallet.h"

static unsigned char *bloom;

// Creates the private key secret from a given input
// Implementation may vary based on brainwallet type (SHA256, scrypt/pbkdf2, etc)
static inline void make_secret(unsigned char *input_str, int input_len, unsigned char *output_str) {
#if defined BRAINWALLET
    SHA256(input_str, input_len, output_str);

#elif defined WARPWALLET
    warpwallet(input_str, input_len, output_str);

#else
    #error Brainwallet mode must be defined
#endif
}


static inline void display(unsigned char *input_word, unsigned char *privkey_secret, unsigned char *pubkey_hash, int compressed)
{
    #if DISPLAY_SECRET > 0
        fprintf(stdout, "%s:", hex_to_str(privkey_secret, 32, NULL));
    #endif

    #if DISPLAY_HASH160 > 0
        fprintf(stdout, "%s:", hex_to_str(pubkey_hash, RIPEMD160_DIGEST_LENGTH, NULL));
    #endif

    #if DISPLAY_WIF > 0
        fprintf(stdout, "%s:", hash256_to_wif(privkey_secret, compressed));
    #endif

    #if DISPLAY_ADDRESS > 0
        fprintf(stdout, "%s:", hash160_to_address(pubkey_hash));
    #endif

    #if DISPLAY_COMPR > 0
        fprintf(stdout, "%c:", (compressed > 0 ? 'c' : 'u'));
    #endif
    
    // Always display the input word
    fprintf(stdout, "%s\n", input_word);
}


int main(int argc, char **argv)
{
    struct timespec timer;
    double timer_delta;
    unsigned int line_count = 0;


    // Set STDOUT to line buffered, STDERR to unbuffered
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    if (argc == 2) {
        fprintf(stderr, "Loading bloom filter %s\n", argv[1]);

        timer = get_clock();
        bloom = bloom_open(argv[1]);

        fprintf(stderr, "Bloom filter loaded in %.2f seconds\n", get_clockdiff_s(timer));
        fprintf(stderr, "\n");

    } else {
        fprintf(stderr, "Incorrect number of arguments, expected 2, got %d\n", argc);
        fprintf(stderr, "USAGE:  %s  BLOOM_FILTER  <  WORD_LIST\n", argv[0]);
        exit(1);
    }


    secp256k1_start();

    // You can modify the number of threads by setting
    //   the environment variable OMP_NUM_THREADS
    fprintf(stderr, "Spawning up to %d threads\n", omp_get_max_threads());
    timer = get_clock();

    #pragma omp parallel shared(line_count)
    {
        char  *cur_line             = NULL;
        size_t cur_line_malloc_size = 0;
        unsigned int cur_line_len   = 0;
        unsigned int my_line_count  = 0;

        // pass2hash160 has been moved here
        // Relying on global variables leads to unexpected
        //   results when running in parallel
        unsigned char privkey_secret[32];
        unsigned char hash256[SHA256_DIGEST_LENGTH];
        hash160_t hash160_comp, hash160_uncomp;

        // We only need 65 bytes but we'll play it safe
        unsigned char pubkey[96];
        int pubkey_len = 0;

        for (;;) {
            #pragma omp critical (getline)
            {
                cur_line_len = getline(&cur_line, &cur_line_malloc_size, stdin);
            }

            // We can't break inside a critical section, so the check goes here
            if (cur_line_len == -1) { break; }
            ++my_line_count;

            unsigned int cur_line_len = strlen(cur_line);
            // Trim the newcur_line by overwriting with a NULL terminator
            cur_line[ --cur_line_len ] = 0;


            // Use our input line to create a private key secret value,
            //   then calculate the pubkey key based on it
            make_secret(cur_line, cur_line_len, privkey_secret);
            secp256k1_ecdsa_pubkey_create(pubkey, &pubkey_len, privkey_secret, 0);


            // SHA256 -> RIPEMD160 to make our uncompressed address hash
            SHA256(pubkey, 65, hash256);
            RIPEMD160(hash256, SHA256_DIGEST_LENGTH, hash160_uncomp.uc);

            // Make a compressed public key out of the uncompressed public key
            // Uncompressed has a byte[0] of 0x04 and a length of 65 bytes
            // Compressed has a byte[0] of 0x02 or 0x03 depending on if the
            //   Y coordinate of the pubkey is odd or even, new length is 33
            pubkey[0] = 0x02 | (pubkey[64] & 0x01);
            SHA256(pubkey, 33, hash256);
            RIPEMD160(hash256, SHA256_DIGEST_LENGTH, hash160_comp.uc);


            if (bloom_chk_hash160(bloom, hash160_uncomp.ul)) {
                #pragma omp critical (display)
                {
                    display(cur_line, privkey_secret, hash160_uncomp.uc, 0);
                }
            }

            if (bloom_chk_hash160(bloom, hash160_comp.ul)) {
                #pragma omp critical (display)
                {
                    display(cur_line, privkey_secret, hash160_comp.uc, 1);
                }
            }

        } // end of parallel loop

        // Cleanup
        free(cur_line);

        #pragma omp atomic
        line_count += my_line_count;
    }


#ifdef BENCHMARK
    timer_delta = get_clockdiff_s(timer);

    fprintf(stderr, "\n");
    fprintf(stderr, "Words: %u\n", (unsigned int)line_count);
    fprintf(stderr, "Time: %.1f sec\n", timer_delta);
    fprintf(stderr, "Words/sec: %.1f\n", line_count / timer_delta);
#endif

    secp256k1_stop();
    return 0;
}

/*  vim: set ts=4 sw=4 et ai si: */
