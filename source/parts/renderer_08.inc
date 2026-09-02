    roundedFill({17, 36, 4, 10}, 2, rgba(9, 17, 49, 255));
    circle(72, 40, 4, rgba(17, 19, 54, 255), true);
    circle(62, 46, 4, rgba(17, 19, 54, 255), true);
    line(48, 43, 59, 36, rgba(223,255,255,185));
    line(53, 47, 64, 40, rgba(223,255,255,128));
    text(fonts_.tiny, "ADV", 49, 88, rgba(214,214,225), 0, true);

    int active = 1;
    if (screen == Screen::Home) active = 0;
    else if (screen == Screen::Collections) active = 2;
    else if (screen == Screen::Settings || screen == Screen::About) active = 6;
    else if (shelf == Shelf::Favorites) active = 3;
    else if (shelf == Shelf::Recent) active = 4;
    else if (shelf == Shelf::Search) active = 5;

    const std::array<int, 7> ys = {146, 226, 306, 386, 466, 546, 642};
    for (int i = 0; i < 7; ++i) {
        const bool on = i == active;
        const bool focus = sidebarFocused_ && i == sidebarSelected_;
        if (on || focus) {
            const int itemX = 8;
            const int itemY = ys[i] - 36;
            const int itemW = 80;
            const int itemH = 72;
            if (focus) {
                roundedFill({itemX - 1, itemY - 2, itemW + 2, itemH + 4}, 24, rgba(accent.r, accent.g, accent.b, 44));
                roundedFill({itemX, itemY, itemW, itemH}, 23, rgba(accent.r, accent.g, accent.b, 76));
                roundedOutline({itemX, itemY, itemW, itemH}, 23, rgba(255,255,255,205), 2);
            } else {
                roundedFill({itemX, itemY, itemW, itemH}, 23, rgba(accent.r, accent.g, accent.b, 24));
                roundedOutline({itemX, itemY, itemW, itemH}, 23, rgba(accent.r, accent.g, accent.b, 150), 1);
            }
            if (on) roundedFill({0, ys[i] - 24, 5, 48}, 2, mixColor(accent, secondary, 0.18f));
        }
        drawSidebarIcon(48, ys[i], i, (on || focus) ? rgba(255,255,255) : rgba(159,159,172));
        if (on && !focus) circle(76, ys[i] - 18, 3, rgba(accent.r, accent.g, accent.b, 230), true);
    }
}

void Renderer::drawSidebarOverlay(Screen screen, Shelf shelf, const Config& config) {
    if (sidebarProgress_ <= 0.001f) return;
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const float t = easeOutCubic(sidebarProgress_);
    const int drawerW = SIDEBAR_W + static_cast<int>((SIDEBAR_EXPANDED_W - SIDEBAR_W) * t);
    const Uint8 dim = static_cast<Uint8>(118 * t);

    fill({SIDEBAR_W, 0, W - SIDEBAR_W, H}, rgba(0,0,0,dim));
    for (int i = 0; i < 10; ++i) {
        const Uint8 a = static_cast<Uint8>(std::max(0, 30 - i * 3) * t);
        fill({drawerW + i * 3, 0, 3, H}, rgba(0,0,0,a));
    }
    fill({0, 0, drawerW, H}, rgba(4,4,8, static_cast<Uint8>(250 * t)));
    fill({0, 0, 4, H}, accent);
    fill({4, 0, 6, H}, rgba(255,255,255,10));
    line(drawerW - 1, 0, drawerW - 1, H, rgba(74,74,86, static_cast<Uint8>(185 * t)));

    roundedFill({12, 10, 74, 74}, 24, rgba(29, 22, 71, 255));
    roundedFill({14, 12, 70, 70}, 22, rgba(122, 92, 247, 255));
    roundedFill({17, 15, 64, 64}, 20, rgba(105, 84, 228, 255));
    roundedOutline({14, 12, 70, 70}, 22, rgba(255,255,255,180), 1);
    roundedFill({24, 28, 48, 28}, 9, rgba(28, 39, 96, 255));
    roundedFill({27, 31, 42, 22}, 6, rgba(57, 214, 226, 255));
    roundedFill({19, 39, 10, 4}, 2, rgba(9, 17, 49, 255));
    roundedFill({17, 36, 4, 10}, 2, rgba(9, 17, 49, 255));
    circle(72, 40, 4, rgba(17, 19, 54, 255), true);
    circle(62, 46, 4, rgba(17, 19, 54, 255), true);
    line(48, 43, 59, 36, rgba(223,255,255,185));
    line(53, 47, 64, 40, rgba(223,255,255,128));
    if (t > 0.08f) {
        const Uint8 a = static_cast<Uint8>(255 * clamp01((t - 0.08f) / 0.40f));
        text(fonts_.medium, "ADVANCE", 104, 16, rgba(255,255,255,a), 226);
        text(fonts_.tiny, "DEFINITIVE GBA FRONTEND", 105, 48, rgba(160,160,173,a), 248);
        roundedFill({104, 64, 118, 21}, 10, rgba(accent.r,accent.g,accent.b, static_cast<Uint8>(54 * t)));
        text(fonts_.tiny, "STORE READY UI", 163, 67, rgba(255,255,255,a), 0, true);
    }

    const std::array<int, 7> ys = {146, 226, 306, 386, 466, 546, 642};
    const std::array<const char*, 7> labels = {"Home", "Library", "Collections", "Favorites", "Recent", "Search", "Settings"};
    const std::array<const char*, 7> subs = {
        "Your personal landing screen", "Browse every game in your library", "Curated + custom shelves",
        "The games you saved", "Continue recent sessions", "Find titles, authors, and tags", "Theme, layout, and behavior"
    };

    int active = 1;
    if (screen == Screen::Home) active = 0;
    else if (screen == Screen::Collections) active = 2;
    else if (screen == Screen::Settings || screen == Screen::About) active = 6;
    else if (shelf == Shelf::Favorites) active = 3;
    else if (shelf == Shelf::Recent) active = 4;
    else if (shelf == Shelf::Search) active = 5;

    for (int i = 0; i < 7; ++i) {
        const bool focus = i == sidebarSelected_;
        const bool on = i == active;
        const int top = ys[i] - 36;
        if (focus) {
            roundedFill({14, top, drawerW - 28, 72}, 24, rgba(accent.r,accent.g,accent.b,214));
