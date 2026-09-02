    std::vector<std::string> tags;
    int releaseYear{0};
};

Metadata readAdvanceMetadata(const fs::path& dir) {
    Metadata m;
    const fs::path path = dir / "advance.json";
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec)) return m;
    const std::string json = readTextFile(path, 256 * 1024);
    if (json.empty()) return m;

    m.path = path.string();
    m.title = jsonString(json, "title");
    m.shortTitle = jsonString(json, "short_title");
    m.author = jsonString(json, "author");
    m.version = jsonString(json, "version");
    m.baseGame = jsonString(json, "base_game");
    m.description = jsonString(json, "description");
    m.releaseYear = jsonInt(json, "release_year", 0);
    m.genres = jsonStringArray(json, "genre");
    if (m.genres.empty()) m.genres = jsonStringArray(json, "genres");
    m.tags = jsonStringArray(json, "tags");
    m.cover = resolveRelativeAsset(dir, jsonString(json, "cover"));
    m.banner = resolveRelativeAsset(dir, jsonString(json, "banner"));
    for (const auto& item : jsonStringArray(json, "screenshots")) {
        if (std::string p = resolveRelativeAsset(dir, item); !p.empty()) m.screenshots.push_back(std::move(p));
    }
    return m;
}

std::string cleanRomHeaderTitle(const char* data, std::size_t length) {
    std::string out;
    out.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == 0x00 || c == 0xFF) break;
        if (c < 0x20 || c > 0x7E) return {};
        out.push_back(static_cast<char>(c));
    }
    std::string cleaned;
    bool previousSpace = false;
    for (unsigned char c : out) {
        const bool isSpace = std::isspace(c) != 0;
        if (isSpace) {
            if (!cleaned.empty() && !previousSpace) cleaned.push_back(' ');
        } else cleaned.push_back(static_cast<char>(c));
        previousSpace = isSpace;
    }
    while (!cleaned.empty() && cleaned.back() == ' ') cleaned.pop_back();
    const bool meaningful = std::any_of(cleaned.begin(), cleaned.end(), [](unsigned char c) { return std::isalnum(c) != 0; });
    return meaningful ? cleaned : std::string{};
}

std::string readInternalRomTitle(const fs::path& rom) {
    std::string ext = lower(rom.extension().string());
    std::ifstream in(rom, std::ios::binary);
    if (!in) return {};
    if (ext == ".gba" || ext == ".agb") {
        std::array<char, 12> title{};
        in.seekg(0xA0, std::ios::beg);
        if (!in.read(title.data(), static_cast<std::streamsize>(title.size()))) return {};
        return cleanRomHeaderTitle(title.data(), title.size());
    }
    if (ext == ".gb" || ext == ".gbc") {
        std::array<char, 16> title{};
        in.seekg(0x134, std::ios::beg);
        if (!in.read(title.data(), static_cast<std::streamsize>(title.size()))) return {};
        const unsigned char cgbFlag = static_cast<unsigned char>(title[15]);
        const std::size_t length = (cgbFlag == 0x80 || cgbFlag == 0xC0) ? 15 : 16;
        return cleanRomHeaderTitle(title.data(), length);
    }
    return {};
}

std::string readGameCode(const fs::path& rom) {
    const std::string ext = lower(rom.extension().string());
    if (ext != ".gba" && ext != ".agb") return {};
    std::ifstream in(rom, std::ios::binary);
    if (!in) return {};
    std::array<char, 4> code{};
    in.seekg(0xAC, std::ios::beg);
    if (!in.read(code.data(), static_cast<std::streamsize>(code.size()))) return {};
    for (unsigned char c : code) if (c < 0x20 || c > 0x7E) return {};
    return std::string(code.data(), code.size());
}

bool isAllowedRom(const fs::path& p, const Config& config) {
    const std::string ext = lower(p.extension().string());
    if (ext == ".gba" || ext == ".agb") return true;
    if (config.includeGb && ext == ".gb") return true;
    if (config.includeGbc && ext == ".gbc") return true;
    return false;
}

bool isImage(const fs::path& p) {
    const std::string ext = lower(p.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp";
}

bool looksLikeArchiveId(const std::string& value) {
    std::string s = trim(fs::path(value).stem().string());
    if (s.empty()) return true;
    const std::string l = lower(s);
    static const std::unordered_set<std::string> generic = {
        "cover", "boxart", "box art", "front", "front cover", "art", "artwork", "image", "poster", "icon",
        "thumb", "thumbnail", "game", "rom", "banner", "hero", "background", "screenshot", "screen"
    };
    if (generic.count(l)) return true;

    std::size_t i = 0, prefixLetters = 0;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i])) && prefixLetters < 4) { ++i; ++prefixLetters; }
    while (i < s.size() && (s[i] == '-' || s[i] == '_' || std::isspace(static_cast<unsigned char>(s[i])))) ++i;
    std::size_t digits = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) { ++i; ++digits; }
    while (i < s.size() && (std::isspace(static_cast<unsigned char>(s[i])) || s[i] == '-' || s[i] == '_')) ++i;
    std::size_t suffixLetters = 0;
