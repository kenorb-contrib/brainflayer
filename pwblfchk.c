/* Copyright (c) 2026 */
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <sys/stat.h>

#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <openssl/ec.h>
#include <openssl/bn.h>
#include <openssl/obj_mac.h>

#include "bloom.h"
#include "hash160.h"
#include "mmapf.h"

static void usage(const char *prog) {
  fprintf(stderr,
    "Usage: %s [-o PASSWORD_BLOOM_FILE] [-v] passwords.txt address_bloom.blf\n"
    "  -o FILE   create/update password bloom file from passphrases\n"
    "  -v        print each possible hit (password + type)\n",
    prog);
}

static int passphrase_to_hash160s(const unsigned char *pass, size_t pass_sz,
                                  hash160_t *u_hash, hash160_t *c_hash) {
  int ok = 0;
  unsigned char priv[32];
  unsigned char pub_u[65];
  unsigned char pub_c[33];
  unsigned char digest[SHA256_DIGEST_LENGTH];
  size_t pub_u_sz, pub_c_sz;

  EC_KEY *key = NULL;
  EC_POINT *pub = NULL;
  BIGNUM *priv_bn = NULL;
  BIGNUM *order = NULL;
  BN_CTX *bn_ctx = NULL;
  const EC_GROUP *group = NULL;

  SHA256(pass, pass_sz, priv);

  key = EC_KEY_new_by_curve_name(NID_secp256k1);
  if (!key) { goto done; }
  group = EC_KEY_get0_group(key);
  if (!group) { goto done; }

  priv_bn = BN_bin2bn(priv, sizeof(priv), NULL);
  if (!priv_bn) { goto done; }

  order = BN_new();
  bn_ctx = BN_CTX_new();
  if (!order || !bn_ctx) { goto done; }
  if (EC_GROUP_get_order(group, order, bn_ctx) != 1) { goto done; }
  if (BN_is_zero(priv_bn) || BN_cmp(priv_bn, order) >= 0) { goto done; }

  if (EC_KEY_set_private_key(key, priv_bn) != 1) { goto done; }

  pub = EC_POINT_new(group);
  if (!pub) { goto done; }
  if (EC_POINT_mul(group, pub, priv_bn, NULL, NULL, bn_ctx) != 1) { goto done; }
  if (EC_KEY_set_public_key(key, pub) != 1) { goto done; }

  pub_u_sz = EC_POINT_point2oct(group, pub, POINT_CONVERSION_UNCOMPRESSED,
                                pub_u, sizeof(pub_u), bn_ctx);
  pub_c_sz = EC_POINT_point2oct(group, pub, POINT_CONVERSION_COMPRESSED,
                                pub_c, sizeof(pub_c), bn_ctx);
  if (pub_u_sz != 65 || pub_c_sz != 33) { goto done; }

  SHA256(pub_u, pub_u_sz, digest);
  RIPEMD160(digest, sizeof(digest), u_hash->uc);
  SHA256(pub_c, pub_c_sz, digest);
  RIPEMD160(digest, sizeof(digest), c_hash->uc);

  ok = 1;

done:
  BN_free(order);
  BN_free(priv_bn);
  EC_POINT_free(pub);
  EC_KEY_free(key);
  BN_CTX_free(bn_ctx);
  return ok;
}

