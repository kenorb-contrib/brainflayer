/* Copyright (c) 2015 Ryan Castellucci, All Rights Reserved */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "bloom.h"

unsigned int bloom_chk_hash160(unsigned char *bloom, uint32_t *h)
{
    unsigned int t;
    t = BH00(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH01(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH02(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH03(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH04(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH05(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH06(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH07(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH08(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH09(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH10(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH11(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH12(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH13(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH14(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH15(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH16(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH17(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH18(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    t = BH19(h); if (BLOOM_GET_BIT(t) == 0) { return 0; }
    return 1;
}

void bloom_set_hash160(unsigned char *bloom, uint32_t *h)
{
    unsigned int t;
    t = BH00(h); BLOOM_SET_BIT(t);
    t = BH01(h); BLOOM_SET_BIT(t);
    t = BH02(h); BLOOM_SET_BIT(t);
    t = BH03(h); BLOOM_SET_BIT(t);
    t = BH04(h); BLOOM_SET_BIT(t);
    t = BH05(h); BLOOM_SET_BIT(t);
    t = BH06(h); BLOOM_SET_BIT(t);
    t = BH07(h); BLOOM_SET_BIT(t);
    t = BH08(h); BLOOM_SET_BIT(t);
    t = BH09(h); BLOOM_SET_BIT(t);
    t = BH10(h); BLOOM_SET_BIT(t);
    t = BH11(h); BLOOM_SET_BIT(t);
    t = BH12(h); BLOOM_SET_BIT(t);
    t = BH13(h); BLOOM_SET_BIT(t);
    t = BH14(h); BLOOM_SET_BIT(t);
    t = BH15(h); BLOOM_SET_BIT(t);
    t = BH16(h); BLOOM_SET_BIT(t);
    t = BH17(h); BLOOM_SET_BIT(t);
    t = BH18(h); BLOOM_SET_BIT(t);
    t = BH19(h); BLOOM_SET_BIT(t);
}

unsigned char *bloom_open(unsigned char *file_name)
{
    unsigned char *bloom = malloc(BLOOM_SIZE);
    if ( bloom == NULL ) { return NULL; }

    FILE *file_handle = fopen(file_name, "r");
    if ( file_handle == NULL ) {
        fprintf(stderr, "[!] Failed to open bloom filter file %s\n", file_name);
        exit(1);
    }

    // Read BLOOM_SIZE elements of one byte
    // If we didn't read exactly BLOOM_SIZE bytes, something went wrong
    // If we read one element of BLOOM_SIZE bytes, we may read up to
    //   an extra BLOOM_SIZE-1 bytes without having a way to detect it
    // http://stackoverflow.com/q/295994/477563

    // In addition, using fread() instead of mmap() is way faster under Cygwin
    if ( fread(bloom, 1, BLOOM_SIZE, file_handle) != BLOOM_SIZE ) {
        fprintf(stderr, "[!] Failed to read entire bloom filter from file %s\n", file_name);
        exit(1);
    }

    fclose(file_handle);

    return bloom;
}
