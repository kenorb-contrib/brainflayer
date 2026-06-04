#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/brainflayer-blfintersect-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"

# A bloom with one known hash
printf '%s\n' "1111111111111111111111111111111111111111" > "$TMP_DIR/a_hashes.txt"
./hex2blf "$TMP_DIR/a_hashes.txt" "$TMP_DIR/a.blf" >/dev/null

# Identical bloom => must report possible intersection
cp "$TMP_DIR/a.blf" "$TMP_DIR/b_same.blf"
./blfintersect "$TMP_DIR/a.blf" "$TMP_DIR/b_same.blf" > "$TMP_DIR/same.txt"
if ! grep -q $'MAY_INTERSECT\tyes' "$TMP_DIR/same.txt"; then
  echo "FAIL [blfintersect-same]: expected MAY_INTERSECT yes" >&2
  cat "$TMP_DIR/same.txt" >&2
  exit 1
fi

# Empty bloom => must report no intersection
truncate -s 1073741824 "$TMP_DIR/empty.blf"
./blfintersect "$TMP_DIR/a.blf" "$TMP_DIR/empty.blf" > "$TMP_DIR/empty.txt"
if ! grep -q $'MAY_INTERSECT\tno' "$TMP_DIR/empty.txt"; then
  echo "FAIL [blfintersect-empty]: expected MAY_INTERSECT no" >&2
  cat "$TMP_DIR/empty.txt" >&2
  exit 1
fi

# Quick mode should also return no on empty pair
./blfintersect -q "$TMP_DIR/a.blf" "$TMP_DIR/empty.blf" > "$TMP_DIR/empty_quick.txt"
if ! grep -q $'MAY_INTERSECT\tno\tmode=quick' "$TMP_DIR/empty_quick.txt"; then
  echo "FAIL [blfintersect-empty-quick]: expected quick no" >&2
  cat "$TMP_DIR/empty_quick.txt" >&2
  exit 1
fi

echo "OK: blfintersect tests passed"
