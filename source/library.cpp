#include "advance/library.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace advance {
namespace fs = std::filesystem;

namespace {

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string sanitizeSingleLine(std::string s) {
    for (char& c : s) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    return trim(s);
}

std::string readTextFile(const fs::path& path, std::size_t maxBytes = 64 * 1024) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::string out;
    out.resize(maxBytes);
    in.read(out.data(), static_cast<std::streamsize>(maxBytes));
    out.resize(static_cast<std::size_t>(in.gcount()));
    if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF &&
        static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return out;
}

std::string jsonUnescape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '\\' || i + 1 >= in.size()) { out.push_back(in[i]); continue; }
        const char n = in[++i];
        switch (n) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            default: out.push_back(n); break;
        }
    }
    return out;
}

std::size_t jsonValueStart(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) return pos;
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos) return pos;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    return pos;
}

std::string jsonString(const std::string& json, const std::string& key) {
    std::size_t pos = jsonValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') return {};
    ++pos;
    std::string raw;
    bool escaped = false;
    for (; pos < json.size(); ++pos) {
        char c = json[pos];
        if (!escaped && c == '"') break;
        raw.push_back(c);
        if (escaped) escaped = false;
        else escaped = c == '\\';
    }
    return sanitizeSingleLine(jsonUnescape(raw));
}

int jsonInt(const std::string& json, const std::string& key, int fallback = 0) {
    std::size_t pos = jsonValueStart(json, key);
    if (pos == std::string::npos) return fallback;
    std::size_t end = pos;
    if (end < json.size() && (json[end] == '-' || json[end] == '+')) ++end;
    while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) ++end;
    if (end == pos) return fallback;
    try { return std::stoi(json.substr(pos, end - pos)); } catch (...) { return fallback; }
}

std::vector<std::string> jsonStringArray(const std::string& json, const std::string& key) {
    std::vector<std::string> out;
    std::size_t pos = jsonValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '[') return out;
    ++pos;
    while (pos < json.size()) {
        while (pos < json.size() && (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ',')) ++pos;
        if (pos >= json.size() || json[pos] == ']') break;
        if (json[pos] != '"') { ++pos; continue; }
        ++pos;
        std::string raw;
        bool escaped = false;
        for (; pos < json.size(); ++pos) {
            const char c = json[pos];
            if (!escaped && c == '"') { ++pos; break; }
            raw.push_back(c);
            if (escaped) escaped = false;
            else escaped = c == '\\';
        }
        std::string value = sanitizeSingleLine(jsonUnescape(raw));
        if (!value.empty()) out.push_back(value);
    }
    return out;
}

std::string resolveRelativeAsset(const fs::path& dir, const std::string& value) {
    if (value.empty()) return {};
    fs::path p(value);
    if (!p.is_absolute() && value.rfind("sdmc:/", 0) != 0) p = dir / p;
    std::error_code ec;
    if (fs::exists(p, ec) && fs::is_regular_file(p, ec)) return p.string();
    return {};
}

struct Metadata {
    std::string path;
    std::string title;
    std::string shortTitle;
    std::string author;
    std::string version;
    std::string baseGame;
    std::string description;
    std::string cover;
    std::string banner;
    std::vector<std::string> screenshots;
    std::vector<std::string> genres;
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
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i])) && suffixLetters < 3) { ++i; ++suffixLetters; }
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    if (digits >= 3 && i == s.size() && prefixLetters <= 3 && suffixLetters <= 2) return true;

    // Common dump/archive labels also use GBA product-code shaped names such as
    // A-BPEE e, B-AXVE, or A-XXXX u. Treat these as identifiers rather than titles.
    const auto dash = s.find('-');
    if (dash != std::string::npos) {
        std::string left = trim(s.substr(0, dash));
        std::string right = trim(s.substr(dash + 1));
        std::string compactRight;
        for (unsigned char c : right) if (std::isalnum(c)) compactRight.push_back(static_cast<char>(c));
        const bool shortPrefix = left.size() >= 1 && left.size() <= 2 &&
            std::all_of(left.begin(), left.end(), [](unsigned char c){ return std::isalpha(c) != 0; });
        const bool shortCode = compactRight.size() >= 4 && compactRight.size() <= 7 &&
            std::all_of(compactRight.begin(), compactRight.end(), [](unsigned char c){ return std::isalnum(c) != 0; });
        if (shortPrefix && shortCode) return true;
    }

    // Bare 4-character GBA game codes and small region suffixes are metadata, not display names.
    std::string compact;
    for (unsigned char c : s) if (std::isalnum(c)) compact.push_back(static_cast<char>(c));
    if (compact.size() >= 4 && compact.size() <= 6) {
        const bool allCodeChars = std::all_of(compact.begin(), compact.end(), [](unsigned char c){ return std::isalnum(c) != 0; });
        const int upperOrDigit = static_cast<int>(std::count_if(compact.begin(), compact.end(), [](unsigned char c){
            return std::isupper(c) || std::isdigit(c);
        }));
        if (allCodeChars && upperOrDigit >= static_cast<int>(compact.size()) - 1) return true;
    }

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
        g.title = g.automaticTitle;
        g.shortTitle = metadata.shortTitle.empty() ? g.title : metadata.shortTitle;
        g.author = metadata.author;
        g.version = metadata.version;
        g.baseGame = metadata.baseGame;
        g.description = metadata.description.empty() ? descriptionFromSidecar(dir) : metadata.description;
        g.genres = metadata.genres;
        g.tags = metadata.tags;
        g.releaseYear = metadata.releaseYear;
        dedupe(g.genres);
        dedupe(g.tags);

        if (auto overrideIt = titleOverrides.find(g.path); overrideIt != titleOverrides.end() && !overrideIt->second.empty()) {
            g.title = overrideIt->second;
            if (metadata.shortTitle.empty()) g.shortTitle = g.title;
            g.titleSource = "Custom override";
            g.customTitle = true;
        }

        if (auto st = state.find(g.path); st != state.end()) {
            g.favorite = st->second.favorite;
            g.hidden = st->second.hidden;
            g.completed = st->second.completed;
            g.launches = st->second.launches;
            g.playSeconds = st->second.playSeconds;
            g.lastPlayed = st->second.lastPlayed;
            g.addedAt = st->second.addedAt;
        }
        if (g.addedAt <= 0) g.addedAt = fileTimestamp(rom);
        games.emplace_back(std::move(g));
    };

    if (config.scanRecursively) {
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            add(*it);
        }
    } else {
        for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            add(*it);
        }
    }

    std::sort(games.begin(), games.end(), [](const Game& a, const Game& b) {
        return normalizeName(a.title) < normalizeName(b.title);
    });
    return games;
}

} // namespace advance
