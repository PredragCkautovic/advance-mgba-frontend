# Contributing to Advance

Advance is a Nintendo Switch homebrew frontend for an existing mGBA installation.

## Development

The supported toolchain is devkitPro/devkitA64 with libnx and the SDL2 Switch portlibs used by the project Makefile.

The easiest reproducible build path is:

```bash
chmod +x scripts/*.sh
./scripts/build-docker.sh
```

Before opening a pull request, run the available syntax/tooling checks and verify the result on real Switch hardware when the change affects input, rendering, account APIs, launching, or SD-card paths.

## Scope

Good contributions include UI polish, performance, metadata/library handling, accessibility, controller/touch UX, documentation, and robust mGBA handoff behavior.

Do not submit ROMs, copyrighted game artwork, proprietary firmware/files, or bundled commercial game content.
