#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ -z "${DEVKITPRO:-}" ]]; then
  if [[ -f /etc/profile.d/devkit-env.sh ]]; then
    source /etc/profile.d/devkit-env.sh
  fi
fi

if [[ -z "${DEVKITPRO:-}" || ! -f "${DEVKITPRO}/libnx/switch_rules" ]]; then
  echo "devkitPro/libnx not found. Use ./scripts/build-docker.sh or install switch-dev."
  exit 1
fi

make clean
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
echo "Built: $ROOT/advance.nro"
