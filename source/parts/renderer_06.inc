            y += lineH + lineGap;
        }
        lineText = word;
    }
    if (!lineText.empty() && y + lineH <= rect.y + rect.h) text(font, lineText, rect.x, y, color, rect.w);
}

SDL_Texture* Renderer::loadImage(const std::string& path, int& w, int& h, SDL_Color* average) {
    w = h = 0;
    if (path.empty() || failedCoverCache_.count(path)) return nullptr;
    if (auto found = coverCache_.find(path); found != coverCache_.end()) {
        found->second.lastUse = frame_;
        w = found->second.w; h = found->second.h;
        if (average) *average = found->second.average;
        return found->second.texture;
    }

    SDL_Surface* source = IMG_Load(path.c_str());
    if (!source) { failedCoverCache_.insert(path); return nullptr; }

    SDL_Color avg = rgba(28, 28, 34);
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
    if (rgbaSurface) {
        if (SDL_MUSTLOCK(rgbaSurface)) SDL_LockSurface(rgbaSurface);
        auto* px = static_cast<Uint32*>(rgbaSurface->pixels);
        std::uint64_t rr = 0, gg = 0, bb = 0, count = 0;
        const int sx = std::max(1, rgbaSurface->w / 18);
        const int sy = std::max(1, rgbaSurface->h / 18);
        for (int y = sy / 2; y < rgbaSurface->h; y += sy) for (int x = sx / 2; x < rgbaSurface->w; x += sx) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(px[y * (rgbaSurface->pitch / 4) + x], rgbaSurface->format, &r, &g, &b, &a);
            if (a < 80) continue;
            rr += r; gg += g; bb += b; ++count;
        }
        if (SDL_MUSTLOCK(rgbaSurface)) SDL_UnlockSurface(rgbaSurface);
        if (count) avg = rgba(static_cast<Uint8>(rr / count), static_cast<Uint8>(gg / count), static_cast<Uint8>(bb / count));
        SDL_FreeSurface(rgbaSurface);
    }

    constexpr int maxW = 760, maxH = 720;
    SDL_Surface* surface = source;
    SDL_Surface* scaled = nullptr;
    if (source->w > maxW || source->h > maxH) {
        const float scale = std::min(static_cast<float>(maxW) / source->w, static_cast<float>(maxH) / source->h);
        const int dw = std::max(1, static_cast<int>(source->w * scale));
        const int dh = std::max(1, static_cast<int>(source->h * scale));
        scaled = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32, SDL_PIXELFORMAT_RGBA32);
        if (scaled) {
            SDL_Rect dst{0, 0, dw, dh};
            if (SDL_BlitScaled(source, nullptr, scaled, &dst) == 0) surface = scaled;
        }
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    w = surface->w; h = surface->h;
    if (scaled) SDL_FreeSurface(scaled);
    SDL_FreeSurface(source);
    if (!texture) { failedCoverCache_.insert(path); return nullptr; }
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    coverCache_[path] = TextureEntry{texture, w, h, avg, frame_};
    if (average) *average = avg;
    return texture;
}

SDL_Texture* Renderer::loadCover(const Game& game, int& w, int& h, SDL_Color* average) {
    return loadImage(game.coverPath, w, h, average);
}

void Renderer::pruneCoverCache() {
    if (coverCache_.size() <= 20) return;
    std::vector<std::pair<std::string, std::uint64_t>> ages;
    for (const auto& [path, entry] : coverCache_) ages.emplace_back(path, entry.lastUse);
    std::sort(ages.begin(), ages.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    while (coverCache_.size() > 14 && !ages.empty()) {
        auto it = coverCache_.find(ages.front().first);
        if (it != coverCache_.end()) { SDL_DestroyTexture(it->second.texture); coverCache_.erase(it); }
        ages.erase(ages.begin());
    }
}

void Renderer::pruneTextCache() {
    if (textCache_.size() <= 220) return;
    std::vector<std::pair<std::string, std::uint64_t>> ages;
    for (const auto& [key, entry] : textCache_) ages.emplace_back(key, entry.lastUse);
    std::sort(ages.begin(), ages.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    const std::size_t remove = textCache_.size() - 160;
    for (std::size_t i = 0; i < remove; ++i) {
        auto it = textCache_.find(ages[i].first);
        if (it != textCache_.end()) { SDL_DestroyTexture(it->second.texture); textCache_.erase(it); }
    }
}

void Renderer::drawBaseBackdrop(const Config& config) {
    fill({0, 0, W, H}, rgba(0, 0, 0));
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    // Very dark layered glows keep OLED black while giving the app a distinct identity.
    for (int i = 0; i < 8; ++i) {
        const int pad = i * 38;
        roundedFill({W - 430 - pad / 2, -150 + pad / 3, 520 + pad, 300 + pad}, 110,
                    rgba(accent.r, accent.g, accent.b, static_cast<Uint8>(11 - i)));
        roundedFill({SIDEBAR_W - 160 - pad / 2, H - 210 - pad / 4, 390 + pad, 300 + pad / 2}, 100,
                    rgba(second.r, second.g, second.b, static_cast<Uint8>(8 - std::min(i, 7))));
    }
}

void Renderer::drawDynamicBackdrop(const Game* game, const Config& config, int strength) {
    drawBaseBackdrop(config);
    if (!game || !config.dynamicBackdrop || (game->coverPath.empty() && game->bannerPath.empty())) return;
