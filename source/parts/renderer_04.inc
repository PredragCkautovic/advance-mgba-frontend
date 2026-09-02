        double v = std::sin(2.0 * pi * f1 * t);
        if (f2 > 0.0) v = 0.7 * v + 0.3 * std::sin(2.0 * pi * f2 * t);
        samples[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(amp * env * v);
    }
    return samples;
}

void Renderer::playSound(UiSound sound, const Config& config) {
    if (!config.uiSounds || config.uiVolume <= 0 || !audioDevice_) return;
    if (SDL_GetQueuedAudioSize(audioDevice_) > 48000) SDL_ClearQueuedAudio(audioDevice_);
    const auto samples = synthTone(sound, config.uiVolume);
    if (!samples.empty()) SDL_QueueAudio(audioDevice_, samples.data(), static_cast<Uint32>(samples.size() * sizeof(std::int16_t)));
}

void Renderer::beginFrame() {
    SDL_PumpEvents();
    ++frame_;
    refreshSystemStatus();
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
}

void Renderer::endFrame() {
    pruneCoverCache();
    pruneTextCache();
    SDL_RenderPresent(renderer_);
}

SDL_Color Renderer::accentColor(const Config& config) const {
    if (config.theme == "atomic-purple") return rgba(158, 92, 255);
    if (config.theme == "emerald") return rgba(34, 218, 131);
    if (config.theme == "midnight-gold") return rgba(255, 184, 61);
    if (config.theme == "ice-blue") return rgba(67, 184, 255);
    if (config.theme == "neon-coral") return rgba(255, 75, 125);
    return rgba(255, 39, 65);
}

SDL_Color Renderer::secondaryColor(const Config& config) const {
    if (config.theme == "atomic-purple") return rgba(86, 48, 170);
    if (config.theme == "emerald") return rgba(13, 105, 69);
    if (config.theme == "midnight-gold") return rgba(130, 79, 10);
    if (config.theme == "ice-blue") return rgba(23, 92, 158);
    if (config.theme == "neon-coral") return rgba(133, 28, 67);
    return rgba(128, 12, 31);
}

SDL_Color Renderer::surfaceColor(const Config&, Uint8 alpha) const { return rgba(8, 8, 12, alpha); }

void Renderer::fill(SDL_Rect rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer_, &rect);
}

void Renderer::outline(SDL_Rect rect, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect r{rect.x - i, rect.y - i, rect.w + i * 2, rect.h + i * 2};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

void Renderer::line(int x1, int y1, int x2, int y2, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
}

void Renderer::circle(int cx, int cy, int radius, SDL_Color color, bool filledCircle) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int y = -radius; y <= radius; ++y) {
        const int x = static_cast<int>(std::sqrt(std::max(0, radius * radius - y * y)));
        if (filledCircle) SDL_RenderDrawLine(renderer_, cx - x, cy + y, cx + x, cy + y);
        else {
            SDL_RenderDrawPoint(renderer_, cx - x, cy + y);
            SDL_RenderDrawPoint(renderer_, cx + x, cy + y);
        }
    }
}

void Renderer::roundedFill(SDL_Rect rect, int radius, SDL_Color color) {
    radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    if (radius == 0) { fill(rect, color); return; }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int y = 0; y < rect.h; ++y) {
        int inset = 0;
        if (y < radius) {
            const int dy = radius - y - 1;
            inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        } else if (y >= rect.h - radius) {
            const int dy = y - (rect.h - radius);
            inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        }
        SDL_RenderDrawLine(renderer_, rect.x + inset, rect.y + y, rect.x + rect.w - 1 - inset, rect.y + y);
    }
}

void Renderer::roundedOutline(SDL_Rect rect, int radius, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; ++t) {
        SDL_Rect r{rect.x + t, rect.y + t, rect.w - t * 2, rect.h - t * 2};
        const int rr = std::max(1, radius - t);
        line(r.x + rr, r.y, r.x + r.w - rr - 1, r.y, color);
        line(r.x + rr, r.y + r.h - 1, r.x + r.w - rr - 1, r.y + r.h - 1, color);
        line(r.x, r.y + rr, r.x, r.y + r.h - rr - 1, color);
        line(r.x + r.w - 1, r.y + rr, r.x + r.w - 1, r.y + r.h - rr - 1, color);
        for (int deg = 0; deg <= 90; deg += 3) {
            const double a = deg * 3.14159265358979323846 / 180.0;
            const int dx = static_cast<int>(std::cos(a) * rr);
            const int dy = static_cast<int>(std::sin(a) * rr);
            SDL_RenderDrawPoint(renderer_, r.x + rr - dx, r.y + rr - dy);
            SDL_RenderDrawPoint(renderer_, r.x + r.w - rr - 1 + dx, r.y + rr - dy);
            SDL_RenderDrawPoint(renderer_, r.x + rr - dx, r.y + r.h - rr - 1 + dy);
            SDL_RenderDrawPoint(renderer_, r.x + r.w - rr - 1 + dx, r.y + r.h - rr - 1 + dy);
        }
    }
}

