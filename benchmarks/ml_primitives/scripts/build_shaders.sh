#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SPV_DIR="$ROOT_DIR/build/spv"

mkdir -p "$SPV_DIR"

if command -v glslc >/dev/null 2>&1; then
  COMPILER="glslc"
  MODE="glslc"
elif command -v glslangValidator >/dev/null 2>&1; then
  COMPILER="glslangValidator"
  MODE="glslangValidator"
else
  echo "Error: neither glslc nor glslangValidator was found in PATH."
  echo ""
  echo "Install one of them:"
  echo "  brew install shaderc        # provides glslc"
  echo "  brew install glslang        # provides glslangValidator"
  echo ""
  echo "Or initialize/build your repo's glslang submodule."
  exit 1
fi

echo "[build_shaders] Using compiler: $COMPILER"

compile_shader() {
  local src="$1"
  local out="$2"

  echo "[build_shaders] $src -> $out"

  if [[ "$MODE" == "glslc" ]]; then
    "$COMPILER" -fshader-stage=compute "$src" -o "$out"
  else
    "$COMPILER" -V "$src" -o "$out"
  fi
}

compile_shader "$ROOT_DIR/shaders/ml_primitives/softmax/softmax_three_pass.comp" \
  "$SPV_DIR/softmax_three_pass.spv"

compile_shader "$ROOT_DIR/shaders/ml_primitives/softmax/softmax_fused_lmem.comp" \
  "$SPV_DIR/softmax_fused_lmem.spv"

compile_shader "$ROOT_DIR/shaders/ml_primitives/softmax/softmax_online.comp" \
  "$SPV_DIR/softmax_online.spv"

compile_shader "$ROOT_DIR/shaders/ml_primitives/rmsnorm/rmsnorm_basic.comp" \
  "$SPV_DIR/rmsnorm_basic.spv"

echo "[build_shaders] Done. SPIR-V files are in:"
echo "  $SPV_DIR"
