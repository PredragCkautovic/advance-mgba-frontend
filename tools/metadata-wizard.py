#!/usr/bin/env python3
"""Create or interactively update one game's advance.json metadata."""
from __future__ import annotations
import argparse, json
from pathlib import Path
IMAGE_EXTS={".png",".jpg",".jpeg",".webp"}; ROM_EXTS={".gba",".agb",".gb",".gbc"}
def prompt(label:str,current:str="")->str:
    shown=f" [{current}]" if current else ""; value=input(f"{label}{shown}: ").strip(); return value or current
def main()->int:
    ap=argparse.ArgumentParser(); ap.add_argument("game_folder",type=Path); args=ap.parse_args(); folder=args.game_folder
    if not folder.is_dir(): raise SystemExit(f"Not a folder: {folder}")
    path=folder/"advance.json"; data={}
    if path.is_file():
        try: data=json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc: raise SystemExit(f"Existing metadata is invalid JSON: {exc}")
    roms=[p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in ROM_EXTS]; imgs=[p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in IMAGE_EXTS]
    guess_title=folder.name if folder.name.lower()!="roms" else (roms[0].stem if roms else ""); cover_guess=next((p.name for p in imgs if p.stem.lower() in {"cover","boxart","front"}),imgs[0].name if imgs else "")
    print("Advance 0.1 Metadata Wizard — press Enter to keep a value.\n")
    data["title"]=prompt("Title",str(data.get("title",guess_title))); data["short_title"]=prompt("Short title",str(data.get("short_title",data["title"]))); data["author"]=prompt("Author",str(data.get("author",""))); data["version"]=prompt("Version",str(data.get("version",""))); data["base_game"]=prompt("Base game",str(data.get("base_game","")))
    genre=prompt("Genres (comma separated)",", ".join(data.get("genre",[]))); tags=prompt("Tags (comma separated)",", ".join(data.get("tags",[]))); data["genre"]=[x.strip() for x in genre.split(",") if x.strip()]; data["tags"]=[x.strip() for x in tags.split(",") if x.strip()]
    data["description"]=prompt("Description",str(data.get("description",""))); year=prompt("Release year",str(data.get("release_year",0) or "")); data["release_year"]=int(year) if year.isdigit() else 0; data["cover"]=prompt("Cover filename",str(data.get("cover",cover_guess))); data["banner"]=prompt("Banner filename",str(data.get("banner",""))); shots=prompt("Screenshots (comma separated)",", ".join(data.get("screenshots",[]))); data["screenshots"]=[x.strip() for x in shots.split(",") if x.strip()]
    path.write_text(json.dumps(data,indent=2,ensure_ascii=False)+"\n",encoding="utf-8"); print(f"Saved: {path}"); return 0
if __name__=="__main__": raise SystemExit(main())
