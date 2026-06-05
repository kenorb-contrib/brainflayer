#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d /tmp/brainflayer-pwblfchk-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"

# Build address bloom that definitely matches password "abc".
printf '%s\n' "abc" > "$TMP_DIR/addr_seed_pass.txt"
./brainflayer -c cu -i "$TMP_DIR/addr_seed_pass.txt" -o "$TMP_DIR/addr_seed.out" >/dev/null 2>&1 || true
awk -F: '/^[0-9a-f]{40}:/ {print $1}' "$TMP_DIR/addr_seed.out" > "$TMP_DIR/addr_hashes.txt"
./hex2blf "$TMP_DIR/addr_hashes.txt" "$TMP_DIR/addr_hit.blf" >/dev/null

# Positive case: one password should produce a possible intersection.
cat > "$TMP_DIR/passwords_hit.txt" <<'EOF'
abc
zzz-not-matching
EOF

./pwblfchk -o "$TMP_DIR/passwords_hit.blf" "$TMP_DIR/passwords_hit.txt" "$TMP_DIR/addr_hit.blf" > "$TMP_DIR/hit_report.txt"

if ! grep -q $'MAY_INTERSECT\tyes' "$TMP_DIR/hit_report.txt"; then
  echo "FAIL [pwblfchk-hit]: expected MAY_INTERSECT yes" >&2
  cat "$TMP_DIR/hit_report.txt" >&2
  exit 1
fi
if ! grep -q "possible_passwords=1" "$TMP_DIR/hit_report.txt"; then
  echo "FAIL [pwblfchk-hit]: expected possible_passwords=1" >&2
  cat "$TMP_DIR/hit_report.txt" >&2
  exit 1
fi
if [ "$(stat -c %s "$TMP_DIR/passwords_hit.blf")" -ne 1073741824 ]; then
  echo "FAIL [pwblfchk-bloom-size]: expected password bloom size 1073741824" >&2
  exit 1
fi

# Negative case: no passwords from this set should intersect.
printf '%s\n' "definitely-no-hit-1" "definitely-no-hit-2" > "$TMP_DIR/passwords_miss.txt"
./pwblfchk "$TMP_DIR/passwords_miss.txt" "$TMP_DIR/addr_hit.blf" > "$TMP_DIR/miss_report.txt"

if ! grep -q $'MAY_INTERSECT\tno' "$TMP_DIR/miss_report.txt"; then
  echo "FAIL [pwblfchk-miss]: expected MAY_INTERSECT no" >&2
  cat "$TMP_DIR/miss_report.txt" >&2
  exit 1
fi
if ! grep -q "possible_passwords=0" "$TMP_DIR/miss_report.txt"; then
  echo "FAIL [pwblfchk-miss]: expected possible_passwords=0" >&2
  cat "$TMP_DIR/miss_report.txt" >&2
  exit 1
fi

echo "OK: pwblfchk intersection tests passed"
