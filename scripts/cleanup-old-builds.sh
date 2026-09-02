#!/usr/bin/env bash
set -euo pipefail

BASE="${1:-$HOME/Downloads}"
CURRENT_DIR="advance-mgba-frontend-v0.1"
CURRENT_ZIP="Advance-mGBA-Frontend-v0.1-source.zip"

[[ -d "$BASE" ]] || { echo "Not a directory: $BASE"; exit 1; }

echo "Advance cleanup root: $BASE"
echo "Keeping: $CURRENT_DIR and $CURRENT_ZIP"
echo

shopt -s nullglob
candidates=(
  "$BASE/advance-mgba-frontend"
  "$BASE/advance-mgba-frontend-v1."*
  "$BASE/advance-mgba-frontend-v2."*
  "$BASE/advance-mgba-frontend-v3."*
  "$BASE/advance-mgba-frontend-v4.0"
  "$BASE/Advance-mGBA-Frontend-v1."*-source.zip
  "$BASE/Advance-mGBA-Frontend-v2."*-source.zip
  "$BASE/Advance-mGBA-Frontend-v3."*-source.zip
  "$BASE/Advance-mGBA-Frontend-v4.0-source.zip"
)

if (( ${#candidates[@]} == 0 )); then
  echo "No older Advance builds found."
  exit 0
fi

printf 'Will remove:\n'
printf '  %s\n' "${candidates[@]}"
echo
read -r -p "Type DELETE to continue: " answer
[[ "$answer" == "DELETE" ]] || { echo "Cancelled."; exit 0; }

for path in "${candidates[@]}"; do
  [[ -e "$path" ]] || continue
  if rm -rf -- "$path" 2>/dev/null; then
    echo "Removed: $path"
  else
    echo "Needs elevated permission: $path"
    sudo rm -rf -- "$path"
    echo "Removed with sudo: $path"
  fi
done

echo "Cleanup complete."
