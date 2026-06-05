/* Copyright (c) 2026 */
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include "bloom.h"
#include "mmapf.h"

static void usage(const char *prog) {
  fprintf(stderr,
          "Usage: %s [-q] bloom_a.blf bloom_b.blf\n"
          "  -q  quick mode: stop on first shared bit\n",
          prog);
}

int main(int argc, char **argv) {
  int opt;
  int quick = 0;
  int ret;
  size_t i;
  mmapf_ctx map_a, map_b;
  const uint64_t *a64, *b64;
  const size_t words = BLOOM_SIZE / sizeof(uint64_t);
  uint64_t common_bits = 0;
  uint64_t a_bits = 0;
  uint64_t b_bits = 0;
  uint64_t and_words_nonzero = 0;

  while ((opt = getopt(argc, argv, "qh")) != -1) {
    switch (opt) {
      case 'q':
        quick = 1;
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

  if ((ret = mmapf(&map_a, argv[optind], BLOOM_SIZE, MMAPF_RNDRD)) != MMAPF_OKAY || map_a.mem == NULL) {
    fprintf(stderr, "[!] failed to open bloom '%s': %s\n", argv[optind], mmapf_strerror(ret));
    return 1;
  }
  if ((ret = mmapf(&map_b, argv[optind + 1], BLOOM_SIZE, MMAPF_RNDRD)) != MMAPF_OKAY || map_b.mem == NULL) {
    fprintf(stderr, "[!] failed to open bloom '%s': %s\n", argv[optind + 1], mmapf_strerror(ret));
    munmapf(&map_a);
    return 1;
  }

  a64 = (const uint64_t *)map_a.mem;
  b64 = (const uint64_t *)map_b.mem;

  if (quick) {
    for (i = 0; i < words; ++i) {
      if ((a64[i] & b64[i]) != 0) {
        printf("MAY_INTERSECT\tyes\tmode=quick\n");
        munmapf(&map_b);
        munmapf(&map_a);
        return 0;
      }
    }
    printf("MAY_INTERSECT\tno\tmode=quick\n");
    munmapf(&map_b);
    munmapf(&map_a);
    return 0;
  }

  for (i = 0; i < words; ++i) {
    uint64_t w_and = a64[i] & b64[i];
    uint64_t w_a = a64[i];
    uint64_t w_b = b64[i];
    common_bits += (uint64_t)__builtin_popcountll(w_and);
    a_bits += (uint64_t)__builtin_popcountll(w_a);
    b_bits += (uint64_t)__builtin_popcountll(w_b);
    if (w_and != 0) { ++and_words_nonzero; }
  }

  printf("REPORT\tcommon_bits=%" PRIu64 "\ta_bits=%" PRIu64 "\tb_bits=%" PRIu64 "\tand_words_nonzero=%" PRIu64 "\n",
         common_bits, a_bits, b_bits, and_words_nonzero);

  if (common_bits > 0) {
    printf("MAY_INTERSECT\tyes\tmode=full\n");
  } else {
    printf("MAY_INTERSECT\tno\tmode=full\n");
  }

  munmapf(&map_b);
  munmapf(&map_a);
  return 0;
}
