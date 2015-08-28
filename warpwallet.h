#ifndef __WARPWALLET_H__
#define __WARPWALLET_H__

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>

#include <sys/types.h>
#include <sys/mman.h>

#include <emmintrin.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sysendian.h"

// Compute the 32-byte warpwallet privkey secret from a password and length
// Make sure password is allocated at least password_len + 1 bytes
void warpwallet(unsigned char *password, int password_len, unsigned char *out_str);


// The warpwallet function needs to know about these functions in advance
int crypto_scrypt(const uint8_t * passwd, size_t passwdlen,
    const uint8_t * salt, size_t saltlen, uint64_t N, uint32_t r, uint32_t p,
    uint8_t * buf, size_t buflen);

// Wrapper function for the PBKDF2_SHA256 function that scrypt expects
inline static void
PBKDF2_SHA256(const uint8_t * passwd, size_t passwdlen, const uint8_t * salt,
    size_t saltlen, uint64_t c, uint8_t * buf, size_t dkLen)
{
    PKCS5_PBKDF2_HMAC(passwd, passwdlen, salt, saltlen, c, EVP_sha256(), dkLen, buf);
}


#endif