    drawDynamicBackdrop(selectedGame, config, 48);
    drawSidebar(Screen::Library, shelf, config);
    const std::string title = shelfName(shelf, collectionName);
    std::ostringstream subtitle;
    subtitle << visible.size() << (visible.size() == 1 ? " game" : " games") << "  ·  " << sortName(sortMode);
    drawTopChrome("ADVANCE  /  LIBRARY", title, subtitle.str(), config);

    const GalleryLayout l = galleryLayout(config);
    const std::size_t page = l.perPage ? selected / l.perPage : 0;
    const std::size_t start = page * l.perPage;
    for (std::size_t slot = 0; slot < l.perPage; ++slot) {
        const std::size_t vi = start + slot;
        if (vi >= visible.size()) break;
        const std::size_t gi = visible[vi];
        if (gi >= games.size()) continue;
        const int row = static_cast<int>(slot / l.cols);
        const int col = static_cast<int>(slot % l.cols);
        SDL_Rect card{l.x + col * (l.cardW + l.gap), l.y + row * (l.cardH + l.gap), l.cardW, l.cardH};
        const bool isSelected = vi == selected;
        const float progress = isSelected ? (SDL_GetTicks64() - selectionChangedAt_) / static_cast<float>(FOCUS_ANIM_MS) : 1.0f;
        drawCoverCard(games[gi], card, isSelected, config, progress);
    }

    drawSelectedStrip(selectedGame, selected, visible.size(), l.perPage, shelf, config);
    drawFooter({{"−","Search"},{"Y","Sort"},{"X","Details"},{"A","Play"},{"L","Favorites"},{"R","Recent"},{"ZL/ZR","Page"},{"B","Home"}});
    drawScreenTransition(Screen::Library, config);
    drawSidebarOverlay(Screen::Library, shelf, config);
    drawToast(toast);
}

void Renderer::drawCollectionCard(const std::vector<Game>& games, const Collection& collection, SDL_Rect rect,
                                  bool selected, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const int grow = selected ? 4 : 0;
    rect = {rect.x - grow, rect.y - grow, rect.w + grow * 2, rect.h + grow * 2};
    if (selected) roundedFill({rect.x - 8, rect.y - 8, rect.w + 16, rect.h + 16}, 24, rgba(accent.r,accent.g,accent.b,26));
    roundedFill({rect.x + 4, rect.y + 7, rect.w, rect.h}, 19, rgba(0,0,0,180));
    roundedFill(rect, 19, rgba(8,8,13,246));
    roundedOutline(rect, 19, selected ? accent : rgba(52,52,63), selected ? 2 : 1);

    const int collageH = 118;
    SDL_Rect clip{rect.x + 3, rect.y + 3, rect.w - 6, collageH};
    roundedFill(clip, 16, rgba(14,14,20));
    const int cellW = clip.w / 4;
    for (int i = 0; i < 4; ++i) {
        if (static_cast<std::size_t>(i) >= collection.games.size()) break;
        const std::size_t gi = collection.games[static_cast<std::size_t>(i)];
        if (gi >= games.size()) continue;
        int iw = 0, ih = 0;
        SDL_Texture* t = loadCover(games[gi], iw, ih, nullptr);
        if (!t || iw <= 0 || ih <= 0) continue;
        SDL_Rect cell{clip.x + i * cellW, clip.y, cellW, clip.h};
        const float scale = std::max(static_cast<float>(cell.w) / iw, static_cast<float>(cell.h) / ih);
        const int dw = static_cast<int>(iw * scale), dh = static_cast<int>(ih * scale);
        SDL_Rect dst{cell.x + (cell.w - dw) / 2, cell.y + (cell.h - dh) / 2, dw, dh};
        SDL_RenderSetClipRect(renderer_, &cell);
        SDL_RenderCopy(renderer_, t, nullptr, &dst);
        SDL_RenderSetClipRect(renderer_, nullptr);
    }
    fill({clip.x, clip.y + clip.h - 35, clip.w, 35}, rgba(0,0,0,120));

    text(fonts_.body, collection.name, rect.x + 18, rect.y + 134, rgba(255,255,255), rect.w - 36);
    text(fonts_.tiny, collection.description, rect.x + 18, rect.y + 168, rgba(139,139,152), rect.w - 36);
    text(fonts_.tiny, std::to_string(collection.games.size()) + (collection.games.size() == 1 ? " game" : " games"),
         rect.x + 18, rect.y + rect.h - 27, selected ? accent : rgba(177,177,188));
    if (!collection.automatic) {
        roundedFill({rect.x + rect.w - 78, rect.y + rect.h - 31, 59, 20}, 9, rgba(accent.r,accent.g,accent.b,30));
        text(fonts_.tiny, "CUSTOM", rect.x + rect.w - 48, rect.y + rect.h - 29, accent, 0, true);
    }
}

void Renderer::renderCollections(const std::vector<Game>& games,
                                 const std::vector<Collection>& collections,
                                 std::size_t selected,
                                 const Config& config,
                                 const std::string& toast) {
    drawBaseBackdrop(config);
    drawSidebar(Screen::Collections, Shelf::All, config);
    drawTopChrome("ADVANCE  /  COLLECTIONS", "Your shelves", "Automatic categories + your own collections", config);
    const SDL_Color accent = accentColor(config);

    if (collections.empty()) {
        roundedFill({CONTENT_X, 130, W - CONTENT_X - CONTENT_R, 380}, 28, rgba(7,7,11,235));
        text(fonts_.hero, "Build a collection that feels like yours.", CONTENT_X + 40, 190, rgba(255,255,255), 850);
        wrappedText(fonts_.body, "Press X to create a custom collection. Advance also creates smart shelves automatically as it learns your library.",
                    {CONTENT_X + 40, 260, 760, 100}, rgba(160,160,174), 6);
        roundedFill({CONTENT_X + 40, 390, 180, 44}, 17, accent);
