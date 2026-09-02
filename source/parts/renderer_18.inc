    text(fonts_.tiny, "Advance stores settings in a human-readable config.ini.", previewX + 24, listY + 258, rgba(115,115,128), previewW - 48);

    roundedFill({previewX, listY + 344, previewW, 204}, 24, rgba(7,7,12,232));
    roundedOutline({previewX, listY + 344, previewW, 204}, 24, rgba(52,52,64), 1);
    text(fonts_.tiny, "RELEASE TOOLS", previewX + 24, listY + 368, accent);
    text(fonts_.body, "Built for real libraries", previewX + 24, listY + 393, rgba(255,255,255), previewW - 48);
    wrappedText(fonts_.tiny, "advance.json metadata, smart collections, custom shelves, title overrides, play-time reconciliation, diagnostics, and safe 2.x migration are all included.",
                {previewX + 24, listY + 428, previewW - 48, 67}, rgba(146,146,159), 3);
    roundedFill({previewX + 24, listY + 505, 134, 28}, 12, rgba(accent.r,accent.g,accent.b,30));
    text(fonts_.tiny, "R  ABOUT", previewX + 54, listY + 511, accent);

    drawFooter({{"←/→","Change"},{"A","Select"},{"X","Rescan"},{"R","About"},{"B","Back"}});
    drawScreenTransition(Screen::Settings, config);
    drawSidebarOverlay(Screen::Settings, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderAbout(const Config& config, const std::string& toast) {
    drawBaseBackdrop(config);
    drawSidebar(Screen::About, Shelf::All, config);
    drawTopChrome("ADVANCE  /  ABOUT", "Advance 0.1", "A definitive Game Boy Advance library experience", config);
    const SDL_Color accent = accentColor(config);
    const int x = CONTENT_X, y = 118, w = W - CONTENT_X - CONTENT_R;
    roundedFill({x, y, w, 470}, 30, rgba(6,6,10,238));
    roundedOutline({x, y, w, 470}, 30, rgba(53,53,65), 1);

    drawBrandHandheld(x + 36, y + 45, 130, 92, accent);
    text(fonts_.mega, "ADVANCE", x + 190, y + 34, rgba(255,255,255), 620);
    text(fonts_.body, "Your GBA library. Your profile. Your shelves. Your style.", x + 194, y + 105, rgba(178,178,190), 700);

    const std::array<std::pair<const char*,const char*>,6> features = {{
        {"HOME", "Continue Playing, Recently Added, Favorites, Most Played, and smart discovery."},
        {"METADATA", "advance.json supports titles, authors, versions, base games, genres, tags, descriptions, banners, and screenshots."},
        {"COLLECTIONS", "Automatic shelves plus unlimited personal collections stored locally on your SD card."},
        {"HISTORY", "Launches, last played, completion state, favorites, hidden games, and approximate play time."},
        {"PERSONAL", "Your Switch avatar and nickname, six OLED themes, adaptive accents, transitions, touch, motion, dynamic backgrounds, and UI sounds."},
        {"OPEN", "No ROM downloads, no bundled copyrighted art, and no modification of your existing mGBA library."}
    }};
    for (int i = 0; i < 6; ++i) {
        const int col = i % 2, row = i / 2;
        const int fx = x + 40 + col * 560, fy = y + 195 + row * 76;
        roundedFill({fx, fy, 520, 62}, 15, rgba(11,11,16));
        text(fonts_.tiny, features[i].first, fx + 16, fy + 11, accent, 120);
        text(fonts_.tiny, features[i].second, fx + 126, fy + 11, rgba(180,180,191), 375);
    }

    text(fonts_.tiny, "Advance 0.1.0 · Native Nintendo Switch homebrew · mGBA remains the emulator backend",
         x + 40, y + 436, rgba(116,116,129), w - 80);
    drawFooter({{"A/B","Back"}});
    drawScreenTransition(Screen::About, config);
    drawSidebarOverlay(Screen::About, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderLaunch(const Game& game, const Config& config) {
    drawDynamicBackdrop(&game, config, 88);
    const SDL_Color accent = accentColor(config);
    fill({0, 0, W, H}, rgba(0,0,0,120));
    const int panelW = 660, panelH = 300;
    const int px = W / 2 - panelW / 2, py = H / 2 - panelH / 2;
    roundedFill({px + 6, py + 10, panelW, panelH}, 30, rgba(0,0,0,150));
    roundedFill({px, py, panelW, panelH}, 30, rgba(6,6,11,242));
    roundedOutline({px, py, panelW, panelH}, 30, rgba(accent.r,accent.g,accent.b,155), 2);
    roundedFill({px, py, 7, panelH}, 4, accent);

    drawCoverCard(game, {px + 28, py + 28, 150, 220}, false, config, 1.0f, true);
    text(fonts_.tiny, "HANDING OFF TO mGBA", px + 208, py + 46, accent, 380);
    text(fonts_.large, game.title, px + 208, py + 74, rgba(255,255,255), 410);
    text(fonts_.small, game.launches > 1 ? "Welcome back." : "Starting your adventure.", px + 208, py + 130, rgba(182,182,194), 410);
    roundedFill({px + 208, py + 182, 338, 4}, 2, rgba(55,55,68));
    const int travel = static_cast<int>((SDL_GetTicks64() / 4) % 280);
    roundedFill({px + 208 + travel, py + 182, 58, 4}, 2, accent);
    text(fonts_.tiny, "ADVANCE keeps your library, artwork, favorites, and play history ready for your return.",
         px + 208, py + 207, rgba(125,125,139), 390);
}

void Renderer::renderBoot(const Config& config, const std::string& message) {
    drawBaseBackdrop(config);
    const SDL_Color accent = accentColor(config);
    const int cx = W / 2, cy = H / 2 - 20;
    for (int i = 0; i < 8; ++i) {
        const int d = 105 + i * 26;
