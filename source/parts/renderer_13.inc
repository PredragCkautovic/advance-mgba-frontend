    if (!rows.empty()) {
        const std::size_t r0 = std::min(selectedRow, rows.size() - 1);
        drawHomeShelf(games, rows[r0], r0, selectedRow, selectedColumn, HEADER_H + 242, config);
        std::size_t r1 = r0 + 1 < rows.size() ? r0 + 1 : (r0 > 0 ? r0 - 1 : r0);
        if (r1 != r0) drawHomeShelf(games, rows[r1], r1, selectedRow, 0, HEADER_H + 424, config);
    }
    drawFooter({{"A","Play"},{"X","Details"},{"Y","Favorite"},{"R","Surprise Me"},{"L","Library"},{"−","Search"},{"+","Settings"}});
    drawScreenTransition(Screen::Home, config);
    drawSidebarOverlay(Screen::Home, Shelf::All, config);
    drawToast(toast);
}

std::string Renderer::shelfName(Shelf shelf, const std::string& collectionName) const {
    switch (shelf) {
        case Shelf::Favorites: return "Favorites";
        case Shelf::Recent: return "Recently Played";
        case Shelf::Search: return collectionName.empty() ? "Search Results" : collectionName;
        case Shelf::Collection: return collectionName.empty() ? "Collection" : collectionName;
        case Shelf::Completed: return "Completed";
        case Shelf::Hidden: return "Hidden";
        default: return "Game Boy Advance";
    }
}

std::string Renderer::sortName(SortMode mode) const {
    switch (mode) {
        case SortMode::Recent: return "Recent";
        case SortMode::Launches: return "Most launches";
        case SortMode::PlayTime: return "Play time";
        case SortMode::Added: return "Recently added";
        case SortMode::Favorites: return "Favorites first";
        default: return "A–Z";
    }
}

void Renderer::drawSelectedStrip(const Game* game, std::size_t selected, std::size_t count,
                                 std::size_t perPage, Shelf shelf, const Config& config) {
    const GalleryLayout l = galleryLayout(config);
    const SDL_Color accent = accentColor(config);
    SDL_Color adaptive = accent;
    if (game) {
        int iw = 0, ih = 0;
        SDL_Color avg = accent;
        loadCover(*game, iw, ih, &avg);
        adaptive = config.adaptiveAccent ? mixColor(accent, avg, 0.34f) : accent;
    }
    const int y = H - FOOTER_H - 82;
    roundedFill({l.x + 4, y + 5, l.w, 72}, 18, rgba(0,0,0,150));
    roundedFill({l.x, y, l.w, 72}, 18, rgba(6,6,10,240));
    roundedOutline({l.x, y, l.w, 72}, 18, rgba(51,51,63), 1);
    roundedFill({l.x, y, 6, 72}, 3, adaptive);

    if (!game) {
        const std::string empty = shelf == Shelf::Favorites ? "Nothing favorited yet" :
                                  shelf == Shelf::Search ? "No games match that search" : "No games found";
        text(fonts_.body, empty, l.x + 22, y + 12, rgba(255,255,255));
        text(fonts_.tiny, "Try another shelf, search, or rebuild the library in Settings.", l.x + 22, y + 43, rgba(139,139,151));
        return;
    }

    TTF_Font* titleFont = game->title.size() > 42 ? fonts_.body : fonts_.medium;
    text(titleFont, game->title, l.x + 22, y + 6, rgba(255,255,255), 650);
    std::ostringstream meta;
    if (game->favorite) meta << "★ Favorite"; else meta << "☆ Add favorite";
    meta << "   ·   " << formatPlayTime(game->playSeconds)
         << "   ·   " << game->launches << (game->launches == 1 ? " launch" : " launches");
    if (game->completed) meta << "   ·   ✓ Completed";
    text(fonts_.tiny, meta.str(), l.x + 22, y + 43, rgba(151,151,164), 770);

    const int playX = l.x + l.w - 142;
    roundedFill({playX, y + 13, 122, 43}, 17, adaptive);
    circle(playX + 23, y + 34, 11, rgba(255,255,255), true);
    text(fonts_.tiny, "A", playX + 23, y + 26, adaptive, 0, true);
    text(fonts_.small, game->launches > 0 ? "RESUME" : "PLAY", playX + 47, y + 23, rgba(255,255,255));

    if (count > 0 && perPage > 0) {
        const std::size_t pages = (count + perPage - 1) / perPage;
        const std::size_t page = selected / perPage;
        textRight(fonts_.tiny, "PAGE " + std::to_string(page + 1) + " / " + std::to_string(pages), playX - 22, y + 12, rgba(157,157,170));
        const int dots = static_cast<int>(std::min<std::size_t>(pages, 7));
        const int startX = playX - 22 - dots * 11;
        for (int i = 0; i < dots; ++i) {
            const std::size_t represented = pages <= 7 ? static_cast<std::size_t>(i)
                : std::min(pages - 1, page > 5 ? page - 5 + i : static_cast<std::size_t>(i));
            circle(startX + i * 11, y + 47, represented == page ? 4 : 3,
                   represented == page ? adaptive : rgba(67,67,78), true);
        }
    }
}

void Renderer::renderLibrary(const std::vector<Game>& games,
                             const std::vector<std::size_t>& visible,
                             std::size_t selected,
                             Shelf shelf,
                             SortMode sortMode,
                             const std::string& collectionName,
                             const Config& config,
                             const std::string& toast) {
    const Game* selectedGame = nullptr;
    if (!visible.empty() && selected < visible.size() && visible[selected] < games.size()) selectedGame = &games[visible[selected]];
    markFocus(selectedGame);
