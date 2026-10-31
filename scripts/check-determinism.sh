#!/usr/bin/env bash
# Assembles the same inputs twice and fails when any output differs.
set -euo pipefail
BINARY="${1:?usage: check-determinism.sh <RV32IToREBEL>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

TESTS="$ROOT/RiscvTests/PreprocessedAssembly"

# Assembly text.
for run in 1 2; do
    "$BINARY" "$TESTS/add.s" -o "$WORK/tas$run" >/dev/null
done
cmp "$WORK/tas1.tas" "$WORK/tas2.tas"

# Binary image.
for run in 1 2; do
    "$BINARY" "$TESTS/add.s" --emit-image --no-tas -o "$WORK/img$run" >/dev/null
done
cmp "$WORK/img1.img" "$WORK/img2.img"

# Linked units.
printf '\t.text\n\t.globl helper\nhelper:\n\tret\n' > "$WORK/helper.s"
for run in 1 2; do
    "$BINARY" --link "$TESTS/add.s" "$WORK/helper.s" -o "$WORK/link$run" >/dev/null
done
cmp "$WORK/link1.tas" "$WORK/link2.tas"

echo "determinism: identical .tas, .img and linked output"
