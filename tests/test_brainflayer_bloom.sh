#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

TMP_DIR="$(mktemp -d /tmp/brainflayer-bloom-test-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

# Known values for password "abc":
#   SHA256("abc") -> private key
#   compressed hash160  : fa19739677ed143ba2dcabf535aebc043cd40cdc
#   compressed address  : 1PoQRMsXyQFSqCCRek7tt7umfRkJG9TY8x
#   uncompressed hash160: e8fdc3b4b312ee8725fab4937901752704ede7f4
#   uncompressed address: 1NEwmNSC7w9nZeASngHCd43Bc5eC2FmXpn
PASSWORD="abc"
HASH_COMP="fa19739677ed143ba2dcabf535aebc043cd40cdc"
ADDR_COMP="1PoQRMsXyQFSqCCRek7tt7umfRkJG9TY8x"
HASH_UNCOMP="e8fdc3b4b312ee8725fab4937901752704ede7f4"
ADDR_UNCOMP="1NEwmNSC7w9nZeASngHCd43Bc5eC2FmXpn"
PRIV1_COMP_PUB="0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
PRIV1_COMP_X="79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798"
PRIV1_UNCOMP_PUB="0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8"
PRIV1_HEX_PAD="0000000000000000000000000000000000000000000000000000000000000001"
WIF_PRIV1_UNCOMP="5HpHagT65TZzG1PH3CSu63k8DbpvD8s5ip4nEB3kEsreAnchuDf"
WIF_PRIV1_COMP="KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn"

assert_found() {
  local name="$1"
  local bloom="$2"
  local pass="$3"
  local expected_hash="$4"

  local out
  out="$(printf '%s\n' "$pass" | ./brainflayer -b "$bloom" 2>/dev/null || true)"
  if ! echo "$out" | grep -q "^$expected_hash:"; then
    echo "FAIL [$name]: brainflayer did not find expected hash160 '$expected_hash'" >&2
    echo "  Output: $out" >&2
    exit 1
  fi
}

assert_not_found() {
  local name="$1"
  local bloom="$2"
  local pass="$3"

  local out
  out="$(printf '%s\n' "$pass" | ./brainflayer -b "$bloom" 2>/dev/null || true)"
  if [ -n "$out" ]; then
    echo "FAIL [$name]: brainflayer unexpectedly found a match" >&2
    echo "  Output: $out" >&2
    exit 1
  fi
}

assert_found_with_type() {
  local name="$1"
  local bloom="$2"
  local pass="$3"
  local expected_hash="$4"
  local expected_type="$5"
  local expected_comp="$6"
  local expected_line

  local out
  out="$(printf '%s\n' "$pass" | ./brainflayer -c "$expected_comp" -b "$bloom" 2>/dev/null || true)"
  expected_line="$expected_hash:$expected_comp:$expected_type:$pass"
  if ! echo "$out" | grep -Fxq -- "$expected_line"; then
    echo "FAIL [$name]: expected '$expected_hash:$expected_comp:$expected_type:$pass'" >&2
    echo "  Output: $out" >&2
    exit 1
  fi
}

assert_found_once_with_stats() {
  local name="$1"
  local bloom="$2"
  local pass="$3"
  local expected_line="$4"
  local out_file="$TMP_DIR/${name}.out"
  local err_file="$TMP_DIR/${name}.err"

  printf '%s\n%s\n' "$pass" "$pass" | ./brainflayer -v -c c -b "$bloom" >"$out_file" 2>"$err_file"

  if [ "$(grep -Fxc -- "$expected_line" "$out_file")" -ne 1 ]; then
    echo "FAIL [$name]: expected exactly one saved match '$expected_line'" >&2
    echo "  Output: $(cat "$out_file")" >&2
    exit 1
  fi
  if [ "$(grep -Ec '^[0-9a-f]{40}:' "$out_file")" -ne 1 ]; then
    echo "FAIL [$name]: duplicate match was written to output" >&2
    echo "  Output: $(cat "$out_file")" >&2
    exit 1
  fi
  if ! grep -Eq "found:[[:space:]]*1/2" "$err_file"; then
    echo "FAIL [$name]: verbose stats did not keep found counter at 1/2" >&2
    echo "  Stderr: $(cat "$err_file")" >&2
    exit 1
  fi
}

