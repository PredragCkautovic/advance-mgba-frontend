#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

[[ -f advance.nro ]] || { echo "advance.nro missing. Build first."; exit 1; }
rm -rf dist/sd-root dist/Advance-Switch-v0.1.0.zip
mkdir -p dist/sd-root/switch/advance/covers
cp advance.nro dist/sd-root/switch/advance/advance.nro
cp sd-template/switch/advance/config.ini dist/sd-root/switch/advance/config.ini
cp README-SWITCH.txt dist/sd-root/switch/advance/README.txt
cp examples/advance.json dist/sd-root/switch/advance/advance.json.example
(
  cd dist/sd-root
  zip -qr ../Advance-Switch-v0.1.0.zip .
)
echo "Release: $ROOT/dist/Advance-Switch-v0.1.0.zip"
