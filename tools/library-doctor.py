#!/usr/bin/env python3
"""Advance 0.4 library doctor.

Read-only by default. Audits a per-game ROM collection for:
- missing / ambiguous artwork
- cryptic folder and ROM names
- missing Advance metadata
- duplicate ROM payloads
- metadata paths that do not exist

With --write-missing-metadata it creates advance.json only in folders that do
not already contain one. It never renames ROMs or overwrites existing metadata.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import defaultdict
from dataclasses import dataclass, asdict
from pathlib import Path

ROM_EXTS = {".gba", ".agb", ".gb", ".gbc"}
IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".webp"}
GENERIC = {
    "cover", "boxart", "box art", "front", "front cover", "art", "artwork", "image",
    "poster", "icon", "thumb", "thumbnail", "game", "rom", "banner", "hero",
    "background", "backdrop", "screenshot", "screen",
}


def norm(s: str) -> str:
    return re.sub(r"[^a-z0-9]", "", s.lower())


def pretty(s: str) -> str:
    s = Path(s).stem.replace("_", " ").replace(".", " ")
    s = re.sub(r"(?<=[a-z])(?=[A-Z])", " ", s)
    s = re.sub(r"(?<=[A-Z])(?=[A-Z][a-z])", " ", s)
    s = re.sub(r"(?<=[A-Za-z])(?=[0-9])", " ", s)
    return " ".join(s.split()).strip()


def is_archive_id(s: str) -> bool:
    s = Path(s).stem.strip()
    if not s or s.lower() in GENERIC:
        return True
    if re.fullmatch(r"[A-Za-z]{0,3}[\s_-]*\d{3,}[\s_-]*[A-Za-z]{0,2}", s):
        return True
    # Product-code / archive labels such as A-BPEE e or B-AXVE are identifiers, not titles.
    if re.fullmatch(r"[A-Za-z]{1,2}[\s_-]+[A-Za-z0-9]{4}[\s_-]*[A-Za-z]{0,2}", s):
        return True
    compact = re.sub(r"[^A-Za-z0-9]", "", s)
    if 4 <= len(compact) <= 6 and sum(ch.isupper() or ch.isdigit() for ch in compact) >= len(compact) - 1:
        return True
    letters = sum(ch.isalpha() for ch in s)
    digits = sum(ch.isdigit() for ch in s)
    return digits >= 3 and letters <= 2


def header_title(rom: Path) -> str:
    try:
        with rom.open("rb") as f:
            if rom.suffix.lower() in {".gba", ".agb"}:
                f.seek(0xA0)
                raw = f.read(12)
            else:
                f.seek(0x134)
                raw = f.read(16)
    except OSError:
        return ""
    out: list[str] = []
    for b in raw:
        if b in (0, 0xFF):
            break
        if not 0x20 <= b <= 0x7E:
            return ""
        out.append(chr(b))
    return " ".join("".join(out).split())


def load_metadata(folder: Path) -> dict:
    path = folder / "advance.json"
    if not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else {}
    except (OSError, json.JSONDecodeError):
        return {"__invalid__": True}


def sidecar_title(folder: Path) -> str:
    for name in ("title.txt", "name.txt", "display_name.txt", "display-name.txt"):
        path = folder / name
        if path.is_file():
            try:
                line = path.read_text(errors="replace").splitlines()[0].strip()
            except (OSError, IndexError):
                line = ""
            if line:
                return line
    return ""


def images(folder: Path) -> list[Path]:
    try:
        return [p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTS]
    except OSError:
        return []


def choose_cover(folder: Path, rom: Path, metadata: dict) -> Path | None:
    explicit = metadata.get("cover")
    if isinstance(explicit, str) and explicit:
        p = folder / explicit
        if p.is_file():
            return p
    for ext in IMAGE_EXTS:
        p = folder / f"{rom.stem}{ext}"
        if p.is_file():
            return p
    imgs = images(folder)
    priority = {"cover": 0, "frontcover": 0, "boxart": 0, "front": 1, "poster": 1, "artwork": 1}
    imgs.sort(key=lambda p: (priority.get(norm(p.stem), 2 if not is_archive_id(p.stem) else 4), -p.stat().st_size))
    return imgs[0] if imgs else None


def resolve_title(folder: Path, rom: Path, metadata: dict, cover: Path | None) -> tuple[str, str]:
    value = metadata.get("title")
    if isinstance(value, str) and value.strip():
        return value.strip(), "advance.json"
    value = sidecar_title(folder)
    if value:
        return value, "sidecar"
    if folder.name.lower() != "roms" and not is_archive_id(folder.name):
        return pretty(folder.name), "folder"
    if cover and not is_archive_id(cover.stem):
        return pretty(cover.stem), "cover"
    internal = header_title(rom)
    if internal and not is_archive_id(internal):
        return pretty(internal), "ROM header"
    return pretty(rom.stem), "filename fallback"


def quick_hash(path: Path) -> str:
    h = hashlib.sha256()
    try:
        size = path.stat().st_size
        h.update(str(size).encode())
        with path.open("rb") as f:
            h.update(f.read(1024 * 1024))
            if size > 2 * 1024 * 1024:
                f.seek(max(0, size - 1024 * 1024))
                h.update(f.read(1024 * 1024))
    except OSError:
        return ""
    return h.hexdigest()


@dataclass
class ReportRow:
    rom: str
    title: str
    title_source: str
    cover: str
    metadata: str
    issues: list[str]
    duplicate_group: str = ""


def suggested_metadata(title: str, cover: Path | None) -> dict:
    data: dict[str, object] = {
        "title": title,
        "short_title": title,
        "author": "",
        "version": "",
        "base_game": "",
        "genre": [],
        "tags": [],
        "description": "",
        "release_year": 0,
    }
    if cover:
        data["cover"] = cover.name
    return data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("rom_root", type=Path)
    ap.add_argument("--json-report", type=Path)
    ap.add_argument("--write-missing-metadata", action="store_true")
    args = ap.parse_args()

    roms = sorted(p for p in args.rom_root.rglob("*") if p.is_file() and p.suffix.lower() in ROM_EXTS)
    hashes: dict[str, list[Path]] = defaultdict(list)
    rows: list[ReportRow] = []

    for rom in roms:
        folder = rom.parent
        metadata = load_metadata(folder)
        cover = choose_cover(folder, rom, metadata)
        title, source = resolve_title(folder, rom, metadata, cover)
        issues: list[str] = []
        if not cover:
            issues.append("missing-cover")
        if not (folder / "advance.json").is_file():
            issues.append("missing-metadata")
        elif metadata.get("__invalid__"):
            issues.append("invalid-metadata-json")
        if source == "filename fallback":
            issues.append("weak-title")
        if is_archive_id(folder.name):
            issues.append("archive-id-folder")
        for key in ("cover", "banner"):
            value = metadata.get(key)
            if isinstance(value, str) and value and not (folder / value).is_file():
                issues.append(f"missing-{key}-file")
        qh = quick_hash(rom)
        if qh:
            hashes[qh].append(rom)
        rows.append(ReportRow(str(rom), title, source, str(cover or ""), str(folder / "advance.json") if (folder / "advance.json").is_file() else "", issues))

        if args.write_missing_metadata and not (folder / "advance.json").exists():
            try:
                (folder / "advance.json").write_text(json.dumps(suggested_metadata(title, cover), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
            except OSError as exc:
                print(f"WARN: could not write {folder / 'advance.json'}: {exc}")

    duplicate_id = 0
    by_rom = {r.rom: r for r in rows}
    for group in hashes.values():
        if len(group) < 2:
            continue
        duplicate_id += 1
        label = f"DUP-{duplicate_id:03d}"
        for rom in group:
            by_rom[str(rom)].duplicate_group = label
            by_rom[str(rom)].issues.append("possible-duplicate")

    issue_count = sum(bool(r.issues) for r in rows)
    print(f"Advance 0.4 Library Doctor — {len(rows)} ROM(s), {issue_count} with review items\n")
    for r in rows:
        status = "OK" if not r.issues else ", ".join(r.issues)
        dup = f" [{r.duplicate_group}]" if r.duplicate_group else ""
        print(f"{r.title[:42]:<42}  {r.title_source:<16}  {status}{dup}")

    if args.json_report:
        args.json_report.parent.mkdir(parents=True, exist_ok=True)
        args.json_report.write_text(json.dumps([asdict(r) for r in rows], indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"\nReport: {args.json_report}")
    if args.write_missing_metadata:
        print("Metadata stubs were created only where advance.json was missing.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
