        text(fonts_.small, "X  NEW COLLECTION", CONTENT_X + 61, 402, rgba(255,255,255));
    } else {
        constexpr int cols = 3;
        const int gap = 16;
        const int x = CONTENT_X;
        const int y = HEADER_H + 17;
        const int totalW = W - CONTENT_X - CONTENT_R;
        const int cardW = (totalW - gap * 2) / cols;
        const int cardH = 248;
        const std::size_t pageSize = 6;
        const std::size_t page = selected / pageSize;
        const std::size_t start = page * pageSize;
        for (std::size_t slot = 0; slot < pageSize; ++slot) {
            const std::size_t ci = start + slot;
            if (ci >= collections.size()) break;
            const int row = static_cast<int>(slot / cols), col = static_cast<int>(slot % cols);
            drawCollectionCard(games, collections[ci], {x + col * (cardW + gap), y + row * (cardH + gap), cardW, cardH},
                               ci == selected, config);
        }
        const std::size_t pages = (collections.size() + pageSize - 1) / pageSize;
        if (pages > 1) textRight(fonts_.tiny, "PAGE " + std::to_string(page + 1) + " / " + std::to_string(pages), W - CONTENT_R, H - FOOTER_H - 24, rgba(138,138,151));
    }
    drawFooter({{"A","Open"},{"X","New collection"},{"R","Surprise Me"},{"B","Home"},{"+","Settings"}});
    drawScreenTransition(Screen::Collections, config);
    drawSidebarOverlay(Screen::Collections, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderDetails(const Game& game, const Config& config, const std::string& toast) {
    drawDynamicBackdrop(&game, config, 74);
    drawSidebar(Screen::Details, Shelf::All, config);
    drawTopChrome("ADVANCE  /  DETAILS", game.title, game.titleSource, config, true);
    const SDL_Color baseAccent = accentColor(config);
    int accentW = 0, accentH = 0;
    SDL_Color coverAverage = baseAccent;
    loadCover(game, accentW, accentH, &coverAverage);
    const SDL_Color accent = config.adaptiveAccent ? mixColor(baseAccent, coverAverage, 0.36f) : baseAccent;

    const int coverX = CONTENT_X, coverY = HEADER_H + 20, coverW = 254, coverH = 354;
    drawCoverCard(game, {coverX, coverY, coverW, coverH}, false, config, 1.0f);

    const int x = coverX + coverW + 30;
    const int right = W - CONTENT_R;
    text(fonts_.tiny, game.completed ? "✓ COMPLETED" : (game.favorite ? "★ FAVORITE" : "GAME PROFILE"), x, coverY,
         game.completed ? rgba(72,232,143) : (game.favorite ? rgba(255,218,75) : accent), 300);
    TTF_Font* titleFont = game.title.size() > 42 ? fonts_.large : fonts_.hero;
    text(titleFont, game.title, x, coverY + 24, rgba(255,255,255), right - x - 15);

    std::vector<std::string> meta;
    if (!game.version.empty()) meta.push_back("v" + game.version);
    if (!game.genres.empty()) meta.push_back(game.genres.front());
    if (!game.author.empty()) meta.push_back(game.author);
    if (game.releaseYear) meta.push_back(std::to_string(game.releaseYear));
    if (meta.empty()) meta.push_back(game.baseGame.empty() ? "Game Boy Advance" : "Based on " + game.baseGame);
    int metaX = x;
    for (std::size_t i = 0; i < meta.size() && i < 4; ++i) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(fonts_.tiny, meta[i].c_str(), &tw, &th);
        const int cw = std::min(180, tw + 24);
        if (metaX + cw > right) break;
        roundedFill({metaX, coverY + 84, cw, 26}, 12, rgba(accent.r,accent.g,accent.b,26));
        roundedOutline({metaX, coverY + 84, cw, 26}, 12, rgba(accent.r,accent.g,accent.b,78), 1);
        text(fonts_.tiny, meta[i], metaX + 12, coverY + 88, rgba(211,211,221), cw - 20);
        metaX += cw + 8;
    }

    roundedFill({x, coverY + 119, right - x, 126}, 19, rgba(6,6,10,224));
    roundedOutline({x, coverY + 119, right - x, 126}, 19, rgba(53,53,65), 1);
    text(fonts_.tiny, "ABOUT THIS GAME", x + 18, coverY + 135, accent);
    const std::string description = game.description.empty()
        ? "This game is ready to launch. You can optionally add metadata and screenshots later to turn this page into a richer showcase."
        : game.description;
    wrappedText(fonts_.small, description, {x + 18, coverY + 161, right - x - 36, 71}, rgba(211,211,220), 4);

    const int statsY = coverY + 260;
    const int statW = (right - x - 20) / 3;
    const std::array<std::pair<std::string,std::string>,3> stats = {{
        {formatPlayTime(game.playSeconds), "PLAY TIME"},
        {std::to_string(game.launches), "LAUNCHES"},
        {game.lastPlayed > 0 ? formatTimestamp(game.lastPlayed) : "Never", "LAST PLAYED"}
    }};
    for (int i = 0; i < 3; ++i) {
        const int sx = x + i * (statW + 10);
        roundedFill({sx, statsY, statW, 88}, 17, rgba(9,9,14,228));
        roundedOutline({sx, statsY, statW, 88}, 17, rgba(48,48,60), 1);
        text(fonts_.body, stats[i].first, sx + 15, statsY + 17, rgba(255,255,255), statW - 30);
        text(fonts_.tiny, stats[i].second, sx + 15, statsY + 55, rgba(128,128,141), statW - 30);
    }

    const int screenshotsY = coverY + coverH + 18;
    text(fonts_.tiny, "MEDIA", coverX, screenshotsY, accent);
    int sx = coverX;
    int shown = 0;
    for (const auto& path : game.screenshotPaths) {