# ── Test 1: bloom built from hash160 (hex2blf -t h), compressed ────────────
printf '%s\n' "$HASH_COMP" > "$TMP_DIR/t1_hashes.txt"
./hex2blf -t h "$TMP_DIR/t1_hashes.txt" "$TMP_DIR/t1.blf" >/dev/null 2>&1
assert_found "hash160/compressed" "$TMP_DIR/t1.blf" "$PASSWORD" "$HASH_COMP"

# ── Test 2: bloom built from Bitcoin address (hex2blf -t a), compressed ────
printf '%s\n' "$ADDR_COMP" > "$TMP_DIR/t2_addrs.txt"
./hex2blf -t a "$TMP_DIR/t2_addrs.txt" "$TMP_DIR/t2.blf" >/dev/null 2>&1
assert_found "address/compressed" "$TMP_DIR/t2.blf" "$PASSWORD" "$HASH_COMP"

# ── Test 3: bloom built from hash160 (hex2blf -t h), uncompressed ──────────
printf '%s\n' "$HASH_UNCOMP" > "$TMP_DIR/t3_hashes.txt"
./hex2blf -t h "$TMP_DIR/t3_hashes.txt" "$TMP_DIR/t3.blf" >/dev/null 2>&1
assert_found "hash160/uncompressed" "$TMP_DIR/t3.blf" "$PASSWORD" "$HASH_UNCOMP"

# ── Test 4: bloom built from Bitcoin address (hex2blf -t a), uncompressed ──
printf '%s\n' "$ADDR_UNCOMP" > "$TMP_DIR/t4_addrs.txt"
./hex2blf -t a "$TMP_DIR/t4_addrs.txt" "$TMP_DIR/t4.blf" >/dev/null 2>&1
assert_found "address/uncompressed" "$TMP_DIR/t4.blf" "$PASSWORD" "$HASH_UNCOMP"

# ── Test 5: wrong password must NOT match ───────────────────────────────────
assert_not_found "no-false-match" "$TMP_DIR/t1.blf" "wrongpassword"

# ── Test 6: mode label must be "passphrase" when first path matches ─────────
assert_found_with_type "label-passphrase" "$TMP_DIR/t1.blf" "$PASSWORD" "$HASH_COMP" "passphrase" "c"

# ── Test 7: mode label must be "exponent" when second path matches ──────────
# SHA256("abc"), interpreted as raw 32-byte private key (secret exponent)
SECRET_EXP_ABC="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
assert_found_with_type "label-exponent" "$TMP_DIR/t1.blf" "$SECRET_EXP_ABC" "$HASH_COMP" "exponent" "c"

# ── Test 8: exponent must support non-64 hex length input ("1") ─────────────
HASH_PRIV1_COMP="751e76e8199196d454941c45d1b3a323f1433bd6"
printf '%s\n' "$HASH_PRIV1_COMP" > "$TMP_DIR/t8_hashes.txt"
./hex2blf -t h "$TMP_DIR/t8_hashes.txt" "$TMP_DIR/t8.blf" >/dev/null 2>&1
assert_found_with_type "label-exponent-short-hex" "$TMP_DIR/t8.blf" "1" "$HASH_PRIV1_COMP" "exponent" "c"

# ── Tests 9-10: passphrase finds BOTH compressed AND uncompressed in one run ─
# Build bloom with both hash160s from "abc" as passphrase
printf '%s\n%s\n' "$HASH_COMP" "$HASH_UNCOMP" > "$TMP_DIR/t9_both.txt"
./hex2blf -t h "$TMP_DIR/t9_both.txt" "$TMP_DIR/t9_both.blf" >/dev/null 2>&1

