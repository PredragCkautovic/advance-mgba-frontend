# Advance 0.1

**Advance** is a native Nintendo Switch homebrew frontend that turns an existing mGBA ROM collection into a polished, personal Game Boy Advance library experience.

Advance does **not** emulate games itself and does not include ROMs, copyrighted game artwork, or downloads. It launches the user's existing mGBA installation and works with locally owned ROM dumps and artwork.

## v0.1 — first public preview

Advance v0.1 is the first public preview of the frontend. It includes the polished OLED-focused interface, expandable navigation drawer, real Switch profile integration, library scanning, cover artwork, metadata, collections, favorites, recent games, search, play history, themes, touch support, and mGBA launching.

The project is still early and the `v0.x` line should be treated as preview software while hardware QA and store-readiness work continue.

## Highlights

- native Nintendo Switch homebrew application
- mGBA as the emulator backend
- recursive GBA ROM scanning
- per-game cover art and optional metadata
- Home, Library, Collections, Favorites, Recent, Search, Details, Settings and About screens
- expandable controller/touch navigation drawer
- real Switch profile nickname and avatar when available
- OLED-oriented themes and dynamic artwork backdrops
- favorites, completed/hidden state, launch counts and approximate play time
- custom collections
- title overrides for ROM-hack-heavy libraries
- controller and touch input
- release-oriented build scripts and GitHub Actions workflow

## Expected SD layout

```text
SD:/
├── mGBA/
│   └── Roms/
│       ├── Example Game/
│       │   ├── game.gba
│       │   ├── cover.png
│       │   └── advance.json   # optional
│       └── ...
│
└── switch/
    ├── mgba.nro
    └── advance/
        ├── advance.nro
        ├── config.ini
        ├── state.tsv
        ├── titles.tsv
        └── collections.tsv
```

Advance defaults to:

```ini
rom_dir=sdmc:/mGBA/Roms
mgba_nro=sdmc:/switch/mgba.nro
```

## Build

The easiest build route is the included devkitPro Docker script:

```bash
chmod +x scripts/*.sh
./scripts/build-docker.sh
```

The resulting application is:

```text
advance.nro
```

A local devkitPro build is also supported with:

```bash
./scripts/build-local.sh
```

## Install / upgrade

With the Switch SD card mounted:

```bash
./scripts/upgrade-existing-sd.sh /path/to/SWITCH_SD
```

The upgrade script preserves the user's ROM library and persistent Advance state/configuration.

## Metadata

Each game folder may optionally contain `advance.json`:

```json
{
  "title": "Example Game",
  "short_title": "Example",
  "author": "Author",
  "version": "1.0",
  "base_game": "Game Boy Advance",
  "genre": ["RPG"],
  "tags": ["ROM Hack"],
  "description": "Optional description.",
  "cover": "cover.png"
}
```

Metadata is optional. Advance will fall back to sidecar title files, descriptive folder/artwork names, cartridge titles, and finally filenames.

## Legal / project scope

Advance is an independent homebrew project. It is not affiliated with Nintendo or the mGBA project. Advance does not ship ROMs, BIOS files, commercial game artwork, or mGBA itself.

Use ROMs and artwork you are legally entitled to use.

## License

MIT. See [LICENSE](LICENSE).
