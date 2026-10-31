#!/usr/bin/env bash
# Assembles the same input twice and fails when the outputs differ.
set -euo pipefail
BINARY="${1:?usage: check-determinism.sh <RV32IToREBEL>}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
for run in 1 2; do
    "$BINARY" "$ROOT/RiscvTests/PreprocessedAssembly/add.s" -o "$WORK/out$run" >/dev/null
done
cmp "$WORK/out1.tas" "$WORK/out2.tas"
echo "determinism: identical output"
