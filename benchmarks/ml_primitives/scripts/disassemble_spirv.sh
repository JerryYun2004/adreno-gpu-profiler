#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/results/disasm"
mkdir -p "$OUT"
SPIRV_DIS="${SPIRV_DIS:-spirv-dis}"
for f in "$ROOT"/build/spv/*.spv; do
  base="$(basename "$f" .spv)"
  "$SPIRV_DIS" "$f" > "$OUT/${base}.spirv_disasm.txt"
done
echo "Wrote SPIR-V disassembly to $OUT"
