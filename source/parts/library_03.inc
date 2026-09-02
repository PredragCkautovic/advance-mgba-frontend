    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i])) && suffixLetters < 3) { ++i; ++suffixLetters; }
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (digits >= 3 && i == s.size() && prefixLetters <= 3 && suffixLetters <= 2) return true;

    int letters = 0, numbers = 0;
    for (unsigned char c : s) { if (std::isalpha(c)) ++letters; if (std::isdigit(c)) ++numbers; }
    return numbers >= 3 && letters <= 2;
}

std::string titleFromLegacySidecar(const fs::path& rom) {
    const fs::path dir = rom.parent_path();
    const char* textNames[] = {"title.txt", "name.txt", "display_name.txt", "display-name.txt"};
    for (const char* name : textNames) {
        std::ifstream in(dir / name);
        std::string line;
        if (in && std::getline(in, line)) {
            line = sanitizeSingleLine(line);
            if (!line.empty()) return line;
        }
    }
    const char* iniNames[] = {"metadata.ini", "game.ini", "info.ini"};
    for (const char* name : iniNames) {
        std::ifstream in(dir / name);
        std::string line;
        while (in && std::getline(in, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = lower(trim(line.substr(0, pos)));
            if (key != "title" && key != "name" && key != "display_name") continue;
            std::string value = sanitizeSingleLine(line.substr(pos + 1));
            if (!value.empty()) return value;
        }
    }
    return {};
}

std::string descriptionFromSidecar(const fs::path& dir) {
    const char* names[] = {"description.txt", "about.txt", "readme.txt"};
    for (const char* name : names) {
        std::string text = readTextFile(dir / name, 12 * 1024);
        text = trim(text);
        if (!text.empty()) {
            for (char& c : text) if (c == '\r') c = ' ';
            return text;
        }
    }
    return {};
}

struct CoverChoice { std::string path; std::string semanticTitle; };

int imagePriority(const fs::path& p) {
    const std::string n = normalizeName(p.stem().string());
    if (n == "cover" || n == "frontcover" || n == "boxart" || n == "boxartfront") return 0;
    if (n == "front" || n == "poster" || n == "artwork") return 1;
    if (n.find("screen") != std::string::npos || n.find("shot") != std::string::npos) return 5;
    if (n == "banner" || n == "hero" || n == "background" || n == "backdrop") return 6;
    if (!looksLikeArchiveId(p.stem().string())) return 2;
    return 4;
}

CoverChoice findSiblingCover(const fs::path& rom) {
    const fs::path dir = rom.parent_path();
    const std::string stem = rom.stem().string();
    const std::string normalizedStem = normalizeName(stem);
    const std::vector<std::string> exts = {".png", ".jpg", ".jpeg", ".webp"};
    for (const auto& ext : exts) {
        const fs::path exact = dir / (stem + ext);
        std::error_code ec;
        if (fs::exists(exact, ec) && fs::is_regular_file(exact, ec)) {
            CoverChoice c{exact.string(), {}};
            if (!looksLikeArchiveId(exact.stem().string())) c.semanticTitle = prettyTitleFromFilename(exact.filename().string());
            return c;
        }
    }

    std::vector<fs::path> images;
    std::error_code ec;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || !isImage(it->path())) continue;
        images.push_back(it->path());
        if (normalizeName(it->path().stem().string()) == normalizedStem) {
            CoverChoice c{it->path().string(), {}};
            if (!looksLikeArchiveId(it->path().stem().string())) c.semanticTitle = prettyTitleFromFilename(it->path().filename().string());
            return c;
        }
    }
    if (images.empty()) return {};
    auto fileSize = [](const fs::path& p) -> std::uintmax_t {
        std::error_code e; const auto n = fs::file_size(p, e); return e ? 0 : n;
    };
    std::stable_sort(images.begin(), images.end(), [&](const fs::path& a, const fs::path& b) {
        const int pa = imagePriority(a), pb = imagePriority(b);
        return pa != pb ? pa < pb : fileSize(a) > fileSize(b);
    });
    CoverChoice result{images.front().string(), {}};
    if (!looksLikeArchiveId(images.front().stem().string()) && imagePriority(images.front()) == 2) {
        result.semanticTitle = prettyTitleFromFilename(images.front().filename().string());
    }
    return result;
}

std::unordered_map<std::string, std::string> buildCoverIndex(const Config& config) {
    std::unordered_map<std::string, std::string> index;
    std::error_code ec;
    fs::path dir(config.coverDir);
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return index;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!it->is_regular_file(ec) || !isImage(it->path())) continue;
