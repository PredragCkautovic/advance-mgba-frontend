        index.emplace(normalizeName(it->path().stem().string()), it->path().string());
    }
    return index;
}

CoverChoice findCover(const fs::path& rom, const Config& config,
                      const std::unordered_map<std::string, std::string>& coverIndex,
                      const Metadata& metadata) {
    if (!metadata.cover.empty()) return {metadata.cover, {}};
    if (CoverChoice local = findSiblingCover(rom); !local.path.empty()) return local;
    const std::string stem = rom.stem().string();
    const std::vector<std::string> exts = {".png", ".jpg", ".jpeg", ".webp"};
    for (const auto& ext : exts) {
        fs::path exact = fs::path(config.coverDir) / (stem + ext);
        std::error_code ec;
        if (fs::exists(exact, ec)) return {exact.string(), {}};
    }
    auto it = coverIndex.find(normalizeName(stem));
    return it == coverIndex.end() ? CoverChoice{} : CoverChoice{it->second, {}};
}

std::string findBanner(const fs::path& dir, const Metadata& metadata) {
    if (!metadata.banner.empty()) return metadata.banner;
    const char* names[] = {"banner", "hero", "background", "backdrop", "header"};
    const char* exts[] = {".png", ".jpg", ".jpeg", ".webp"};
    for (const char* name : names) for (const char* ext : exts) {
        fs::path p = dir / (std::string(name) + ext);
        std::error_code ec;
        if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) return p.string();
    }
    return {};
}

std::vector<std::string> findScreenshots(const fs::path& dir, const Metadata& metadata, const std::string& cover, const std::string& banner) {
    if (!metadata.screenshots.empty()) return metadata.screenshots;
    std::vector<std::string> result;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || !isImage(it->path())) continue;
        const std::string path = it->path().string();
        if (path == cover || path == banner) continue;
        const std::string n = normalizeName(it->path().stem().string());
        if (n.find("screen") != std::string::npos || n.find("shot") != std::string::npos || n.find("preview") != std::string::npos) {
            result.push_back(path);
        }
    }
    std::sort(result.begin(), result.end());
    if (result.size() > 6) result.resize(6);
    return result;
}

struct ResolvedTitle { std::string title; std::string source; };

ResolvedTitle resolveAutomaticTitle(const fs::path& rom, const CoverChoice& cover, const Metadata& metadata) {
    if (!metadata.title.empty()) return {metadata.title, "advance.json"};
    if (std::string sidecar = titleFromLegacySidecar(rom); !sidecar.empty()) return {sidecar, "Sidecar metadata"};

    struct Candidate { std::string title; std::string source; int base; };
    std::vector<Candidate> candidates;
    const std::string folder = rom.parent_path().filename().string();
    if (!folder.empty() && normalizeName(folder) != "roms" && !looksLikeArchiveId(folder))
        candidates.push_back({prettyTitleFromFilename(folder), "Game folder", 80});
    if (!cover.semanticTitle.empty() && !looksLikeArchiveId(cover.semanticTitle))
        candidates.push_back({prettyTitleFromFilename(cover.semanticTitle), "Cover filename", 86});
    if (std::string internal = readInternalRomTitle(rom); !internal.empty() && !looksLikeArchiveId(internal))
        candidates.push_back({prettyTitleFromFilename(internal), "mGBA ROM title", 58});
    const std::string raw = prettyTitleFromFilename(rom.filename().string());
    if (!raw.empty() && !looksLikeArchiveId(raw)) candidates.push_back({raw, "ROM filename", 42});

    auto score = [](const Candidate& c) {
        int value = c.base + std::min<int>(static_cast<int>(c.title.size()), 38) + static_cast<int>(std::count(c.title.begin(), c.title.end(), ' ')) * 3;
        int alpha = 0, upper = 0;
        for (unsigned char ch : c.title) if (std::isalpha(ch)) { ++alpha; if (std::isupper(ch)) ++upper; }
        if (alpha > 5 && alpha == upper) value -= 9;
        if (c.title.size() <= 5) value -= 20;
        return value;
    };
    if (!candidates.empty()) {
        const auto it = std::max_element(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) { return score(a) < score(b); });
        return {it->title, it->source};
    }
    return {raw, "ROM filename"};
}

std::int64_t fileTimestamp(const fs::path& path) {
    std::error_code ec;
    const auto ft = fs::last_write_time(path, ec);
    if (ec) return 0;
    const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ft - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(sctp));
}

void dedupe(std::vector<std::string>& values) {
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    for (auto& value : values) {
        value = sanitizeSingleLine(value);
        if (value.empty()) continue;
        const std::string key = normalizeName(value);