t9_out="$(printf '%s\n' "$PASSWORD" | ./brainflayer -b "$TMP_DIR/t9_both.blf" 2>/dev/null || true)"

if ! echo "$t9_out" | grep -Fq "$HASH_COMP:c:passphrase:$PASSWORD"; then
  echo "FAIL [passphrase-both/compressed]: compressed not found" >&2
  echo "  Output: $t9_out" >&2
  exit 1
fi
if ! echo "$t9_out" | grep -Fq "$HASH_UNCOMP:u:passphrase:$PASSWORD"; then
  echo "FAIL [passphrase-both/uncompressed]: uncompressed not found" >&2
  echo "  Output: $t9_out" >&2
  exit 1
fi
echo "  PASS [passphrase-both]: compressed and uncompressed both found as passphrase"

# ── Tests 11-12: exponent finds BOTH compressed AND uncompressed in one run ──
# Private key = 1 (short hex), compressed hash160 and uncompressed hash160
HASH_PRIV1_UNCOMP="91b24bf9f5288532960ac687abb035127b1d28a5"
printf '%s\n%s\n' "$HASH_PRIV1_COMP" "$HASH_PRIV1_UNCOMP" > "$TMP_DIR/t11_both.txt"
./hex2blf -t h "$TMP_DIR/t11_both.txt" "$TMP_DIR/t11_both.blf" >/dev/null 2>&1

t11_out="$(printf '%s\n' "1" | ./brainflayer -b "$TMP_DIR/t11_both.blf" 2>/dev/null || true)"

if ! echo "$t11_out" | grep -Fq "$HASH_PRIV1_COMP:c:exponent:1"; then
  echo "FAIL [exponent-both/compressed]: compressed not found" >&2
  echo "  Output: $t11_out" >&2
  exit 1
fi
if ! echo "$t11_out" | grep -Fq "$HASH_PRIV1_UNCOMP:u:exponent:1"; then
  echo "FAIL [exponent-both/uncompressed]: uncompressed not found" >&2
  echo "  Output: $t11_out" >&2
  exit 1
fi
echo "  PASS [exponent-both]: compressed and uncompressed both found as exponent"

# ── Test 13: repeated found input must not be written or counted twice ────────
assert_found_once_with_stats \
  "dedupe-repeated-found-input" \
  "$TMP_DIR/t1.blf" \
  "$PASSWORD" \
  "$HASH_COMP:c:passphrase:$PASSWORD"

# ── Test 14: cross-run deduplication – second run appends, no duplicates ──────
FOUND_FILE="$TMP_DIR/t14_found.txt"
rm -f "$FOUND_FILE"

# first run: writes one match
printf '%s\n' "$PASSWORD" | ./brainflayer -c c -b "$TMP_DIR/t1.blf" -o "$FOUND_FILE" 2>/dev/null || true

first_count="$(grep -Ec '^[0-9a-f]{40}:' "$FOUND_FILE" 2>/dev/null || echo 0)"
if [ "$first_count" -ne 1 ]; then
  echo "FAIL [cross-run/first-run]: expected 1 match, got $first_count" >&2
  echo "  File: $(cat "$FOUND_FILE")" >&2
  exit 1
fi

# second run with same password: should NOT add another line
printf '%s\n' "$PASSWORD" | ./brainflayer -c c -b "$TMP_DIR/t1.blf" -o "$FOUND_FILE" 2>/dev/null || true

second_count="$(grep -Ec '^[0-9a-f]{40}:' "$FOUND_FILE" 2>/dev/null || echo 0)"
if [ "$second_count" -ne 1 ]; then
  echo "FAIL [cross-run/second-run]: expected still 1 match after second run, got $second_count" >&2
  echo "  File: $(cat "$FOUND_FILE")" >&2
  exit 1
fi
echo "  PASS [cross-run]: found.txt not duplicated across runs"

