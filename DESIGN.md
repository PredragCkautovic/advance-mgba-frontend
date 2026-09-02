# Advance 0.1 Design System

Advance is intentionally an original console UI, not a clone of any first-party Nintendo application.

## Identity

- True black remains the canvas on OLED.
- The selected game's artwork becomes atmosphere rather than decoration.
- Accent color communicates focus; white communicates primary content.
- The left rail is Advance's permanent brand spine.
- Cover art remains the hero asset, especially for ROM-hack libraries.

## Main surfaces

### Home
A destination, not a file browser. The active game becomes a hero with history and primary actions, followed by contextual shelves.

### Library
A fast, dense 6×2 default gallery with responsive 4–7 × 2–3 options.

### Collections
Large collage cards turn metadata and library state into browseable concepts.

### Details
The game becomes a product page: art, story, author/version/base game, screenshots, history, and user state.

## Theme presets

- Crimson — Advance signature
- Atomic Purple — translucent-retro inspiration
- Emerald — vivid handheld green
- Midnight Gold — dark premium warmth
- Ice Blue — clean futuristic blue
- Neon Coral — energetic pink/coral

## Motion

Motion should communicate state change, never delay input:
- 170 ms focus lift
- one-shot 680 ms shine on focus
- immediate controller response
- motion can be disabled completely

## Sound

UI sounds are synthesized in memory at runtime. No external copyrighted sound assets are bundled.


## v0.1 UI foundation

- NRO/store branding uses the neon handheld Advance mark in `icon.jpg` and `assets/`.
- Selected game art can tint focus rings, action buttons, chips, and hero accents when Adaptive Game Accents is enabled.
- Home uses eight 132×138 shelf cards for stronger handheld legibility and visual hierarchy.
- Major screen changes use a short 170 ms black fade when Screen Transitions is enabled.
- Launching a game renders a dedicated mGBA handoff card before the application exits.
- Missing artwork uses the in-app handheld mark instead of a plain letter placeholder.
