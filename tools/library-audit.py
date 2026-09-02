#!/usr/bin/env python3
"""Compatibility audit for Advance 0.1; use library-doctor.py for deeper checks."""
from __future__ import annotations
import argparse
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("rom_root", type=Path)
    ap.add_argument("--csv", type=Path)
    args = ap.parse_args()
    doctor = Path(__file__).with_name("library-doctor.py")
    with tempfile.TemporaryDirectory() as td:
        report = Path(td) / "report.json"
        rc = subprocess.call([sys.executable, str(doctor), str(args.rom_root), "--json-report", str(report)])
        if rc: return rc
        rows = json.loads(report.read_text(encoding="utf-8"))
        if args.csv:
            args.csv.parent.mkdir(parents=True, exist_ok=True)
            with args.csv.open("w", newline="", encoding="utf-8") as f:
                writer = csv.writer(f)
                writer.writerow(["rom", "advance_title", "source", "cover", "issues", "duplicate_group"])
                for row in rows:
                    writer.writerow([row["rom"], row["title"], row["title_source"], row["cover"], ";".join(row["issues"]), row["duplicate_group"]])
            print(f"CSV: {args.csv}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
