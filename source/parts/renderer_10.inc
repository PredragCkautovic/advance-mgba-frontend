    roundedFill({x, y, badgeW, 21}, 10, rgba(240,240,246));
    text(fonts_.tiny, button, x + badgeW / 2, y + 1, rgba(12,12,15), 0, true);
    x += badgeW + 6;
    text(fonts_.tiny, label, x, y + 1, rgba(191,191,201));
    int tw = 0, th = 0; TTF_SizeUTF8(fonts_.tiny, label.c_str(), &tw, &th);
    x += tw + 15;
}

void Renderer::drawFooter(const std::vector<std::pair<std::string, std::string>>& hints) {
    fill({SIDEBAR_W, H - FOOTER_H, W - SIDEBAR_W, FOOTER_H}, rgba(4,4,8,246));
    line(SIDEBAR_W, H - FOOTER_H, W, H - FOOTER_H, rgba(43,43,53));
    int x = CONTENT_X;
    const int y = H - FOOTER_H + 9;
    for (const auto& hint : hints) drawButtonHint(x, y, hint.first, hint.second);
}

void Renderer::markFocus(const Game* game) {
    const std::string path = game ? game->path : std::string{};
    if (path != lastFocusPath_) {
        lastFocusPath_ = path;
        selectionChangedAt_ = SDL_GetTicks64();
    }
}

void Renderer::drawPlaceholderCover(const Game& game, SDL_Rect rect, const Config& config) {
    const SDL_Color accent = accentColor(config);
    roundedFill(rect, 10, rgba(7,7,11,245));
    roundedOutline(rect, 10, rgba(55,55,66), 1);
    for (int i = 0; i < 5; ++i) {
        const int inset = 12 + i * 10;
        if (rect.w - inset * 2 <= 0 || rect.h - inset * 2 <= 0) break;
        roundedOutline({rect.x + inset, rect.y + inset, rect.w - inset * 2, rect.h - inset * 2}, 16,
                       rgba(accent.r, accent.g, accent.b, static_cast<Uint8>(55 - i * 8)), 1);
    }
    const int logoW = std::min(rect.w - 24, 92);
    const int logoH = std::min(rect.h / 2, 62);
    drawBrandHandheld(rect.x + rect.w / 2 - logoW / 2, rect.y + rect.h / 2 - logoH / 2 - 18,
                      logoW, logoH, accent, 235);
    text(fonts_.tiny, "ARTWORK MISSING", rect.x + rect.w / 2, rect.y + rect.h / 2 + 34,
         rgba(160,160,173), rect.w - 18, true);
    if (rect.h > 150)
        text(fonts_.tiny, game.shortTitle.empty() ? game.title : game.shortTitle, rect.x + rect.w / 2,
             rect.y + rect.h / 2 + 56, rgba(112,112,126), rect.w - 24, true);
}

void Renderer::drawCoverCard(const Game& game, SDL_Rect rect, bool selected, const Config& config,
                             float focusProgress, bool compact) {
    const SDL_Color accent = accentColor(config);
    const float eased = easeOutBack(focusProgress);
    const int grow = selected ? (config.motionEnabled ? static_cast<int>((compact ? 4.0f : 6.0f) * eased) : (compact ? 4 : 6)) : 0;
    SDL_Rect card{rect.x - grow, rect.y - grow, rect.w + grow * 2, rect.h + grow * 2};

    int iw = 0, ih = 0;
    SDL_Color avg = accent;
    SDL_Texture* cover = loadCover(game, iw, ih, &avg);
    const SDL_Color cardAccent = config.adaptiveAccent && cover ? mixColor(accent, avg, 0.34f) : accent;

    if (selected) {
        roundedFill({card.x - 13, card.y - 13, card.w + 26, card.h + 26}, 23, rgba(cardAccent.r,cardAccent.g,cardAccent.b,18));
        roundedFill({card.x - 8, card.y - 8, card.w + 16, card.h + 16}, 19, rgba(cardAccent.r,cardAccent.g,cardAccent.b,36));
    }
    roundedFill({card.x + 4, card.y + 7, card.w, card.h}, 12, rgba(0,0,0,195));
    roundedFill(card, 12, rgba(7,7,10,248));

    SDL_Rect art{card.x + 3, card.y + 3, card.w - 6, card.h - 6};
    if (cover && iw > 0 && ih > 0) {
        const float scale = std::min(static_cast<float>(art.w) / iw, static_cast<float>(art.h) / ih);
        const int dw = std::max(1, static_cast<int>(iw * scale));
        const int dh = std::max(1, static_cast<int>(ih * scale));
        SDL_Rect dst{art.x + (art.w - dw) / 2, art.y + (art.h - dh) / 2, dw, dh};
        SDL_RenderCopy(renderer_, cover, nullptr, &dst);

        if (selected && config.motionEnabled) {
            const std::uint64_t elapsed = SDL_GetTicks64() - selectionChangedAt_;
            if (elapsed < SHINE_ANIM_MS) {
                const float t = static_cast<float>(elapsed) / SHINE_ANIM_MS;
                const int shineX = art.x - 34 + static_cast<int>((art.w + 68) * t);
                SDL_RenderSetClipRect(renderer_, &art);
                for (int i = -7; i <= 7; ++i) {
                    const Uint8 a = static_cast<Uint8>(std::max(0, 34 - std::abs(i) * 4));
                    line(shineX + i, art.y, shineX + i + 28, art.y + art.h, rgba(255,255,255,a));
                }
                SDL_RenderSetClipRect(renderer_, nullptr);
            }
        }
    } else drawPlaceholderCover(game, art, config);

    if (config.showCoverLabels && cover) {
        roundedFill({card.x + 5, card.y + card.h - 32, card.w - 10, 27}, 7, rgba(0,0,0,220));
        text(fonts_.tiny, game.shortTitle.empty() ? game.title : game.shortTitle, card.x + 10, card.y + card.h - 29,
             rgba(255,255,255), card.w - 20);
    }

    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
    const bool newlyAdded = game.addedAt > 0 && now >= game.addedAt && (now - game.addedAt) <= 7 * 24 * 60 * 60;
    if (newlyAdded && game.launches == 0) {
        roundedFill({card.x + 7, card.y + card.h - 28, 42, 20}, 9, rgba(cardAccent.r,cardAccent.g,cardAccent.b,225));
