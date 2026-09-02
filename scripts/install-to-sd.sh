#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/mounted/switch-sd"
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SD="$(realpath "$1")"
[[ -d "$SD" ]] || { echo "Not a directory: $SD"; exit 1; }
[[ -f "$ROOT/advance.nro" ]] || { echo "advance.nro missing; build it first."; exit 1; }

mkdir -p "$SD/switch/advance/covers"
cp "$ROOT/advance.nro" "$SD/switch/advance/advance.nro"
[[ -f "$SD/switch/advance/config.ini" ]] || cp "$ROOT/sd-template/switch/advance/config.ini" "$SD/switch/advance/config.ini"
[[ -f "$SD/switch/advance/advance.json.example" ]] || cp "$ROOT/examples/advance.json" "$SD/switch/advance/advance.json.example"
sync

echo "Advance 0.1 installed to: $SD/switch/advance/advance.nro"
echo "Expected mGBA:          $SD/switch/mgba.nro"
echo "Expected ROM root:      $SD/mGBA/Roms"
echo "Existing ROMs, artwork, mGBA and Advance state were not modified."