# ── Test 15: found hash160 must also be checked as passphrase ─────────────────
CHAIN_INPUT="$HASH_COMP"
CHAIN_HASH_COMP="$(printf '%s\n' "$CHAIN_INPUT" | ./brainflayer -c c 2>/dev/null | head -n1 | cut -d: -f1)"

printf '%s\n%s\n' "$HASH_COMP" "$CHAIN_HASH_COMP" > "$TMP_DIR/t15_hashes.txt"
./hex2blf -t h "$TMP_DIR/t15_hashes.txt" "$TMP_DIR/t15.blf" >/dev/null 2>&1

t15_out="$(printf '%s\n' "$PASSWORD" | ./brainflayer -c c -b "$TMP_DIR/t15.blf" 2>/dev/null || true)"

if ! echo "$t15_out" | grep -Fq "$HASH_COMP:c:passphrase:$PASSWORD"; then
  echo "FAIL [chain-passphrase/original]: original match not found" >&2
  echo "  Output: $t15_out" >&2
  exit 1
fi
if ! echo "$t15_out" | grep -Fq "$CHAIN_HASH_COMP:c:passphrase:$CHAIN_INPUT"; then
  echo "FAIL [chain-passphrase/chained]: chained passphrase match not found" >&2
  echo "  Output: $t15_out" >&2
  exit 1
fi
echo "  PASS [chain-passphrase]: found hash160 checked as passphrase"

# ── Test 16: found hash160 must also be checked as exponent ────────────────────
CHAIN_INPUT_PADDED="$(printf '%024d%s' 0 "$CHAIN_INPUT")"
CHAIN_EXP_HASH_COMP="$(printf '%s\n' "$CHAIN_INPUT_PADDED" | ./brainflayer -x -t priv -c c 2>/dev/null | head -n1 | cut -d: -f1)"

printf '%s\n%s\n' "$HASH_COMP" "$CHAIN_EXP_HASH_COMP" > "$TMP_DIR/t16_hashes.txt"
./hex2blf -t h "$TMP_DIR/t16_hashes.txt" "$TMP_DIR/t16.blf" >/dev/null 2>&1

t16_out="$(printf '%s\n' "$PASSWORD" | ./brainflayer -c c -b "$TMP_DIR/t16.blf" 2>/dev/null || true)"

if ! echo "$t16_out" | grep -Fq "$HASH_COMP:c:passphrase:$PASSWORD"; then
  echo "FAIL [chain-exponent/original]: original match not found" >&2
  echo "  Output: $t16_out" >&2
  exit 1
fi
if ! echo "$t16_out" | grep -Fq "$CHAIN_EXP_HASH_COMP:c:exponent:$CHAIN_INPUT"; then
  echo "FAIL [chain-exponent/chained]: chained exponent match not found" >&2
  echo "  Output: $t16_out" >&2
  exit 1
fi
echo "  PASS [chain-exponent]: found hash160 checked as exponent"

# ── Test 17: derived pubkey/privkey inputs are also checked and logged ─────────
HASH_CPUB_PASS="$(printf '%s\n' "$PRIV1_COMP_PUB" | ./brainflayer -c c 2>/dev/null | head -n1 | cut -d: -f1)"
HASH_CPUB_EXP="$(printf '%s\n' "$PRIV1_COMP_X" | ./brainflayer -x -t priv -c c 2>/dev/null | head -n1 | cut -d: -f1)"
HASH_UPUB_PASS="$(printf '%s\n' "$PRIV1_UNCOMP_PUB" | ./brainflayer -c c 2>/dev/null | head -n1 | cut -d: -f1)"
HASH_PRIV_PASS="$(printf '%s\n' "$PRIV1_HEX_PAD" | ./brainflayer -c c 2>/dev/null | head -n1 | cut -d: -f1)"

