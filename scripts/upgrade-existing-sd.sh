#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 /path/to/mounted/switch-sd"
  exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SD="$(realpath "$1")"
[[ -d "$SD" ]] || { echo "Not a directory: $SD"; exit 1; }
[[ -f "$ROOT/advance.nro" ]] || { echo "advance.nro missing; run ./scripts/build-docker.sh first."; exit 1; }

mkdir -p "$SD/switch/advance/covers"
cp "$ROOT/advance.nro" "$SD/switch/advance/advance.nro"

CFG="$SD/switch/advance/config.ini"
if [[ ! -f "$CFG" ]]; then
  cp "$ROOT/sd-template/switch/advance/config.ini" "$CFG"
  echo "Installed Advance 0.1 config.ini"
else
  sed -i \
    -e 's#^rom_dir=sdmc:/roms/gba$#rom_dir=sdmc:/mGBA/Roms#' \
    -e 's#^mgba_nro=sdmc:/switch/mgba/mgba.nro$#mgba_nro=sdmc:/switch/mgba.nro#' \
    "$CFG"
  grep -q '^theme=' "$CFG" || printf '\ntheme=crimson\n' >> "$CFG"
  grep -q '^ui_sounds=' "$CFG" || printf 'ui_sounds=true\n' >> "$CFG"
  grep -q '^ui_volume=' "$CFG" || printf 'ui_volume=72\n' >> "$CFG"
  grep -q '^dynamic_backdrop=' "$CFG" || printf 'dynamic_backdrop=true\n' >> "$CFG"
  grep -q '^backdrop_intensity=' "$CFG" || printf 'backdrop_intensity=58\n' >> "$CFG"
  grep -q '^show_hidden=' "$CFG" || printf 'show_hidden=false\n' >> "$CFG"
  grep -q '^confirm_launch=' "$CFG" || printf 'confirm_launch=false\n' >> "$CFG"
  echo "Preserved config.ini and migrated it to Advance 0.1 defaults"
fi

[[ -f "$SD/switch/advance/advance.json.example" ]] || cp "$ROOT/examples/advance.json" "$SD/switch/advance/advance.json.example"
sync

echo
echo "Advance 0.1 installed: $SD/switch/advance/advance.nro"
echo "Preserved: mGBA, ROMs, artwork, config.ini, titles.tsv, state.tsv and collections.tsv"
echo "ROM root: $SD/mGBA/Roms"
echo "mGBA:     $SD/switch/mgba.nro"
