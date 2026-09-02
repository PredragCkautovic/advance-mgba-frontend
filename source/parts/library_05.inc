        if (!seen.insert(key).second) continue;
        out.push_back(value);
    }
    values = std::move(out);
}

} // namespace

std::string normalizeName(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char c : value) if (std::isalnum(c)) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

std::string prettyTitleFromFilename(const std::string& filename) {
    std::string s = fs::path(filename).stem().string();
    for (char& c : s) if (c == '_' || c == '.') c = ' ';
    auto cut = s.find(" ("); if (cut != std::string::npos) s.erase(cut);
    cut = s.find(" ["); if (cut != std::string::npos) s.erase(cut);

    std::string split;
    split.reserve(s.size() + 8);
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const unsigned char prev = i > 0 ? static_cast<unsigned char>(s[i - 1]) : 0;
        const unsigned char next = i + 1 < s.size() ? static_cast<unsigned char>(s[i + 1]) : 0;
        const bool lowerToUpper = i > 0 && std::islower(prev) && std::isupper(c);
        const bool acronymToWord = i > 0 && std::isupper(prev) && std::isupper(c) && next && std::islower(next);
        const bool alphaToDigit = i > 0 && std::isalpha(prev) && std::isdigit(c);
        if ((lowerToUpper || acronymToWord || alphaToDigit) && !split.empty() && split.back() != ' ') split.push_back(' ');
        split.push_back(static_cast<char>(c));
    }
    s = split;
    std::string out;
    bool prevSpace = false;
    for (char c : s) {
        const bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
        if (!space || !prevSpace) out.push_back(c);
        prevSpace = space;
    }
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out.empty() ? fs::path(filename).stem().string() : out;
}

std::string searchableText(const Game& game) {
    std::ostringstream ss;
    ss << game.title << ' ' << game.shortTitle << ' ' << game.author << ' ' << game.version << ' '
       << game.baseGame << ' ' << game.description << ' ' << fs::path(game.folderPath).filename().string() << ' '
       << fs::path(game.path).filename().string() << ' ' << game.gameCode << ' ';
    for (const auto& g : game.genres) ss << g << ' ';
    for (const auto& t : game.tags) ss << t << ' ';
    return lower(ss.str());
}

bool gameMatchesQuery(const Game& game, const std::string& query) {
    std::string q = lower(trim(query));
    if (q.empty()) return true;
    const std::string haystack = searchableText(game);
    std::istringstream tokens(q);
    std::string token;
    while (tokens >> token) if (haystack.find(token) == std::string::npos) return false;
    return true;
}

TitleOverrideMap loadTitleOverrides(const std::string& path) {
    TitleOverrideMap result;
    std::ifstream in(path);
    std::string line;
    while (in && std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string rom = line.substr(0, tab);
        std::string title = sanitizeSingleLine(line.substr(tab + 1));
        if (!rom.empty() && !title.empty()) result[rom] = title;
    }
    return result;
}

bool saveTitleOverrides(const std::string& path, const TitleOverrideMap& overrides) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::vector<std::pair<std::string, std::string>> rows(overrides.begin(), overrides.end());
    std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    for (const auto& [rom, title] : rows) if (!rom.empty() && !title.empty()) {
        out << rom << '\t' << sanitizeSingleLine(title) << '\n';
    }
    return true;
}

std::vector<Game> scanLibrary(const Config& config, const StateMap& state, const TitleOverrideMap& titleOverrides) {
    std::vector<Game> games;
    std::error_code ec;
    const fs::path root(config.romDir);
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return games;
    const auto coverIndex = buildCoverIndex(config);

    auto add = [&](const fs::directory_entry& entry) {
        std::error_code localEc;
        if (!entry.is_regular_file(localEc) || !isAllowedRom(entry.path(), config)) return;
        const fs::path rom = entry.path();
        const fs::path dir = rom.parent_path();
        const Metadata metadata = readAdvanceMetadata(dir);
        const CoverChoice cover = findCover(rom, config, coverIndex, metadata);

        Game g;
        g.path = rom.string();
        g.folderPath = dir.string();
        g.metadataPath = metadata.path;
        g.coverPath = cover.path;
        g.bannerPath = findBanner(dir, metadata);
        g.screenshotPaths = findScreenshots(dir, metadata, g.coverPath, g.bannerPath);
        g.gameCode = readGameCode(rom);

        const ResolvedTitle automatic = resolveAutomaticTitle(rom, cover, metadata);
        g.automaticTitle = automatic.title;
        g.titleSource = automatic.source;
