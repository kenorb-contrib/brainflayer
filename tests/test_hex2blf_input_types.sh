#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

TMP_DIR="$(mktemp -d /tmp/brainflayer-hex2blf-test-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

assert_match() {
  local mode="$1"
  local input_line="$2"
  local expected_hash="$3"
  local name="$4"
  local in_file="$TMP_DIR/${name}.txt"
  local bloom_file="$TMP_DIR/${name}.blf"
  local out_file="$TMP_DIR/${name}.out"

  printf '%s\n' "$input_line" > "$in_file"
  ./hex2blf -t "$mode" "$in_file" "$bloom_file" >/dev/null 2>&1
  printf '%s\n' "$expected_hash" | ./blfchk "$bloom_file" > "$out_file"

  if ! grep -qx "$expected_hash" "$out_file"; then
    echo "FAIL: expected match for $name (mode=$mode)" >&2
    exit 1
  fi
}

assert_no_match() {
  local mode="$1"
  local input_line="$2"
  local expected_hash="$3"
  local name="$4"
  local in_file="$TMP_DIR/${name}.txt"
  local bloom_file="$TMP_DIR/${name}.blf"
  local out_file="$TMP_DIR/${name}.out"

  printf '%s\n' "$input_line" > "$in_file"
  ./hex2blf -t "$mode" "$in_file" "$bloom_file" >/dev/null 2>&1
  printf '%s\n' "$expected_hash" | ./blfchk "$bloom_file" > "$out_file"

  if [ -s "$out_file" ]; then
    echo "FAIL: unexpected match for $name (mode=$mode)" >&2
    exit 1
  fi
}

HASH_GENESIS="62e907b15cbf27d5425399ebf6f0fb50ebb88f18"
ADDR_GENESIS="1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"
PUBKEY_COMP="0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
HASH_COMP="751e76e8199196d454941c45d1b3a323f1433bd6"
PUBKEY_UNCOMP="0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8"
HASH_UNCOMP="91b24bf9f5288532960ac687abb035127b1d28a5"

assert_match "h" "$HASH_GENESIS" "$HASH_GENESIS" "hash160_hex"
assert_match "a" "$ADDR_GENESIS" "$HASH_GENESIS" "bitcoin_address"
assert_match "c" "$PUBKEY_COMP" "$HASH_COMP" "compressed_pubkey"
assert_match "u" "$PUBKEY_UNCOMP" "$HASH_UNCOMP" "uncompressed_pubkey"
assert_no_match "h" "$ADDR_GENESIS" "$HASH_GENESIS" "address_in_hash_mode"

echo "OK: hex2blf input type tests passed"
