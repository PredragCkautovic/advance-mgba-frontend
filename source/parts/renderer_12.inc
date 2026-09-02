    const char* actionLabel = game->launches > 0 ? "RESUME" : "PLAY";
    const int actionW = game->launches > 0 ? 128 : 108;
    roundedFill({tx, actionY, actionW, 34}, 14, adaptive);
    circle(tx + 19, actionY + 17, 10, rgba(255,255,255), true);
    text(fonts_.tiny, "A", tx + 19, actionY + 9, adaptive, 0, true);
    text(fonts_.tiny, actionLabel, tx + 40, actionY + 8, rgba(255,255,255));

    roundedFill({tx + actionW + 10, actionY, 128, 34}, 14, rgba(26,26,34,240));
    roundedOutline({tx + actionW + 10, actionY, 128, 34}, 14, rgba(78,78,91), 1);
    text(fonts_.tiny, "X  DETAILS", tx + actionW + 30, actionY + 8, rgba(230,230,237));

    // Activity card combines per-game history with library-wide progress.
    std::uint64_t totalPlay = 0;
    std::size_t favorites = 0, completed = 0;
    for (const auto& g : games) {
        totalPlay += g.playSeconds;
        if (g.favorite) ++favorites;
        if (g.completed) ++completed;
    }
    const int statX = x + w - 286;
    roundedFill({statX, y + 18, 258, 166}, 21, rgba(second.r,second.g,second.b,42));
    roundedOutline({statX, y + 18, 258, 166}, 21, rgba(adaptive.r,adaptive.g,adaptive.b,82), 1);
    text(fonts_.tiny, "YOUR ACTIVITY", statX + 18, y + 32, rgba(182,182,194));
    text(fonts_.medium, game->playSeconds > 0 ? formatPlayTime(game->playSeconds) : std::to_string(game->launches) + (game->launches == 1 ? " launch" : " launches"),
         statX + 18, y + 55, rgba(255,255,255), 220);
    text(fonts_.tiny, game->playSeconds > 0 ? "THIS GAME" : "THIS GAME · PLAY TIME STARTS ON RETURN", statX + 18, y + 90, rgba(122,122,137), 220);
    line(statX + 18, y + 112, statX + 240, y + 112, rgba(70,70,82));
    text(fonts_.tiny, "★ " + std::to_string(favorites) + " FAVORITES", statX + 18, y + 124, rgba(255,218,79), 110);
    text(fonts_.tiny, "✓ " + std::to_string(completed) + " DONE", statX + 137, y + 124, rgba(82,230,147), 103);
    text(fonts_.tiny, "TOTAL " + formatPlayTime(totalPlay), statX + 18, y + 148, rgba(142,142,155), 220);
}

void Renderer::drawHomeShelf(const std::vector<Game>& games, const HomeRow& row, std::size_t rowIndex,
                             std::size_t selectedRow, std::size_t selectedColumn, int y, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const bool active = rowIndex == selectedRow;
    if (active) roundedFill({CONTENT_X, y - 1, 6, 25}, 3, accent);
    text(fonts_.small, row.title, CONTENT_X + (active ? 14 : 2), y, active ? rgba(255,255,255) : rgba(196,196,206), 300);
    text(fonts_.tiny, row.subtitle, CONTENT_X + 270, y + 3, active ? rgba(153,153,168) : rgba(114,114,128), 600);

    constexpr int cardW = 132, cardH = 138, gap = 13, visibleCards = 8;
    std::size_t start = 0;
    if (active && selectedColumn >= visibleCards) start = selectedColumn - visibleCards + 1;
    if (row.games.size() > visibleCards && start + visibleCards > row.games.size()) start = row.games.size() - visibleCards;
    const int cardY = y + 32;
    for (int slot = 0; slot < visibleCards; ++slot) {
        const std::size_t c = start + static_cast<std::size_t>(slot);
        if (c >= row.games.size()) break;
        const std::size_t gi = row.games[c];
        if (gi >= games.size()) continue;
        const bool selected = active && c == selectedColumn;
        if (selected) markFocus(&games[gi]);
        const float t = selected ? (SDL_GetTicks64() - selectionChangedAt_) / static_cast<float>(FOCUS_ANIM_MS) : 1.0f;
        drawCoverCard(games[gi], {CONTENT_X + slot * (cardW + gap), cardY, cardW, cardH}, selected, config, t, true);
    }
    if (row.games.size() > visibleCards) {
        std::ostringstream count; count << (active ? selectedColumn + 1 : 1) << " / " << row.games.size();
        textRight(fonts_.tiny, count.str(), W - CONTENT_R, y + 2, rgba(120,120,132));
    }
}

void Renderer::renderHome(const std::vector<Game>& games,
                          const std::vector<HomeRow>& rows,
                          std::size_t selectedRow,
                          std::size_t selectedColumn,
                          const Config& config,
                          const std::string& toast) {
    const Game* selected = nullptr;
    if (!rows.empty() && selectedRow < rows.size() && !rows[selectedRow].games.empty()) {
        selectedColumn = std::min(selectedColumn, rows[selectedRow].games.size() - 1);
        const std::size_t idx = rows[selectedRow].games[selectedColumn];
        if (idx < games.size()) selected = &games[idx];
    }
    drawDynamicBackdrop(selected, config, 84);
    drawSidebar(Screen::Home, Shelf::All, config);
    std::string greeting = "Welcome back";
    if (const std::time_t now = std::time(nullptr); now > 0) {
        if (std::tm* tm = std::localtime(&now)) {
            if (tm->tm_hour >= 5 && tm->tm_hour < 12) greeting = "Good morning";
            else if (tm->tm_hour >= 12 && tm->tm_hour < 18) greeting = "Good afternoon";
            else if (tm->tm_hour >= 18 && tm->tm_hour < 23) greeting = "Good evening";
        }
    }
    drawTopChrome("ADVANCE  /  HOME", greeting + ", " + nickname_, "Your personal GBA universe", config);
    drawHero(selected, games, config);

