        if (shown >= 4) break;
        int iw = 0, ih = 0;
        SDL_Texture* image = loadImage(path, iw, ih, nullptr);
        if (!image || iw <= 0 || ih <= 0) continue;
        SDL_Rect box{sx, screenshotsY + 25, 200, 102};
        roundedFill(box, 12, rgba(7,7,11));
        const float scale = std::max(static_cast<float>(box.w) / iw, static_cast<float>(box.h) / ih);
        const int dw = static_cast<int>(iw * scale), dh = static_cast<int>(ih * scale);
        SDL_Rect dst{box.x + (box.w - dw) / 2, box.y + (box.h - dh) / 2, dw, dh};
        SDL_RenderSetClipRect(renderer_, &box);
        SDL_RenderCopy(renderer_, image, nullptr, &dst);
        SDL_RenderSetClipRect(renderer_, nullptr);
        roundedOutline(box, 12, rgba(62,62,73), 1);
        sx += 214; ++shown;
    }
    if (shown == 0) {
        roundedFill({coverX, screenshotsY + 25, 414, 102}, 14, rgba(7,7,11,220));
        roundedOutline({coverX, screenshotsY + 25, 414, 102}, 14, rgba(47,47,58), 1);
        text(fonts_.small, "Add screenshots to make this page yours", coverX + 18, screenshotsY + 47, rgba(205,205,215), 380);
        text(fonts_.tiny, "screenshot1.png · screenshot2.png · or list them in advance.json", coverX + 18, screenshotsY + 74, rgba(124,124,138), 380);
    }

    // Quick actions feel like an action palette rather than plain text status.
    const int actionX = right - 300;
    roundedFill({actionX, screenshotsY + 25, 300, 102}, 16, rgba(accent.r,accent.g,accent.b,23));
    roundedOutline({actionX, screenshotsY + 25, 300, 102}, 16, rgba(accent.r,accent.g,accent.b,90), 1);
    const std::array<std::tuple<const char*, std::string, SDL_Color>,3> actions = {{
        {"Y", game.favorite ? "Favorited" : "Favorite", game.favorite ? rgba(255,220,82) : rgba(235,235,242)},
        {"ZR", game.completed ? "Completed" : "Mark complete", game.completed ? rgba(73,232,142) : rgba(190,190,201)},
        {"ZL", game.hidden ? "Hidden" : "Hide game", game.hidden ? rgba(255,137,90) : rgba(160,160,174)}
    }};
    for (int i = 0; i < 3; ++i) {
        const int yy = screenshotsY + 34 + i * 27;
        roundedFill({actionX + 16, yy, i == 0 ? 24 : 30, 21}, 10, rgba(238,238,244));
        text(fonts_.tiny, std::get<0>(actions[i]), actionX + (i == 0 ? 28 : 31), yy + 1, rgba(9,9,12), 0, true);
        text(fonts_.tiny, std::get<1>(actions[i]), actionX + 56, yy + 2, std::get<2>(actions[i]), 210);
    }

    drawFooter({{"A","Play"},{"Y","Favorite"},{"X","Edit title"},{"R","Collections"},{"ZR","Complete"},{"ZL","Hide"},{"−","Auto title"},{"B","Back"}});
    drawScreenTransition(Screen::Details, config);
    drawSidebarOverlay(Screen::Details, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderCollectionPicker(const Game& game,
                                      const std::vector<CustomCollection>& collections,
                                      std::size_t selected,
                                      const Config& config,
                                      const std::string& toast) {
    drawDynamicBackdrop(&game, config, 66);
    drawSidebar(Screen::Details, Shelf::All, config);
    drawTopChrome("ADVANCE  /  COLLECTIONS", "Add to collection", game.title, config, true);
    const SDL_Color accent = accentColor(config);

    const int panelX = 260, panelY = 118, panelW = 760, panelH = 500;
    roundedFill({panelX, panelY, panelW, panelH}, 28, rgba(5,5,9,244));
    roundedOutline({panelX, panelY, panelW, panelH}, 28, rgba(63,63,76), 1);
    text(fonts_.tiny, "PERSONAL SHELVES", panelX + 28, panelY + 24, accent);
    text(fonts_.large, "Where should this game live?", panelX + 28, panelY + 47, rgba(255,255,255), panelW - 56);
    text(fonts_.small, "A toggles membership. X creates another collection.", panelX + 28, panelY + 95, rgba(145,145,158));

    if (collections.empty()) {
        roundedFill({panelX + 28, panelY + 150, panelW - 56, 180}, 20, rgba(11,11,16));
        text(fonts_.medium, "No custom collections yet", panelX + 52, panelY + 189, rgba(255,255,255));
        wrappedText(fonts_.small, "Press X to create your first shelf. You can use collections for must-play games, favorite hacks, testing builds, or anything else.",
                    {panelX + 52, panelY + 235, panelW - 104, 75}, rgba(157,157,170), 5);
    } else {
        const std::size_t maxRows = 7;
        const std::size_t start = selected >= maxRows ? selected - maxRows + 1 : 0;
        for (std::size_t slot = 0; slot < maxRows; ++slot) {
            const std::size_t i = start + slot;
            if (i >= collections.size()) break;
            const bool on = i == selected;
            const bool member = collectionContains(collections[i], game.path);
            const int y = panelY + 145 + static_cast<int>(slot) * 47;
            roundedFill({panelX + 28, y, panelW - 56, 39}, 13, on ? rgba(accent.r,accent.g,accent.b,29) : rgba(12,12,17));
            if (on) roundedOutline({panelX + 28, y, panelW - 56, 39}, 13, accent, 1);
            circle(panelX + 52, y + 20, 10, member ? accent : rgba(62,62,74), true);
            if (member) text(fonts_.tiny, "✓", panelX + 52, y + 12, rgba(255,255,255), 0, true);