printf '%s\n%s\n%s\n%s\n%s\n' \
  "$HASH_PRIV1_COMP" \
  "$HASH_CPUB_PASS" \
  "$HASH_CPUB_EXP" \
  "$HASH_UPUB_PASS" \
  "$HASH_PRIV_PASS" > "$TMP_DIR/t17_hashes.txt"
./hex2blf -t h "$TMP_DIR/t17_hashes.txt" "$TMP_DIR/t17.blf" >/dev/null 2>&1

t17_found="$TMP_DIR/t17_found.txt"
rm -f "$t17_found"
printf '%s\n' "1" | ./brainflayer -c c -b "$TMP_DIR/t17.blf" -o "$t17_found" 2>/dev/null || true

if ! grep -Fqx "$HASH_CPUB_PASS:c:cpub-passphrase:$PRIV1_COMP_PUB" "$t17_found"; then
  echo "FAIL [derived/cpub-passphrase]: compressed pubkey as passphrase not found" >&2
  echo "  Output: $(cat "$t17_found")" >&2
  exit 1
fi
if ! grep -Fqx "$HASH_CPUB_EXP:c:cpub-exponent:$PRIV1_COMP_X" "$t17_found"; then
  echo "FAIL [derived/cpub-exponent]: compressed pubkey as exponent not found" >&2
  echo "  Output: $(cat "$t17_found")" >&2
  exit 1
fi
if ! grep -Fqx "$HASH_UPUB_PASS:c:upub-passphrase:$PRIV1_UNCOMP_PUB" "$t17_found"; then
  echo "FAIL [derived/upub-passphrase]: uncompressed pubkey as passphrase not found" >&2
  echo "  Output: $(cat "$t17_found")" >&2
  exit 1
fi
if ! grep -Fqx "$HASH_PRIV_PASS:c:privkey-passphrase:$PRIV1_HEX_PAD" "$t17_found"; then
  echo "FAIL [derived/privkey-passphrase]: private key as passphrase not found" >&2
  echo "  Output: $(cat "$t17_found")" >&2
  exit 1
fi
echo "  PASS [derived-inputs]: compressed/uncompressed pubkey and private key paths logged to file"

# ── Test 18: WIF input (compressed) in -t wif mode ─────────────────────────────
t18_out="$(printf '%s\n' "$WIF_PRIV1_COMP" | ./brainflayer -t wif -c c -b "$TMP_DIR/t8.blf" 2>/dev/null || true)"
if ! echo "$t18_out" | grep -Fqx "$HASH_PRIV1_COMP:c:wif:$WIF_PRIV1_COMP"; then
  echo "FAIL [wif/compressed]: compressed WIF did not match expected hash/type" >&2
  echo "  Output: $t18_out" >&2
  exit 1
fi
echo "  PASS [wif/compressed]: compressed WIF accepted"

# ── Test 19: WIF input (uncompressed) in -t wif mode ───────────────────────────
t19_out="$(printf '%s\n' "$WIF_PRIV1_UNCOMP" | ./brainflayer -t wif -c u -b "$TMP_DIR/t11_both.blf" 2>/dev/null || true)"
if ! echo "$t19_out" | grep -Fqx "$HASH_PRIV1_UNCOMP:u:wif:$WIF_PRIV1_UNCOMP"; then
  echo "FAIL [wif/uncompressed]: uncompressed WIF did not match expected hash/type" >&2
  echo "  Output: $t19_out" >&2
  exit 1
fi
echo "  PASS [wif/uncompressed]: uncompressed WIF accepted"

# ── Test 20: WIF mode must reject -x hex mode ───────────────────────────────────
if printf '%s\n' "$WIF_PRIV1_COMP" | ./brainflayer -x -t wif -c c -b "$TMP_DIR/t8.blf" >/dev/null 2>&1; then
  echo "FAIL [wif/reject-hex]: -t wif must fail when used with -x" >&2
  exit 1
fi
echo "  PASS [wif/reject-hex]: -x correctly rejected for WIF input"

echo "OK: brainflayer bloom filter tests passed"
