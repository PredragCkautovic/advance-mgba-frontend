#!/usr/bin/env python3
"""Copy locally-owned cover art into Advance's cover folder by fuzzy filename matching.

This tool never downloads artwork. Point it at a folder of cover images you already have.
"""
from __future__ import annotations
import argparse
import re
import shutil
from pathlib import Path

IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".webp"}
ROM_EXTS = {".gba", ".agb", ".gb", ".gbc"}


def norm(s: str) -> str:
    return re.sub(r"[^a-z0-9]", "", s.lower())


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("rom_dir", type=Path)
    p.add_argument("art_dir", type=Path)
    p.add_argument("output_dir", type=Path)
    args = p.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    art = {}
    for f in args.art_dir.rglob("*"):
        if f.is_file() and f.suffix.lower() in IMAGE_EXTS:
            art.setdefault(norm(f.stem), f)

    copied = 0
    missing = []
    for rom in sorted(args.rom_dir.rglob("*")):
        if not rom.is_file() or rom.suffix.lower() not in ROM_EXTS:
            continue
        key = norm(rom.stem)
        source = art.get(key)
        if not source:
            # Try containment for release-tag differences such as "(USA)".
            candidates = [v for k, v in art.items() if key in k or k in key]
            source = candidates[0] if len(candidates) == 1 else None
        if source:
            dest = args.output_dir / f"{rom.stem}{source.suffix.lower()}"
            shutil.copy2(source, dest)
            copied += 1
        else:
            missing.append(rom.name)

    print(f"Copied {copied} covers")
    if missing:
        print(f"No unique match for {len(missing)} ROM(s):")
        for name in missing[:40]:
            print(f"  - {name}")
        if len(missing) > 40:
            print(f"  ... and {len(missing)-40} more")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
