# Homebrew App Store release notes

Advance is structured for public homebrew distribution, but a store submission should happen only after the v0.4 hardware QA checklist is complete.

## Distribution posture

- MIT-licensed source is included.
- Advance is non-destructive: it reads the user's library and writes its own configuration/state files under `sd:/switch/advance/` plus optional `advance.json` metadata files only when the user explicitly creates them with the desktop tool.
- It does **not** include mGBA, ROMs, BIOS files, patches, downloaded content, or copyrighted box-art packs.
- mGBA remains an external dependency selected by path in `config.ini`.
- The release ZIP contains only Advance, its default configuration, a local README, and a metadata example.
- `Makefile` embeds application title, author, version and icon into the NRO/NACP.
- GitHub Actions can build the NRO from a tagged public source revision.

## Before submitting

1. Publish the exact source in a public repository and tag the tested commit as `v0.1.0`.
2. Build the release from that tag and create the SD-root ZIP with `./scripts/make-release.sh`.
3. Verify the SHA-256 hashes of both source and release archives.
4. Capture clean screenshots on real Switch hardware using artwork you have permission to display publicly.
5. Clearly state that mGBA is required separately and that users must provide their own legally obtained game dumps/artwork.
6. Provide the public source repository, issue tracker and release URL in the store submission.
7. Test the launch contexts you intend to support. Full-memory/title-override hbmenu should be part of QA, and applet-mode limitations must be documented if present.
8. Run `RELEASE-CHECKLIST.md` completely before submission.

The current Homebrew App Store project directs developers to its official submit/request page for repository guidelines and app submission. Re-check that page immediately before publishing because store requirements can change.

## Suggested store copy

**Advance** is a native Nintendo Switch library frontend for mGBA. It provides an OLED-focused Home dashboard, cover library, smart and custom collections, metadata-aware search, favorites, completion/history tracking, themes, dynamic artwork backdrops, Switch profile integration and ROM-hack-friendly metadata. Advance does not include games or mGBA.

## Dependency disclosure

Default paths:

```text
mGBA: sdmc:/switch/mgba.nro
ROMs: sdmc:/mGBA/Roms
```

Both are configurable.
