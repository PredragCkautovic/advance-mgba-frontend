#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${DEVKITPRO_IMAGE:-devkitpro/devkita64:latest}"
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

command -v docker >/dev/null 2>&1 || { echo "Docker is required for this build method."; exit 1; }

# Build as root inside the official devkitPro image (the most compatible mode),
# then hand generated artifacts back to the host user before the container exits.
# This prevents the root-owned build/ directories older Advance releases left behind.
docker run --rm \
  -e HOST_UID="$HOST_UID" \
  -e HOST_GID="$HOST_GID" \
  -v "$ROOT:/project" \
  -w /project \
  "$IMAGE" \
  bash -lc 'set -e; make clean; make -j"$(nproc)"; chown -R "$HOST_UID:$HOST_GID" /project/build /project/advance.nro /project/advance.nacp /project/advance.elf 2>/dev/null || true'

echo "Built: $ROOT/advance.nro"
echo "Artifacts owned by: $(id -un):$(id -gn)"
