        text(fonts_.tiny, "NEW", card.x + 28, card.y + card.h - 27, rgba(255,255,255), 0, true);
    }
    if (game.favorite) {
        roundedFill({card.x + card.w - 31, card.y + 7, 24, 24}, 12, rgba(3,3,6,225));
        text(fonts_.tiny, "★", card.x + card.w - 19, card.y + 9, rgba(255,219,72), 0, true);
    }
    if (game.completed) {
        roundedFill({card.x + 7, card.y + 7, 26, 22}, 10, rgba(3,3,6,225));
        text(fonts_.tiny, "✓", card.x + 20, card.y + 8, rgba(80,234,147), 0, true);
    }
    if (selected) {
        roundedOutline(card, 12, rgba(255,255,255,238), 2);
        roundedOutline({card.x - 3, card.y - 3, card.w + 6, card.h + 6}, 15, cardAccent, 2);
        roundedFill({card.x + 11, card.y + card.h - 7, card.w - 22, 4}, 2, cardAccent);
    } else roundedOutline(card, 12, rgba(46,46,56), 1);
}

std::string Renderer::formatPlayTime(std::uint64_t seconds) const {
    if (seconds < 60) return seconds == 0 ? "Not tracked yet" : "< 1 min";
    const std::uint64_t hours = seconds / 3600;
    const std::uint64_t minutes = (seconds % 3600) / 60;
    std::ostringstream ss;
    if (hours) ss << hours << "h ";
    ss << minutes << "m";
    return ss.str();
}

std::string Renderer::formatTimestamp(std::int64_t ts) const {
    if (ts <= 0) return "Not played yet";
    std::time_t t = static_cast<std::time_t>(ts);
    std::tm* tm = std::localtime(&t);
    if (!tm) return "Unknown";
    char buf[64]; std::strftime(buf, sizeof(buf), "%d %b %Y · %H:%M", tm); return buf;
}

std::string Renderer::join(const std::vector<std::string>& values, const std::string& separator, std::size_t max) const {
    std::ostringstream ss;
    const std::size_t n = std::min(max, values.size());
    for (std::size_t i = 0; i < n; ++i) { if (i) ss << separator; ss << values[i]; }
    return ss.str();
}

void Renderer::drawHero(const Game* game, const std::vector<Game>& games, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const int x = CONTENT_X, y = HEADER_H + 8, w = W - CONTENT_X - CONTENT_R, h = 230;

    roundedFill({x + 6, y + 10, w, h}, 26, rgba(0,0,0,170));
    roundedFill({x, y, w, h}, 26, rgba(5,5,9,228));
    roundedOutline({x, y, w, h}, 26, rgba(60,60,74), 1);

    if (!game) {
        roundedFill({x, y, 7, h}, 4, accent);
        text(fonts_.hero, "Your GBA universe is ready.", x + 32, y + 48, rgba(255,255,255), 760);
        text(fonts_.body, "Add games to your ROM folder and Advance will build a polished library with artwork, history, collections, and personalized shelves.",
             x + 32, y + 112, rgba(165,165,178), 860);
        return;
    }

    markFocus(game);
    int iw = 0, ih = 0;
    SDL_Color coverAvg = accent;
    loadCover(*game, iw, ih, &coverAvg);
    const SDL_Color adaptive = config.adaptiveAccent ? mixColor(accent, coverAvg, 0.38f) : accent;
    roundedFill({x, y, 7, h}, 4, adaptive);
    roundedFill({x + 7, y, 240, 3}, 1, rgba(adaptive.r, adaptive.g, adaptive.b, 210));

    // Cover pedestal.
    roundedFill({x + 17, y + 12, 164, h - 24}, 20, rgba(adaptive.r,adaptive.g,adaptive.b,18));
    SDL_Rect coverRect{x + 25, y + 16, 148, h - 32};
    drawCoverCard(*game, coverRect, false, config, 1.0f, true);

    const int tx = x + 198;
    text(fonts_.tiny, game->completed ? "COMPLETED · SPOTLIGHT" : (game->launches > 0 ? "CONTINUE PLAYING" : "FEATURED NOW"), tx, y + 17,
         game->completed ? rgba(70,232,143) : adaptive, 520);
    TTF_Font* titleFont = game->title.size() > 45 ? fonts_.medium : (game->title.size() > 29 ? fonts_.large : fonts_.hero);
    text(titleFont, game->title, tx, y + 37, rgba(255,255,255), 660);

    // Metadata chips: faster to read than one long debug-looking line.
    std::vector<std::string> chips;
    if (!game->version.empty()) chips.push_back("v" + game->version);
    if (!game->genres.empty()) chips.push_back(game->genres.front());
    if (!game->author.empty()) chips.push_back(game->author);
    if (game->releaseYear > 0) chips.push_back(std::to_string(game->releaseYear));
    if (chips.empty()) chips.push_back(game->baseGame.empty() ? "Game Boy Advance" : "Based on " + game->baseGame);
    int chipX = tx;
    const int chipY = y + 94;
    for (std::size_t i = 0; i < chips.size() && i < 3; ++i) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(fonts_.tiny, chips[i].c_str(), &tw, &th);
        const int cw = std::min(175, tw + 24);
        if (chipX + cw > x + w - 305) break;
        roundedFill({chipX, chipY, cw, 25}, 12, rgba(adaptive.r,adaptive.g,adaptive.b,24));
        roundedOutline({chipX, chipY, cw, 25}, 12, rgba(adaptive.r,adaptive.g,adaptive.b,75), 1);
        text(fonts_.tiny, chips[i], chipX + 12, chipY + 4, rgba(210,210,221), cw - 20);
        chipX += cw + 8;
    }

    const std::string description = game->description.empty()
        ? "Ready when you are. Open Details to personalize metadata, screenshots, collections, and completion state."
        : game->description;
    wrappedText(fonts_.tiny, description, {tx, y + 128, 645, 46}, rgba(149,149,163), 3);

    const int actionY = y + 187;
