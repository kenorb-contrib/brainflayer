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
echo "  [passphrase both] output: $t9_out"

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
echo "  [exponent both] output: $t11_out"

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

echo "OK: brainflayer bloom filter tests passed"
