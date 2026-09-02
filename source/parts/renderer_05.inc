void Renderer::drawBrandHandheld(int x, int y, int w, int h, SDL_Color accent, Uint8 alpha) {
    const SDL_Color shell = mixColor(rgba(101, 63, 232), accent, 0.22f, alpha);
    const SDL_Color shellDark = rgba(35, 25, 84, alpha);
    const SDL_Color ink = rgba(10, 12, 39, alpha);
    const SDL_Color screen = rgba(45, 208, 221, alpha);
    roundedFill({x, y, w, h}, std::max(8, h / 4), shellDark);
    roundedFill({x + 3, y + 3, w - 6, h - 6}, std::max(7, h / 4 - 2), shell);
    roundedOutline({x + 3, y + 3, w - 6, h - 6}, std::max(7, h / 4 - 2), rgba(255,255,255, static_cast<Uint8>(alpha * 0.6f)), 1);

    const int bezelX = x + w * 28 / 100;
    const int bezelY = y + h * 22 / 100;
    const int bezelW = w * 45 / 100;
    const int bezelH = h * 55 / 100;
    roundedFill({bezelX, bezelY, bezelW, bezelH}, std::max(5, h / 8), ink);
    roundedFill({bezelX + 4, bezelY + 4, bezelW - 8, bezelH - 8}, std::max(4, h / 10), screen);
    line(bezelX + bezelW / 2 - 8, bezelY + bezelH / 2 + 4,
         bezelX + bezelW / 2 + 8, bezelY + bezelH / 2 - 6, rgba(225,255,255, static_cast<Uint8>(alpha * 0.8f)));
    line(bezelX + bezelW / 2 - 3, bezelY + bezelH / 2 + 9,
         bezelX + bezelW / 2 + 12, bezelY + bezelH / 2, rgba(225,255,255, static_cast<Uint8>(alpha * 0.62f)));

    const int dpadX = x + w * 16 / 100;
    const int dpadY = y + h * 52 / 100;
    const int arm = std::max(3, h / 12);
    roundedFill({dpadX - arm * 2, dpadY - arm / 2, arm * 4, arm}, 2, ink);
    roundedFill({dpadX - arm / 2, dpadY - arm * 2, arm, arm * 4}, 2, ink);

    const int aX = x + w * 84 / 100;
    const int aY = y + h * 44 / 100;
    const int br = std::max(3, h / 11);
    circle(aX, aY, br + 2, ink, true);
    circle(aX, aY, br, rgba(255, 70, 105, alpha), true);
    circle(aX - br * 2, aY + br * 2, br + 2, ink, true);
    circle(aX - br * 2, aY + br * 2, br, rgba(255, 70, 105, alpha), true);
}

void Renderer::drawScreenTransition(Screen screen, const Config& config) {
    if (!hasRenderedScreen_) {
        lastRenderedScreen_ = screen;
        hasRenderedScreen_ = true;
        screenChangedAt_ = SDL_GetTicks64();
        return;
    }
    if (screen != lastRenderedScreen_) {
        lastRenderedScreen_ = screen;
        screenChangedAt_ = SDL_GetTicks64();
    }
    if (!config.motionEnabled || !config.screenTransitions) return;
    const std::uint64_t elapsed = SDL_GetTicks64() - screenChangedAt_;
    constexpr std::uint64_t duration = 170;
    if (elapsed >= duration) return;
    const float t = clamp01(static_cast<float>(elapsed) / duration);
    const Uint8 a = static_cast<Uint8>(150 * (1.0f - easeOutCubic(t)));
    fill({SIDEBAR_W, 0, W - SIDEBAR_W, H}, rgba(0,0,0,a));
}

SDL_Texture* Renderer::textTexture(TTF_Font* font, const std::string& value, SDL_Color color, int& w, int& h) {
    if (!font || value.empty()) { w = h = 0; return nullptr; }
    std::ostringstream key;
    key << reinterpret_cast<std::uintptr_t>(font) << '|' << colorKey(color) << '|' << value;
    const std::string cacheKey = key.str();
    if (auto found = textCache_.find(cacheKey); found != textCache_.end()) {
        found->second.lastUse = frame_;
        w = found->second.w; h = found->second.h;
        return found->second.texture;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, value.c_str(), color);
    if (!surface) { w = h = 0; return nullptr; }
    w = surface->w; h = surface->h;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!texture) return nullptr;
    textCache_[cacheKey] = TextureEntry{texture, w, h, rgba(0,0,0), frame_};
    return texture;
}

void Renderer::text(TTF_Font* font, const std::string& value, int x, int y, SDL_Color color,
                    int maxWidth, bool centered) {
    const std::string shown = maxWidth > 0 ? ellipsize(font, value, maxWidth) : value;
    int w = 0, h = 0;
    SDL_Texture* texture = textTexture(font, shown, color, w, h);
    if (!texture) return;
    SDL_Rect dst{x, y, w, h};
    if (centered) dst.x -= w / 2;
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void Renderer::textRight(TTF_Font* font, const std::string& value, int rightX, int y, SDL_Color color) {
    int w = 0, h = 0;
    SDL_Texture* texture = textTexture(font, value, color, w, h);
    if (!texture) return;
    SDL_Rect dst{rightX - w, y, w, h};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void Renderer::wrappedText(TTF_Font* font, const std::string& value, SDL_Rect rect, SDL_Color color, int lineGap) {
    if (!font || value.empty() || rect.w <= 0 || rect.h <= 0) return;
    std::istringstream words(value);
    std::string word, lineText;
    int y = rect.y;
    int lineH = 0, dummy = 0;
    TTF_SizeUTF8(font, "Ag", &dummy, &lineH);
    while (words >> word) {
        const std::string trial = lineText.empty() ? word : lineText + " " + word;
        int w = 0, h = 0;
        TTF_SizeUTF8(font, trial.c_str(), &w, &h);
        if (w <= rect.w) { lineText = trial; continue; }
        if (!lineText.empty()) {
            if (y + lineH > rect.y + rect.h) break;
            text(font, lineText, rect.x, y, color, rect.w);
