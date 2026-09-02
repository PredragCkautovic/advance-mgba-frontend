<div align="center">

<img src="assets/store-icon.png" alt="Advance logo" width="180">

# Advance

### A premium Game Boy Advance library frontend for Nintendo Switch

Turn an existing **mGBA** setup into a fast, polished, controller-first library with cover art, collections, favorites, play history, metadata, search, Switch profile integration, and an OLED-focused interface.

[![Build Advance](https://github.com/PredragCkautovic/advance-mgba-frontend/actions/workflows/build.yml/badge.svg)](https://github.com/PredragCkautovic/advance-mgba-frontend/actions/workflows/build.yml)
[![Version](https://img.shields.io/badge/version-v0.1.0-ff3355)](VERSION)
[![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-e60012)](#requirements)
[![License](https://img.shields.io/badge/license-MIT-7f5af0)](LICENSE)

**Public alpha · v0.1**

[Features](#features) · [Install](#installation) · [Metadata](#game-metadata) · [Controls](#controls) · [Build](#building-from-source)

</div>

---

## What is Advance?

**Advance** is a native Nintendo Switch homebrew frontend for people who already use **mGBA** and want their Game Boy Advance collection to feel more like a complete console library than a folder of ROM files.

It scans your existing collection, matches local artwork, builds personalized shelves, tracks library activity, exposes rich metadata, and launches games through your existing mGBA installation.

Advance is a **frontend, not an emulator**.

It does **not** include:

- ROMs
- BIOS files
- commercial box art
- game downloads
- mGBA itself

Use ROMs and artwork you are legally entitled to use.

> [!IMPORTANT]
> Advance v0.1 is an early public alpha. The current build is functional and CI-tested, but compatibility, performance, UI polish, and release packaging are still being actively improved.

---

## Features

### Console-style Home experience

- dynamic game-art backgrounds
- time-aware greeting using the Switch clock
- real Switch profile nickname and avatar when Horizon exposes them
- **Continue Playing**
- **Recently Added**
- **Favorites**
- **Most Played**
- contextual Pokémon / ROM Hack / Completed shelves
- **Surprise Me** random-game discovery
- Play / Resume state directly from the hero card

### Full library frontend

- recursive `.gba` ROM scanning
- configurable 4–7 column layouts
- configurable 2–3 row layouts
- local cover-art discovery
- branded fallback artwork
- newly-added game indicators
- Favorites, Recent, Search, and Collections views
- sorting by:
  - A–Z
  - recently played
  - launch count
  - approximate play time
  - recently added
  - favorites

### Expandable navigation drawer

The left rail stays compact while browsing and expands into a full overlay drawer when focused.

Available destinations:

- Home
- Library
- Collections
- Favorites
- Recent
- Search
- Settings

The drawer overlays the current screen instead of shifting the entire layout.

### Game details and organization

Each title can expose:

- full display title
- author
- version
- base game
- genres
- tags
- release year
- description
- screenshots
- launch count
- last played timestamp
- approximate play time
- favorite state
- completed state
- hidden state
- custom collection membership

### Personalization

- six OLED-oriented themes:
  - Crimson
  - Atomic Purple
  - Emerald
  - Midnight Gold
  - Ice Blue
  - Neon Coral
- adaptive accent colors derived from game artwork
- adjustable dynamic backdrop intensity
- screen transitions
- launch transition
- UI sounds and volume control
- touchscreen support
- system clock, Wi-Fi, battery, and charging indicators

---

## Requirements

You need:

- a Nintendo Switch capable of running homebrew
- mGBA already installed
- your own Game Boy Advance ROM dumps
- an SD card accessible from the Switch homebrew environment

The default paths are:

```text
ROM library:  sdmc:/mGBA/Roms
mGBA:         sdmc:/switch/mgba.nro
Advance:      sdmc:/switch/advance/advance.nro
```

These paths are configurable.

---

## Recommended SD layout

```text
SD:/
├── mGBA/
│   └── Roms/
│       ├── Example Game/
│       │   ├── game.gba
│       │   ├── cover.png
│       │   ├── banner.jpg              # optional
│       │   ├── screenshot1.png         # optional
│       │   └── advance.json            # optional, recommended
│       │
│       └── Another Game/
│           ├── A-3244 e.gba
│           └── cover.png
│
└── switch/
    ├── mgba.nro
    └── advance/
        ├── advance.nro
        ├── config.ini
        ├── state.tsv
        ├── titles.tsv
        ├── collections.tsv
        ├── session.pending             # temporary play-time state
        └── diagnostics.log
```

Cryptic filenames such as `A-3244 e.gba` are completely fine. **Advance never renames or modifies your ROM files.**

---

## Installation

### Fresh install

1. Build or obtain `advance.nro`.
2. Mount your Switch SD card.
3. Run:

```bash
./scripts/install-to-sd.sh /path/to/mounted/sd
```

Or manually place the application at:

```text
SD:/switch/advance/advance.nro
```

### Upgrade an existing Advance installation

```bash
./scripts/upgrade-existing-sd.sh /path/to/mounted/sd
```

The upgrade script preserves your existing:

- ROM library
- mGBA installation
- artwork
- configuration
- favorites and history
- title overrides
- collections

---

## Game metadata

Advance works without manual metadata, but `advance.json` gives each game a definitive identity and richer Details page.

Place it beside the ROM:

```text
Example Game/
├── game.gba
├── cover.png
├── banner.jpg
├── screenshot1.png
└── advance.json
```

Example:

```json
{
  "title": "Example ROM Hack Definitive",
  "short_title": "Example Hack",
  "author": "ROM Hack Author",
  "version": "4.1",
  "base_game": "Game Boy Advance",
  "genre": ["RPG", "ROM Hack"],
  "tags": ["Challenge", "Quality of Life"],
  "description": "Optional metadata gives Advance a richer library presentation.",
  "release_year": 2026,
  "cover": "cover.png",
  "banner": "banner.jpg",
  "screenshots": [
    "screenshot1.png",
    "screenshot2.png"
  ]
}
```

See [`examples/advance.json`](examples/advance.json).

### Display-name resolution

Advance resolves names in this order:

1. manual override from `titles.tsv`
2. `advance.json`
3. `title.txt`, `name.txt`, or legacy sidecar metadata
4. best descriptive folder / artwork filename
5. internal GBA cartridge title
6. raw ROM filename as a final fallback

For large ROM-hack collections, `advance.json` is the recommended source of truth.

---

## Controls

### Navigation drawer

| Input | Action |
|---|---|
| Left from left-most content | Open drawer |
| Up / Down | Select destination |
| A | Open destination |
| B / Right | Close drawer |
| Touch | Expand / select / open |

### Home

| Input | Action |
|---|---|
| D-pad / Left Stick | Browse shelves |
| A | Play / Resume |
| X | Details |
| Y | Favorite |
| R | Surprise Me |
| L | Library |
| - | Search |
| + | Settings |
| B | Exit |

### Library

| Input | Action |
|---|---|
| D-pad / Left Stick | Browse |
| A | Play |
| X | Details |
| Y | Cycle sort mode |
| L | Favorites |
| R | Recently Played |
| ZL / ZR | Previous / next page |
| - | Search |
| B | Home |

### Details

| Input | Action |
|---|---|
| A | Play |
| Y | Favorite |
| X | Edit display title |
| R | Custom collections |
| ZR | Mark completed |
| ZL | Hide / unhide |
| - | Restore automatic title |
| B | Back |

---

## Building from source

### Docker — recommended

The repository includes a reproducible devkitPro Docker build:

```bash
chmod +x scripts/*.sh tools/*.py
./scripts/build-docker.sh
```

Output:

```text
advance.nro
```

The script restores generated build artifacts to your host UID/GID, avoiding root-owned build directories.

### Local devkitPro

```bash
./scripts/build-local.sh
```

Requires:

- devkitPro / devkitA64
- libnx
- SDL2
- SDL2_image
- SDL2_ttf

### Continuous integration

Every push to `main` is built by GitHub Actions using the official devkitPro image. The public v0.1 source has already completed this CI build successfully.

Build artifacts are uploaded by the workflow as **Advance-Switch**.

---

## Desktop library tools

Advance ships with utilities for cleaning up and enriching large ROM collections.

### Library doctor

```bash
python3 tools/library-doctor.py "/path/to/mGBA/Roms"
```

Generate a JSON report:

```bash
python3 tools/library-doctor.py "/path/to/mGBA/Roms" \
  --json-report ~/Downloads/advance-library-report.json
```

Create starter `advance.json` files only where metadata is missing:

```bash
python3 tools/library-doctor.py "/path/to/mGBA/Roms" \
  --write-missing-metadata
```

The doctor can identify:

- missing artwork
- weak or cryptic titles
- invalid metadata
- missing referenced files
- archive-ID-style folder names
- possible duplicate ROM payloads

It never renames ROMs by default.

### Metadata wizard

```bash
python3 tools/metadata-wizard.py "/path/to/mGBA/Roms/Example Game"
```

### Artwork sync

`tools/cover-sync.py` can fuzzy-match **locally owned** artwork to games. It does not download artwork from the internet.

---

## Approximate play-time tracking

Advance hands execution to mGBA rather than embedding the emulator.

Before launching a game, Advance writes a small pending-session marker. When Advance is opened again, it reconciles the elapsed time and adds it to that game's library history, with a safety cap of 12 hours per unresolved session.

Because Advance is not receiving frame-perfect telemetry directly from mGBA, the UI deliberately describes this as **approximate play time**.

---

## Project status

### v0.1 public alpha

Already working:

- native Switch frontend
- real mGBA launching
- library scanning
- artwork
- Home experience
- collections
- favorites
- recent games
- search
- metadata
- profile integration
- play history
- themes
- touch input
- CI builds

Current priorities for the `v0.x` line:

- broader hardware QA
- faster library scanning and caching
- additional UI polish
- stronger error recovery
- improved metadata workflows
- release packaging
- Homebrew App Store readiness

See [`CHANGELOG.md`](CHANGELOG.md) and [`RELEASE-CHECKLIST.md`](RELEASE-CHECKLIST.md).

---

## Contributing

Issues, bug reports, UI feedback, and pull requests are welcome.

Please read [`CONTRIBUTING.md`](CONTRIBUTING.md) before submitting larger changes.

When reporting a Switch-specific issue, include where possible:

- Advance version
- Atmosphère / homebrew launch mode
- whether full-memory title override or applet mode was used
- relevant `diagnostics.log` output
- reproduction steps

---

## Legal

Advance is an independent homebrew project and is **not affiliated with or endorsed by Nintendo or the mGBA project**.

Nintendo, Game Boy Advance, and related names and marks belong to their respective owners.

Advance does not provide ROMs, BIOS files, copyrighted game artwork, or commercial game downloads.

---

## License

Advance is released under the **MIT License**. See [`LICENSE`](LICENSE).

---

<div align="center">

### Your GBA library. Your way.

Built for the Nintendo Switch homebrew community.

</div>
