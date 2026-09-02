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
