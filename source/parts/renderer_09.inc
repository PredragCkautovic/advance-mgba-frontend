            roundedFill({18, top + 5, drawerW - 36, 62}, 20, rgba(255,255,255,16));
            roundedOutline({14, top, drawerW - 28, 72}, 24, rgba(255,255,255,190), 1);
        } else if (on) {
            roundedFill({14, top, drawerW - 28, 72}, 24, rgba(accent.r,accent.g,accent.b,34));
            roundedOutline({14, top, drawerW - 28, 72}, 24, rgba(accent.r,accent.g,accent.b,116), 1);
        }
        drawSidebarIcon(52, ys[i], i, (focus || on) ? rgba(255,255,255) : rgba(168,168,182));
        if (t > 0.18f) {
            const Uint8 a = static_cast<Uint8>(255 * clamp01((t - 0.18f) / 0.40f));
            text(fonts_.small, labels[i], 106, top + 11, (focus || on) ? rgba(255,255,255,a) : rgba(225,225,232,a), 228);
            text(fonts_.tiny, subs[i], 106, top + 41, focus ? rgba(255,242,244,a) : rgba(129,129,143,a), 242);
            if (on && !focus) roundedFill({drawerW - 44, ys[i] - 6, 18, 12}, 6, rgba(second.r, second.g, second.b, 170));
        }
    }

    if (t > 0.56f) {
        const Uint8 a = static_cast<Uint8>(235 * clamp01((t - 0.56f) / 0.30f));
        roundedFill({18, 686, drawerW - 36, 24}, 12, rgba(17,17,24,a));
        text(fonts_.tiny, "A  OPEN", 32, 690, rgba(238,238,244,a));
        textRight(fonts_.tiny, "B / →  CLOSE", drawerW - 30, 690, rgba(155,155,169,a));
    }
}

void Renderer::drawAvatar(int x, int y, int diameter, const Config& config) {
    const SDL_Color accent = accentColor(config);
    circle(x + diameter / 2, y + diameter / 2, diameter / 2 + 2, rgba(accent.r, accent.g, accent.b, 170), true);
    if (avatarTexture_) {
        SDL_Rect dst{x, y, diameter, diameter};
        SDL_RenderCopy(renderer_, avatarTexture_, nullptr, &dst);
    } else {
        circle(x + diameter / 2, y + diameter / 2, diameter / 2, rgba(24,24,30), true);
        text(fonts_.body, nickname_.empty() ? "P" : std::string(1, nickname_[0]), x + diameter / 2, y + 7,
             rgba(255,255,255), 0, true);
    }
}

void Renderer::drawWifiIcon(int x, int y, SDL_Color color) {
    if (!connected_) { circle(x, y + 10, 2, rgba(110,110,120), true); return; }
    const int bars = static_cast<int>(std::max<std::uint32_t>(1, wifiStrength_));
    for (int i = 0; i < 4; ++i) {
        const int h = 4 + i * 4;
        roundedFill({x + i * 5, y + 16 - h, 3, h}, 1, i < bars ? color : rgba(75,75,85));
    }
}

void Renderer::drawBatteryIcon(int x, int y, SDL_Color color) {
    roundedOutline({x, y, 28, 13}, 4, color, 1);
    fill({x + 29, y + 4, 3, 5}, color);
    const int inner = static_cast<int>(24 * batteryPercent_ / 100);
    if (inner > 0) roundedFill({x + 2, y + 2, inner, 9}, 2,
        batteryPercent_ <= 15 ? rgba(255,75,75) : color);
    if (charging_) text(fonts_.tiny, "+", x + 12, y - 4, rgba(255,255,255));
}

std::string Renderer::formatClock() const {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    if (!tm) return "--:--";
    char buf[16]; std::strftime(buf, sizeof(buf), "%H:%M", tm); return buf;
}

void Renderer::drawProfileChip(const Config& config, bool compact) {
    const SDL_Color accent = accentColor(config);
    const int w = compact ? 176 : 205;
    const int x = W - w - 18;
    const int y = compact ? 12 : 11;
    const int h = 53;
    roundedFill({x, y, w, h}, 17, rgba(8,8,12,222));
    roundedOutline({x, y, w, h}, 17, rgba(85,85,96), 1);
    roundedFill({x + w - 5, y + 5, 3, h - 10}, 2, accent);
    drawAvatar(x + 8, y + 7, 39, config);
    text(fonts_.small, nickname_, x + 55, y + 8, rgba(252,252,255), w - 67);
    text(fonts_.tiny, profileSource_ == "Switch profile" ? "SWITCH PROFILE" : "ADVANCE PROFILE",
         x + 55, y + 31, rgba(136,136,148), w - 67);
}

void Renderer::drawTopChrome(const std::string& eyebrow, const std::string& title, const std::string& subtitle,
                             const Config& config, bool compactProfile) {
    const SDL_Color accent = accentColor(config);
    fill({SIDEBAR_W, 0, W - SIDEBAR_W, HEADER_H}, rgba(0,0,0,186));
    line(SIDEBAR_W, HEADER_H - 1, W, HEADER_H - 1, rgba(39,39,49));
    roundedFill({CONTENT_X, 8, 170, 18}, 8, rgba(accent.r,accent.g,accent.b,28));
    text(fonts_.tiny, eyebrow, CONTENT_X + 10, 9, accent, 380);
    text(fonts_.medium, title, CONTENT_X, 29, rgba(255,255,255), 580);
    if (!subtitle.empty()) text(fonts_.tiny, subtitle, CONTENT_X + 430, 46, rgba(149,149,162), 340);

    if (config.showSystemStatus) {
        drawWifiIcon(W - (compactProfile ? 393 : 422), 28, rgba(242,242,247));
        text(fonts_.small, formatClock(), W - (compactProfile ? 355 : 384), 20, rgba(248,248,252));
        drawBatteryIcon(W - (compactProfile ? 291 : 320), 28, rgba(240,240,245));
        text(fonts_.tiny, std::to_string(batteryPercent_) + "%", W - (compactProfile ? 290 : 319), 48,
             batteryPercent_ <= 15 ? rgba(255,78,78) : rgba(143,143,155));
    }
    drawProfileChip(config, compactProfile);
    roundedFill({CONTENT_X, HEADER_H - 4, 124, 2}, 1, accent);
}

void Renderer::drawButtonHint(int& x, int y, const std::string& button, const std::string& label) {
    const int badgeW = button.size() > 2 ? 40 : (button.size() > 1 ? 32 : 22);
