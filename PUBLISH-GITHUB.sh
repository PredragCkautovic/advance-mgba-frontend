#!/usr/bin/env bash
set -euo pipefail

REPO_NAME="${1:-advance-mgba-frontend}"
OWNER="${GITHUB_OWNER:-PredragCkautovic}"
TAG="v0.4"

command -v git >/dev/null || { echo "git is required"; exit 1; }
command -v gh >/dev/null || { echo "GitHub CLI (gh) is required"; exit 1; }

gh auth status >/dev/null

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git init -b main
fi

git add .
if ! git diff --cached --quiet; then
  git commit -m "release: Advance v0.4"
fi

if ! gh repo view "$OWNER/$REPO_NAME" >/dev/null 2>&1; then
  gh repo create "$OWNER/$REPO_NAME" \
    --public \
    --source=. \
    --remote=origin \
    --description="Premium Nintendo Switch frontend for an existing mGBA Game Boy Advance library"
else
  git remote get-url origin >/dev/null 2>&1 || git remote add origin "https://github.com/$OWNER/$REPO_NAME.git"
fi

git branch -M main
git push -u origin main

if git rev-parse "$TAG" >/dev/null 2>&1; then
  echo "$TAG already exists locally"
else
  git tag -a "$TAG" -m "Advance v0.4"
fi

git push origin "$TAG"

gh release view "$TAG" >/dev/null 2>&1 || gh release create "$TAG" \
  --title "Advance v0.4" \
  --notes-file STORE-LISTING.md

echo "Published: https://github.com/$OWNER/$REPO_NAME"
