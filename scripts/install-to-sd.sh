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

mkdir -p "$SD/switch/advance/covers" "$SD/switch/advance/assets/icons"
cp "$ROOT/advance.nro" "$SD/switch/advance/advance.nro"
cp -f "$ROOT/assets/store-icon.png" "$SD/switch/advance/assets/store-icon.png"
cp -f "$ROOT/assets/advance-logo.png" "$SD/switch/advance/assets/advance-logo.png"
cp -f "$ROOT/assets/sidebar-brand.png" "$SD/switch/advance/assets/sidebar-brand.png"
cp -f "$ROOT/assets/sidebar-mark.png" "$SD/switch/advance/assets/sidebar-mark.png"
cp -f "$ROOT/assets/icons/"*.png "$SD/switch/advance/assets/icons/"
cp -f "$ROOT/assets/icons/LICENSE-LUCIDE.txt" "$SD/switch/advance/assets/icons/LICENSE-LUCIDE.txt"
[[ -f "$SD/switch/advance/config.ini" ]] || cp "$ROOT/sd-template/switch/advance/config.ini" "$SD/switch/advance/config.ini"
[[ -f "$SD/switch/advance/advance.json.example" ]] || cp "$ROOT/examples/advance.json" "$SD/switch/advance/advance.json.example"
sync

echo "Advance 0.4 installed to: $SD/switch/advance/advance.nro"
echo "Expected mGBA:          $SD/switch/mgba.nro"
echo "Expected ROM root:      $SD/mGBA/Roms"
echo "Existing ROMs, artwork, mGBA and Advance state were not modified."
