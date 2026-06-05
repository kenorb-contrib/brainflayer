/*  Copyright (c) 2015 Ryan Castellucci, All Rights Reserved */
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <arpa/inet.h> /*  for ntohl/htonl */

#include <math.h> /* pow/exp */
#include <openssl/sha.h>

#include "hex.h"
#include "bloom.h"
#include "hash160.h"

const double k_hashes = 25;
const double m_bits   = 4294967296*2;
static const size_t hash160_hex_len = 40;
static const size_t cpub_hex_len = 66;
static const size_t upub_hex_len = 130;
static const size_t max_b58_address_len = 50;
/* Intermediate buffer for Base58 decoding math; final payload must still decode to 25 bytes. */
#define MAX_B58_DECODED_LEN 64

typedef struct input_types_s {
  int hash160_hex;
  int btc_address;
  int cpub_hex;
  int upub_hex;
} input_types_t;

static void usage(const char *prog) {
  fprintf(stderr,
          "[!] Usage: %s [-t TYPES] input.txt bloomfile.blf\n"
          "    TYPES: h=hash160-hex, a=bitcoin-base58-address, c=compressed-pubkey-hex, u=uncompressed-pubkey-hex\n",
          prog);
}

static int b58_value(unsigned char c) {
  static const char *alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
  const char *p = strchr(alphabet, c);
  return p ? (int)(p - alphabet) : -1;
}

static int parse_hash160_from_base58check(const unsigned char *str, size_t str_sz, hash160_t *hash) {
  size_t i, j, leading_ones = 0, leading_zeros = 0, payload_len, decoded_len;
  int carry, v;
  /* Bitcoin Base58Check addresses are usually 26-35 chars; 50 is a safe upper bound. */
  unsigned char decoded[MAX_B58_DECODED_LEN] = {0};
  unsigned char payload[25];
  unsigned char digest[SHA256_DIGEST_LENGTH];

  if (str_sz == 0 || str_sz > max_b58_address_len) { return 0; }

  for (i = 0; i < str_sz && str[i] == '1'; ++i) { ++leading_ones; }

  for (i = 0; i < str_sz; ++i) {
    v = b58_value(str[i]);
    if (v < 0) { return 0; }
    carry = v;
    for (j = sizeof(decoded); j-- > 0;) {
      carry += 58 * decoded[j];
      decoded[j] = carry & 0xff;
      carry >>= 8;
    }
    if (carry != 0) { return 0; }
  }

  for (i = 0; i < sizeof(decoded) && decoded[i] == 0; ++i) { ++leading_zeros; }

  decoded_len = sizeof(decoded) - leading_zeros;
  payload_len = leading_ones + decoded_len;
  if (payload_len != 25) { return 0; }
  if (leading_ones > sizeof(payload) || decoded_len > sizeof(payload) - leading_ones) { return 0; }

  memset(payload, 0, leading_ones);
  memcpy(payload + leading_ones, decoded + leading_zeros, decoded_len);

  SHA256(payload, 21, digest);
  SHA256(digest, SHA256_DIGEST_LENGTH, digest);
  if (memcmp(payload + 21, digest, 4) != 0) { return 0; }

  memcpy(hash->uc, payload + 1, sizeof(hash->uc));
  return 1;
}

static void format_enabled_types(const input_types_t *types, char *buf, size_t buf_sz) {
  size_t n = 0;
  if (types->hash160_hex && n + 1 < buf_sz) { buf[n++] = 'h'; }
  if (types->btc_address && n + 1 < buf_sz) { buf[n++] = 'a'; }
  if (types->cpub_hex && n + 1 < buf_sz) { buf[n++] = 'c'; }
  if (types->upub_hex && n + 1 < buf_sz) { buf[n++] = 'u'; }
  buf[n] = 0;
}

static int parse_hash160_from_pubkey_hex(const unsigned char *str, size_t str_sz, hash160_t *hash, int compressed) {
  size_t i;
  unsigned char pub[65];
  unsigned char digest[SHA256_DIGEST_LENGTH];
  const size_t expect = compressed ? cpub_hex_len : upub_hex_len;
  const size_t pub_sz = compressed ? 33 : 65;

  if (str_sz != expect) { return 0; }
  for (i = 0; i < str_sz; ++i) {
    if (!isxdigit(str[i])) { return 0; }
  }

  unhex((unsigned char *)str, str_sz, pub, pub_sz);
  if (compressed) {
    if (pub[0] != 0x02 && pub[0] != 0x03) { return 0; }
  } else if (pub[0] != 0x04) {
    return 0;
  }

  SHA256(pub, pub_sz, digest);
  RIPEMD160(digest, SHA256_DIGEST_LENGTH, hash->uc);
  return 1;
}

static int parse_hash160_line(char *line, hash160_t *hash, input_types_t *types) {
  size_t i, line_sz;
  unsigned char *p;

  p = (unsigned char *)line;
  while (*p && isspace(*p)) { ++p; }

  line_sz = strlen((char *)p);
  while (line_sz > 0 && isspace(p[line_sz - 1])) { --line_sz; }
  if (line_sz == 0) { return 0; }

  if (types->hash160_hex && line_sz == hash160_hex_len) {
    for (i = 0; i < hash160_hex_len; ++i) {
      if (!isxdigit(p[i])) { break; }
    }
    if (i == hash160_hex_len) {
      unhex(p, hash160_hex_len, hash->uc, sizeof(hash->uc));
      return 1;
    }
  }

  if (types->btc_address && parse_hash160_from_base58check(p, line_sz, hash)) {
    return 1;
  }

  if (types->cpub_hex && parse_hash160_from_pubkey_hex(p, line_sz, hash, 1)) {
    return 1;
  }

  if (types->upub_hex && parse_hash160_from_pubkey_hex(p, line_sz, hash, 0)) {
    return 1;
  }

  return 0;
}

