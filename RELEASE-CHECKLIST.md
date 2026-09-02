# Advance 0.1 public-release checklist

Use this before tagging `v0.1.0` or submitting Advance to a public homebrew catalog.

## Hardware stability

- [ ] Launch through full-memory hbmenu/title override.
- [ ] Test Album/applet mode if it will be claimed as supported; document limitations.
- [ ] Cold boot Advance at least 10 times.
- [ ] Leave Advance open for at least 30 minutes.
- [ ] Navigate rapidly for at least 10 minutes without a crash or progressive slowdown.
- [ ] Suspend/resume the console while Advance is open if that use case is supported.
- [ ] Verify clean exit behavior.

## System integration

- [ ] Real Switch profile nickname appears.
- [ ] Real Switch avatar appears.
- [ ] Profile fallback image/name works.
- [ ] Battery percentage and charging state are correct.
- [ ] Network state is correct online and offline.
- [ ] Clock is correct.
- [ ] Touch input behaves correctly.
- [ ] UI sounds work and respect volume/off settings.

## Home

- [ ] Continue Playing appears only when meaningful.
- [ ] Recently Added is ordered correctly.
- [ ] Favorites shelf updates immediately.
- [ ] Most Played ordering is correct.
- [ ] Smart Pokémon / ROM Hacks / Completed shelves appear only when applicable.
- [ ] Surprise Me chooses only visible playable entries.
- [ ] Empty/very small libraries still look intentional.
- [ ] Dynamic hero/backdrop updates smoothly between games.

## Library

- [ ] 4, 5, 6 and 7 column layouts.
- [ ] 2- and 3-row layouts.
- [ ] A–Z sorting.
- [ ] Recent sorting.
- [ ] Launch-count sorting.
- [ ] Play-time sorting.
- [ ] Recently-added sorting.
- [ ] Favorites sorting.
- [ ] ZL/ZR paging across a multi-page library.
- [ ] Favorites filter.
- [ ] Recent filter.
- [ ] Hidden titles remain hidden unless Show Hidden is enabled.
- [ ] Long titles and unusual cover aspect ratios render cleanly.
- [ ] Missing/corrupt artwork is handled without repeated stalls.

## Collections

- [ ] Favorites smart collection.
- [ ] Completed smart collection.
- [ ] Never Played smart collection.
- [ ] Recently Added smart collection.
- [ ] Pokémon / RPG / ROM Hacks smart collections with relevant libraries.
- [ ] Create multiple custom collections.
- [ ] Add/remove games from custom collections.
- [ ] Custom collection membership persists after restart.
- [ ] Collection with zero members has a polished empty state.

## Search

- [ ] Search by display title.
- [ ] Search by author/version/base game.
- [ ] Search by genre/tag.
- [ ] Search by description.
- [ ] Search by folder/file/game code.
- [ ] No-results state is polished.
- [ ] Search survives long ROM-hack names and Unicode-safe text supported by current font path.

## Details / metadata

- [ ] `advance.json` title wins over automatic title resolution.
- [ ] `short_title`, author, version, base game, genre, tags, description and year render correctly.
- [ ] Metadata-selected cover works.
- [ ] Metadata-selected banner works.
- [ ] Screenshot strip works with 0, 1 and multiple screenshots.
- [ ] Manual title override persists after restart.
- [ ] Resetting a title restores automatic metadata.
- [ ] Favorite toggle persists.
- [ ] Completed toggle persists.
- [ ] Hide/unhide persists.
- [ ] Collection picker works.
- [ ] Description wrapping cannot overlap actions or footer.

## Launching / state

- [ ] Launch at least 20 different ROMs into mGBA.
- [ ] Paths containing spaces and punctuation work.
- [ ] Launch confirmation option works on/off.
- [ ] Launch count increments exactly once per launch.
- [ ] Last-played timestamp persists.
- [ ] Pending play-session reconciliation works after returning to Advance.
- [ ] Extremely short launches do not add bogus play time.
- [ ] Long elapsed gaps are capped as designed.
- [ ] No ROM file is renamed, patched or modified.

## Visual QA on OLED

- [ ] Crimson theme.
- [ ] Atomic Purple theme.
- [ ] Emerald theme.
- [ ] Midnight Gold theme.
- [ ] Ice Blue theme.
- [ ] Neon Coral theme.
- [ ] Dynamic backdrop on/off.
- [ ] Backdrop intensity range.
- [ ] Motion on/off.
- [ ] Cover labels on/off.
- [ ] Long profile nickname.
- [ ] Low-battery state.
- [ ] Offline state.
- [ ] Home, Library, Collections, Details, Collection Picker, Settings and About screens all match the same design language.
- [ ] No important element is clipped at 1280×720.

## Desktop tooling

- [ ] `library-doctor.py` is read-only without `--write-missing-metadata`.
- [ ] `--write-missing-metadata` only creates missing metadata files and never overwrites existing `advance.json`.
- [ ] Duplicate reporting does not delete anything.
- [ ] `metadata-wizard.py` preserves existing valid fields it does not edit.
- [ ] `library-audit.py` compatibility wrapper works.
- [ ] `cover-sync.py` never downloads artwork.

## Build / release hygiene

- [ ] `./scripts/build-docker.sh` succeeds with current `devkitpro/devkita64:latest`.
- [ ] Docker-created artifacts are owned by the host user afterward.
- [ ] `./scripts/build-local.sh` succeeds on a supported local devkitPro installation.
- [ ] `python3 -m py_compile tools/*.py` succeeds.
- [ ] Every shell script passes `bash -n`.
- [ ] `CHANGELOG.md` matches `0.1.0`.
- [ ] `APP_VERSION` in `Makefile` is `0.1.0`.
- [ ] `./scripts/make-release.sh` produces the expected SD-root ZIP.
- [ ] Fresh install script works on an empty `switch/advance` folder.
- [ ] Upgrade script preserves config, state, title overrides and collections from 2.x.
- [ ] No ROMs, BIOS files, commercial game assets, copyrighted cover packs or mGBA binaries are included.
- [ ] Source archive contains LICENSE, README, build instructions and metadata example.
- [ ] Store screenshots are captured from real hardware.
- [ ] Store description discloses mGBA and user-provided game dumps as requirements.
- [ ] Public source tag, release artifact and submitted store version all identify the same build.
