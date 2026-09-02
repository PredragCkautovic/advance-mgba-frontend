        roundedOutline({cx - d, cy - d / 2, d * 2, d}, d / 2, rgba(accent.r,accent.g,accent.b,static_cast<Uint8>(38 - i * 4)), 1);
    }
    drawBrandHandheld(cx - 94, cy - 78, 188, 122, accent);
    text(fonts_.hero, "ADVANCE", cx, cy + 62, rgba(255,255,255), 0, true);
    text(fonts_.small, "YOUR GAME BOY ADVANCE UNIVERSE", cx, cy + 112, rgba(161,161,174), 0, true);
    text(fonts_.tiny, message, cx, cy + 151, rgba(119,119,132), 680, true);
    const int barW = 310;
    roundedFill({cx - barW / 2, cy + 188, barW, 4}, 2, rgba(43,43,52));
    const int pulse = 70 + static_cast<int>((SDL_GetTicks64() / 7) % (barW - 70));
    roundedFill({cx - barW / 2 + pulse - 70, cy + 188, 70, 4}, 2, accent);
}

void Renderer::drawToast(const std::string& toast) {
    if (toast.empty()) return;
    int tw = 0, th = 0;
    TTF_SizeUTF8(fonts_.small, toast.c_str(), &tw, &th);
    const int w = std::min(620, tw + 44);
    const int x = W / 2 - w / 2;
    const int y = H - FOOTER_H - 58;
    roundedFill({x, y, w, 42}, 17, rgba(20,20,27,244));
    roundedOutline({x, y, w, 42}, 17, rgba(92,92,105), 1);
    text(fonts_.small, toast, W / 2, y + 10, rgba(255,255,255), w - 34, true);
}

std::optional<std::size_t> Renderer::hitTestLibrary(int x, int y,
                                                    std::size_t visibleCount,
                                                    std::size_t selected,
                                                    const Config& config) const {
    const GalleryLayout l = galleryLayout(config);
    if (x < l.x || y < l.y || x >= l.x + l.w || y >= l.y + l.h) return std::nullopt;
    const int unitW = l.cardW + l.gap, unitH = l.cardH + l.gap;
    const int col = (x - l.x) / unitW, row = (y - l.y) / unitH;
    if (col < 0 || col >= l.cols || row < 0 || row >= l.rows) return std::nullopt;
    const int localX = (x - l.x) % unitW, localY = (y - l.y) % unitH;
    if (localX >= l.cardW || localY >= l.cardH) return std::nullopt;
    const std::size_t page = l.perPage ? selected / l.perPage : 0;
    const std::size_t index = page * l.perPage + static_cast<std::size_t>(row * l.cols + col);
    return index < visibleCount ? std::optional<std::size_t>(index) : std::nullopt;
}

std::optional<std::pair<std::size_t, std::size_t>> Renderer::hitTestHome(int x, int y,
                                                                         const std::vector<HomeRow>& rows,
                                                                         std::size_t selectedRow,
                                                                         std::size_t selectedColumn) const {
    if (rows.empty()) return std::nullopt;
    constexpr int cardW = 132, cardH = 138, gap = 13, visibleCards = 8;
    const std::array<int,2> ys = {HEADER_H + 242, HEADER_H + 424};
    const std::size_t r0 = std::min(selectedRow, rows.size() - 1);
    const std::size_t r1 = r0 + 1 < rows.size() ? r0 + 1 : (r0 > 0 ? r0 - 1 : r0);
    const std::array<std::size_t,2> rs = {r0, r1};
    for (int lineIndex = 0; lineIndex < 2; ++lineIndex) {
        const std::size_t r = rs[lineIndex];
        if (lineIndex == 1 && r == r0) continue;
        const int cardY = ys[lineIndex] + 32;
        if (y < cardY || y >= cardY + cardH || x < CONTENT_X) continue;
        const int slot = (x - CONTENT_X) / (cardW + gap);
        if (slot < 0 || slot >= visibleCards || (x - CONTENT_X) % (cardW + gap) >= cardW) continue;
        std::size_t start = 0;
        if (r == selectedRow) {
            if (selectedColumn >= visibleCards) start = selectedColumn - visibleCards + 1;
            if (rows[r].games.size() > visibleCards && start + visibleCards > rows[r].games.size())
                start = rows[r].games.size() - visibleCards;
        }
        const std::size_t index = start + static_cast<std::size_t>(slot);
        if (index < rows[r].games.size()) return std::make_pair(r, index);
    }
    return std::nullopt;
}

int Renderer::hitTestSidebar(int x, int y, bool expanded) const {
    const int width = expanded ? SIDEBAR_EXPANDED_W : SIDEBAR_W;
    if (x < 0 || x >= width) return -1;
    const std::array<int, 7> ys = {148, 228, 308, 388, 468, 548, 646};
    for (int i = 0; i < 7; ++i) if (std::abs(y - ys[i]) <= 31) return i;
    return -1;
}

} // namespace advance