int main(int argc, char **argv) {
  hash160_t hash;
  int i;
  int opt, idx;
  double pct;
  struct stat sb;
  unsigned char *bloom, *hashfile, *bloomfile;
  FILE *f, *b;
  size_t line_sz = 1024, line_ct = 0, line_no = 0;
  char *line;
  char *type_string = "ha"; /* default modes: hash160-hex + bitcoin address */
  input_types_t types = {0};

  double err_rate;
  int parsed;
  char enabled_types[8];

  while ((opt = getopt(argc, argv, "t:h")) != -1) {
    switch (opt) {
      case 't':
        type_string = optarg;
        break;
      case 'h':
      default:
        usage(argv[0]);
        exit(1);
    }
  }

  for (idx = 0; type_string[idx]; ++idx) {
    switch (type_string[idx]) {
      case 'h':
        if (types.hash160_hex) { fprintf(stderr, "[!] Duplicate type 'h'\n"); exit(1); }
        types.hash160_hex = 1;
        break;
      case 'a':
        if (types.btc_address) { fprintf(stderr, "[!] Duplicate type 'a'\n"); exit(1); }
        types.btc_address = 1;
        break;
      case 'c':
        if (types.cpub_hex) { fprintf(stderr, "[!] Duplicate type 'c'\n"); exit(1); }
        types.cpub_hex = 1;
        break;
      case 'u':
        if (types.upub_hex) { fprintf(stderr, "[!] Duplicate type 'u'\n"); exit(1); }
        types.upub_hex = 1;
        break;
      default:
        fprintf(stderr, "[!] Unknown input type '%c'\n", type_string[idx]);
        usage(argv[0]);
        exit(1);
    }
  }

  if (!types.hash160_hex && !types.btc_address && !types.cpub_hex && !types.upub_hex) {
    fprintf(stderr, "[!] No input types enabled.\n");
    usage(argv[0]);
    exit(1);
  }

  if (argc - optind != 2) {
    usage(argv[0]);
    exit(1);
  }

  hashfile = (unsigned char *)argv[optind];
  bloomfile = (unsigned char *)argv[optind + 1];
  format_enabled_types(&types, enabled_types, sizeof(enabled_types));

  if ((f = fopen(hashfile, "r")) == NULL) {
    fprintf(stderr, "[!] Failed to open hash160 file '%s'\n", hashfile);
    exit(1);
  }

  if ((bloom = malloc(BLOOM_SIZE)) == NULL) {
    fprintf(stderr, "[!] malloc failed (bloom filter)\n");
    exit(1);
  }

  if (stat(bloomfile, &sb) == 0) {
    if (!S_ISREG(sb.st_mode) || sb.st_size != BLOOM_SIZE) {
      fprintf(stderr, "[!] Bloom filter file '%s' is not the correct size (%ju != %d)\n", bloomfile, sb.st_size, BLOOM_SIZE);
      exit(1);
    }
    if ((b = fopen(bloomfile, "r+")) == NULL) {
      fprintf(stderr, "[!] Failed to open bloom filter file '%s' for read/write\n", bloomfile);
      exit(1);
    }
    fprintf(stderr, "[*] Reading existing bloom filter from '%s'...\n", bloomfile);
    if ((fread(bloom, BLOOM_SIZE, 1, b)) != 1 || (fseek(b, 0, SEEK_SET)) != 0) {
      fprintf(stderr, "[!] Failed to read existing boom filter from '%s'\n", bloomfile);
      exit(1);
    }
  } else {
    /*  Assume the file didn't exist - yes there is a race condition */
    if ((b = fopen(bloomfile, "w+")) == NULL) {
      fprintf(stderr, "[!] Failed to create bloom filter file '%s'\n", bloomfile);
      exit(1);
    }
    // start it empty
    fprintf(stderr, "[*] Initializing bloom filter...\n");
    memset(bloom, 0, BLOOM_SIZE);
  }

  if ((line = malloc(line_sz+1)) == NULL) {
    fprintf(stderr, "[!] malloc failed (line buffer)\n");
    exit(1);
  }

  i = 0;
  stat(hashfile, &sb);
  fprintf(stderr, "[*] Loading hash160s/addresses from '%s' \033[s  0.0%%", hashfile);
  while (getline(&line, &line_sz, f) > 0) {
    ++line_no;
    parsed = parse_hash160_line(line, &hash, &types);
    if (!parsed) {
      fprintf(stderr, "[!] Skipping invalid input at line %zu (expected types: %s)\n", line_no, enabled_types);
      continue;
    }
    ++line_ct;
    bloom_set_hash160(bloom, hash.ul);

    if ((++i & 0x3ffff) == 0) {
      pct = 100.0 * ftell(f) / sb.st_size;
      fprintf(stderr, "\033[u%5.1f%%", pct);
      fflush(stderr);
    }
  }
  fprintf(stderr, "\033[u 100.0%%\n");

  err_rate = pow(1 - exp(-k_hashes * line_ct / m_bits), k_hashes);
  fprintf(stderr, "[*] Loaded %zu hashes, false positive rate: ~%.3e (1 in ~%.3e)\n", line_ct, err_rate, 1/err_rate);

  fprintf(stderr, "[*] Writing bloom filter to '%s'...\n", bloomfile);
  if ((fwrite(bloom, BLOOM_SIZE, 1, b)) != 1) {
    fprintf(stderr, "[!] Failed to write bloom filter file '%s'\n", bloomfile);
    exit(1);
  }

  fprintf(stderr, "[+] Success!\n");
  return 0;
}

/*  vim: set ts=2 sw=2 et ai si: */