int main(int argc, char **argv) {
  int opt;
  int ret;
  int vopt = 0;
  char *line = NULL;
  size_t line_sz = 0;
  size_t line_no = 0;
  size_t password_count = 0;
  size_t invalid_count = 0;
  size_t possible_passwords = 0;
  size_t possible_u_hits = 0;
  size_t possible_c_hits = 0;
  size_t pw_bloom_inserts = 0;
  char *pw_bloom_file = NULL;
  unsigned char *passwords_file;
  unsigned char *addr_bloom_file;
  FILE *pf = NULL;
  FILE *bf = NULL;
  unsigned char *addr_bloom = NULL;
  unsigned char *pw_bloom = NULL;
  mmapf_ctx addr_mmapf;
  struct stat sb;
  hash160_t u_hash, c_hash;

  while ((opt = getopt(argc, argv, "o:vh")) != -1) {
    switch (opt) {
      case 'o':
        pw_bloom_file = optarg;
        break;
      case 'v':
        vopt = 1;
        break;
      case 'h':
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if (argc - optind != 2) {
    usage(argv[0]);
    return 1;
  }

  passwords_file = (unsigned char *)argv[optind];
  addr_bloom_file = (unsigned char *)argv[optind + 1];

  if ((pf = fopen((char *)passwords_file, "r")) == NULL) {
    fprintf(stderr, "[!] Failed to open passwords file '%s'\n", passwords_file);
    return 1;
  }

  if ((ret = mmapf(&addr_mmapf, (char *)addr_bloom_file, BLOOM_SIZE, MMAPF_RNDRD)) != MMAPF_OKAY) {
    fprintf(stderr, "[!] Failed to open address bloom '%s': %s\n", addr_bloom_file, mmapf_strerror(ret));
    fclose(pf);
    return 1;
  } else if (addr_mmapf.mem == NULL) {
    fprintf(stderr, "[!] Got NULL pointer mapping address bloom\n");
    fclose(pf);
    return 1;
  }
  addr_bloom = addr_mmapf.mem;

  if (pw_bloom_file) {
    pw_bloom = malloc(BLOOM_SIZE);
    if (!pw_bloom) {
      fprintf(stderr, "[!] malloc failed for password bloom\n");
      fclose(pf);
      munmapf(&addr_mmapf);
      return 1;
    }

    if (stat(pw_bloom_file, &sb) == 0) {
      if (!S_ISREG(sb.st_mode) || sb.st_size != BLOOM_SIZE) {
        fprintf(stderr, "[!] Password bloom '%s' has wrong size (%ju != %d)\n",
                pw_bloom_file, (uintmax_t)sb.st_size, BLOOM_SIZE);
        fclose(pf);
        munmapf(&addr_mmapf);
        free(pw_bloom);
        return 1;
      }
      bf = fopen(pw_bloom_file, "r+");
      if (!bf || fread(pw_bloom, BLOOM_SIZE, 1, bf) != 1 || fseek(bf, 0, SEEK_SET) != 0) {
        fprintf(stderr, "[!] Failed to read password bloom '%s'\n", pw_bloom_file);
        if (bf) { fclose(bf); }
        fclose(pf);
        munmapf(&addr_mmapf);
        free(pw_bloom);
        return 1;
      }
    } else {
      bf = fopen(pw_bloom_file, "w+");
      if (!bf) {
        fprintf(stderr, "[!] Failed to create password bloom '%s'\n", pw_bloom_file);
        fclose(pf);
        munmapf(&addr_mmapf);
        free(pw_bloom);
        return 1;
      }
      memset(pw_bloom, 0, BLOOM_SIZE);
    }
  }

  while (getline(&line, &line_sz, pf) > 0) {
    size_t raw_sz;
    int hit_u, hit_c;

    ++line_no;
    raw_sz = strlen(line);
    while (raw_sz > 0 && isspace((unsigned char)line[raw_sz - 1])) { --raw_sz; }
    if (raw_sz == 0) { continue; }

    ++password_count;
    if (!passphrase_to_hash160s((unsigned char *)line, raw_sz, &u_hash, &c_hash)) {
      ++invalid_count;
      continue;
    }

    if (pw_bloom) {
      bloom_set_hash160(pw_bloom, u_hash.ul);
      bloom_set_hash160(pw_bloom, c_hash.ul);
      pw_bloom_inserts += 2;
    }

    hit_u = bloom_chk_hash160(addr_bloom, u_hash.ul);
    hit_c = bloom_chk_hash160(addr_bloom, c_hash.ul);

    if (hit_u || hit_c) {
      ++possible_passwords;
      if (hit_u) { ++possible_u_hits; }
      if (hit_c) { ++possible_c_hits; }
      if (vopt) {
        if (hit_u && hit_c) {
          printf("POSSIBLE\tline=%zu\ttype=both\tpassword=%.*s\n", line_no, (int)raw_sz, line);
        } else if (hit_u) {
          printf("POSSIBLE\tline=%zu\ttype=u\tpassword=%.*s\n", line_no, (int)raw_sz, line);
        } else {
          printf("POSSIBLE\tline=%zu\ttype=c\tpassword=%.*s\n", line_no, (int)raw_sz, line);
        }
      }
    }
  }

  if (pw_bloom_file) {
    fprintf(stderr, "[*] Writing password bloom to '%s'...\n", pw_bloom_file);
    if (fwrite(pw_bloom, BLOOM_SIZE, 1, bf) != 1) {
      fprintf(stderr, "[!] Failed to write password bloom '%s'\n", pw_bloom_file);
      fclose(pf);
      fclose(bf);
      munmapf(&addr_mmapf);
      free(pw_bloom);
      free(line);
      return 1;
    }
  }

  printf("REPORT\tpasswords_total=%zu\tinvalid=%zu\tpassword_bloom_hashes=%zu\tpossible_passwords=%zu\tpossible_u_hits=%zu\tpossible_c_hits=%zu\n",
         password_count, invalid_count, pw_bloom_inserts, possible_passwords, possible_u_hits, possible_c_hits);

  if (possible_passwords > 0) {
    printf("MAY_INTERSECT\tyes\tpossible_passwords=%zu\n", possible_passwords);
  } else {
    printf("MAY_INTERSECT\tno\tpossible_passwords=0\n");
  }

  if (bf) { fclose(bf); }
  if (pf) { fclose(pf); }
  if (pw_bloom) { free(pw_bloom); }
  if (line) { free(line); }
  munmapf(&addr_mmapf);

  return 0;
}
