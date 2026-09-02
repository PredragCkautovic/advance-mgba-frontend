    strength = strength < 0 ? config.backdropIntensity : strength;
    strength = std::clamp(strength, 0, 100);
    int iw = 0, ih = 0;
    SDL_Color avg{};
    SDL_Texture* texture = !game->bannerPath.empty()
        ? loadImage(game->bannerPath, iw, ih, &avg)
        : loadCover(*game, iw, ih, &avg);
    if (!texture || iw <= 0 || ih <= 0) return;

    const int alpha = 16 + strength / 3;
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(alpha));
    const float scale = std::max(static_cast<float>(W - SIDEBAR_W) / iw, static_cast<float>(H) / ih);
    const int dw = static_cast<int>(iw * scale);
    const int dh = static_cast<int>(ih * scale);
    const int baseX = SIDEBAR_W + (W - SIDEBAR_W - dw) / 2;
    const int baseY = (H - dh) / 2;
    for (int oy : {-14, 0, 14}) for (int ox : {-18, 0, 18}) {
        SDL_Rect dst{baseX + ox, baseY + oy, dw, dh};
        SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    }
    SDL_SetTextureAlphaMod(texture, 255);

    // Tint derived from the selected cover, then crush it back to black.
    const SDL_Color accent = accentColor(config);
    const SDL_Color tint = mixColor(avg, accent, 0.35f, static_cast<Uint8>(20 + strength / 4));
    fill({SIDEBAR_W, 0, W - SIDEBAR_W, H}, tint);
    fill({SIDEBAR_W, 0, W - SIDEBAR_W, H}, rgba(0, 0, 0, static_cast<Uint8>(205 - strength / 2)));
    // Bottom readability ramp.
    for (int i = 0; i < 7; ++i) {
        fill({SIDEBAR_W, H - 240 + i * 28, W - SIDEBAR_W, 32}, rgba(0, 0, 0, static_cast<Uint8>(25 + i * 18)));
    }
}

void Renderer::drawSidebarIcon(int cx, int cy, int type, SDL_Color c) {
    constexpr double pi = 3.14159265358979323846;
    switch (type) {
        case 0: // Home
            line(cx - 20, cy + 2, cx, cy - 16, c);
            line(cx, cy - 16, cx + 20, cy + 2, c);
            roundedOutline({cx - 16, cy - 1, 32, 24}, 7, c, 4);
            roundedOutline({cx - 5, cy + 10, 10, 12}, 3, c, 3);
            break;
        case 1: // Library grid
            for (int yy : {-12, 12}) for (int xx : {-12, 12})
                roundedOutline({cx + xx - 7, cy + yy - 7, 14, 14}, 4, c, 4);
            break;
        case 2: // Collections / layers
            roundedOutline({cx - 21, cy - 13, 31, 23}, 6, c, 4);
            roundedOutline({cx - 10, cy - 4, 31, 23}, 6, c, 4);
            line(cx - 2, cy + 1, cx + 10, cy + 1, c);
            break;
        case 3: { // Favorites star
            int px[10], py[10];
            for (int i = 0; i < 10; ++i) {
                const double a = -pi / 2.0 + i * pi / 5.0;
                const double rr = (i % 2 == 0) ? 20.0 : 9.0;
                px[i] = cx + static_cast<int>(std::cos(a) * rr);
                py[i] = cy + static_cast<int>(std::sin(a) * rr);
            }
            for (int i = 0; i < 10; ++i) line(px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], c);
            break;
        }
        case 4: // Recent clock
            circle(cx, cy, 19, c, false);
            line(cx, cy, cx, cy - 10, c);
            line(cx, cy, cx + 10, cy + 7, c);
            break;
        case 5: // Search
            circle(cx - 5, cy - 5, 14, c, false);
            line(cx + 7, cy + 7, cx + 21, cy + 21, c);
            break;
        default: // Settings gear
            circle(cx, cy, 12, c, false);
            circle(cx, cy, 4, c, false);
            for (int i = 0; i < 8; ++i) {
                const double a = i * pi / 4.0;
                line(cx + static_cast<int>(std::cos(a) * 16), cy + static_cast<int>(std::sin(a) * 16),
                     cx + static_cast<int>(std::cos(a) * 22), cy + static_cast<int>(std::sin(a) * 22), c);
            }
            break;
    }
}

void Renderer::drawSidebar(Screen screen, Shelf shelf, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const SDL_Color secondary = secondaryColor(config);
    const std::uint64_t now = SDL_GetTicks64();
    if (sidebarFrameAt_ == 0) sidebarFrameAt_ = now;
    const float dt = std::min(0.05f, static_cast<float>(now - sidebarFrameAt_) / 1000.0f);
    sidebarFrameAt_ = now;
    const float target = sidebarFocused_ ? 1.0f : 0.0f;
    if (!sidebarMotionEnabled_) sidebarProgress_ = target;
    else {
        const float speed = sidebarFocused_ ? 8.8f : 10.2f;
        if (sidebarProgress_ < target) sidebarProgress_ = std::min(target, sidebarProgress_ + dt * speed);
        else if (sidebarProgress_ > target) sidebarProgress_ = std::max(target, sidebarProgress_ - dt * speed);
    }

    fill({0, 0, SIDEBAR_W, H}, rgba(2, 2, 5, 252));
    fill({0, 0, 4, H}, accent);
    fill({4, 0, 5, H}, rgba(255,255,255,12));
    line(SIDEBAR_W - 1, 0, SIDEBAR_W - 1, H, rgba(42, 42, 52));

    // Premium brand pedestal.
    roundedFill({12, 10, 74, 74}, 24, rgba(29, 22, 71, 255));
    roundedFill({14, 12, 70, 70}, 22, rgba(122, 92, 247, 255));
    roundedFill({17, 15, 64, 64}, 20, rgba(105, 84, 228, 255));
    roundedOutline({14, 12, 70, 70}, 22, rgba(255,255,255,170), 1);
    roundedFill({24, 28, 48, 28}, 9, rgba(28, 39, 96, 255));
    roundedFill({27, 31, 42, 22}, 6, rgba(57, 214, 226, 255));
    roundedFill({19, 39, 10, 4}, 2, rgba(9, 17, 49, 255));
