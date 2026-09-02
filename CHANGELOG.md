# Changelog

## 0.4.0 — Definitive polish pass

- bundled Lucide-based navigation icon assets with ISC attribution
- added a dedicated runtime sidebar brand mark and reused it across sidebar, Settings, About, boot, and missing-art states
- refined compact and expanded navigation focus states
- widened and polished the navigation drawer
- strengthened top-level typography and header hierarchy
- rebuilt footer hints as compact command groups
- upgraded major-screen transitions with a cinematic diagonal light sweep
- filled unused cover-card space with a dimmed artwork crop while preserving the full uncropped cover on top
- improved selected-cover bloom and focus animation
- rebuilt the Library selected-game strip with cover thumbnail, title hierarchy, state chips, page status, and Play/Resume action
- fixed Home activity-stat truncation
- improved empty Collection cards
- simplified Settings presentation for end users
- fixed archive/product-code title fallbacks such as `A-BPEE e`; descriptive metadata or ROM-header title now wins
- mirrored the archive-code fix in desktop library-doctor tooling
- install and upgrade scripts now deploy runtime brand and navigation assets automatically

## 0.1.0 — First public alpha

Advance v0.1 is the first public release line of the project. It packages the current Switch-tested frontend foundation with:

- OLED-focused Home and Library experiences
- expandable controller/touch navigation drawer
- real Switch profile nickname/avatar integration when available
- mGBA handoff using configurable paths
- recursive ROM scanning and local cover artwork
- ROM-hack-friendly display names and optional `advance.json` metadata
- Continue Playing, Recently Added, Favorites, Most Played, Search, and Collections
- smart and user-created collections
- favorites, completion, hidden-state, launch history, and approximate play-time tracking
- dynamic artwork backdrops and adaptive game accents
- six visual themes
- Details, Settings, About, boot, and launch transition screens
- controller, touchscreen, battery, clock, network, and UI sound support
- desktop metadata/library audit tooling
- reproducible devkitPro Docker build workflow
- release/install/upgrade helper scripts

This is an early public release. Compatibility, UI polish, metadata handling, and public distribution packaging will continue to improve.
