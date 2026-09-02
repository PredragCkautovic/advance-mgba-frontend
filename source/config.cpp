#include "advance/config.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace advance {
namespace {

std::string trim(std::string s) {
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

bool parseBool(const std::string& v, bool fallback) {
    std::string x = v;
    std::transform(x.begin(), x.end(), x.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (x == "1" || x == "true" || x == "yes" || x == "on") return true;
    if (x == "0" || x == "false" || x == "no" || x == "off") return false;
    return fallback;
}

void autoDetect(Config& config) {
    std::error_code ec;
    if (!std::filesystem::exists(config.romDir, ec)) {
        const char* romCandidates[] = {
            "sdmc:/mGBA/Roms", "sdmc:/mGBA/roms", "sdmc:/roms/gba", "sdmc:/roms/GBA", "sdmc:/ROMs/GBA"
        };
        for (const char* candidate : romCandidates) {
            ec.clear();
            if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
                config.romDir = candidate; break;
            }
        }
    }
    ec.clear();
    if (!std::filesystem::exists(config.mgbaNro, ec)) {
        const char* mgbaCandidates[] = {"sdmc:/switch/mgba.nro", "sdmc:/switch/mgba/mgba.nro"};
        for (const char* candidate : mgbaCandidates) {
            ec.clear();
            if (std::filesystem::exists(candidate, ec) && std::filesystem::is_regular_file(candidate, ec)) {
                config.mgbaNro = candidate; break;
            }
        }
    }
}

std::string themeFromLegacyAccent(const std::string& accent) {
    if (accent == "purple") return "atomic-purple";
    if (accent == "blue") return "ice-blue";
    if (accent == "orange") return "midnight-gold";
    if (accent == "pink") return "neon-coral";
    return "crimson";
}

} // namespace

Config loadConfig(const std::string& path) {
    Config config;
    bool sawTheme = false;
    std::ifstream in(path);
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            const std::string key = trim(line.substr(0, pos));
            const std::string value = trim(line.substr(pos + 1));

            if (key == "rom_dir") config.romDir = value;
            else if (key == "cover_dir") config.coverDir = value;
            else if (key == "mgba_nro") config.mgbaNro = value;
            else if (key == "scan_recursively") config.scanRecursively = parseBool(value, config.scanRecursively);
            else if (key == "include_gb") config.includeGb = parseBool(value, config.includeGb);
            else if (key == "include_gbc") config.includeGbc = parseBool(value, config.includeGbc);
            else if (key == "columns") { try { config.columns = std::clamp(std::stoi(value), 4, 7); } catch (...) {} }
            else if (key == "rows") { try { config.rows = std::clamp(std::stoi(value), 2, 3); } catch (...) {} }
            else if (key == "theme") { config.theme = value; sawTheme = true; }
            else if (key == "accent") config.accent = value;
            else if (key == "use_account_profile") config.useAccountProfile = parseBool(value, config.useAccountProfile);
            else if (key == "show_system_status") config.showSystemStatus = parseBool(value, config.showSystemStatus);
            else if (key == "touch_enabled") config.touchEnabled = parseBool(value, config.touchEnabled);
            else if (key == "motion_enabled") config.motionEnabled = parseBool(value, config.motionEnabled);
            else if (key == "ui_sounds") config.uiSounds = parseBool(value, config.uiSounds);
            else if (key == "ui_volume") { try { config.uiVolume = std::clamp(std::stoi(value), 0, 100); } catch (...) {} }
            else if (key == "dynamic_backdrop") config.dynamicBackdrop = parseBool(value, config.dynamicBackdrop);
            else if (key == "backdrop_intensity") { try { config.backdropIntensity = std::clamp(std::stoi(value), 0, 100); } catch (...) {} }
            else if (key == "adaptive_accent") config.adaptiveAccent = parseBool(value, config.adaptiveAccent);
            else if (key == "screen_transitions") config.screenTransitions = parseBool(value, config.screenTransitions);
            else if (key == "launch_transition") config.launchTransition = parseBool(value, config.launchTransition);
            else if (key == "show_cover_labels") config.showCoverLabels = parseBool(value, config.showCoverLabels);
            else if (key == "show_hidden") config.showHidden = parseBool(value, config.showHidden);
            else if (key == "confirm_launch") config.confirmLaunch = parseBool(value, config.confirmLaunch);
        }
    }
    if (!sawTheme) config.theme = themeFromLegacyAccent(config.accent);
    autoDetect(config);
    return config;
}

bool saveConfig(const std::string& path, const Config& config) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << "# Advance 0.1 - premium mGBA library frontend\n"
        << "# Paths use libnx sdmc:/ notation. Existing 2.x configs are migrated automatically.\n\n"
        << "[library]\n"
        << "rom_dir=" << config.romDir << "\n"
        << "cover_dir=" << config.coverDir << "\n"
        << "scan_recursively=" << (config.scanRecursively ? "true" : "false") << "\n"
        << "include_gb=" << (config.includeGb ? "true" : "false") << "\n"
        << "include_gbc=" << (config.includeGbc ? "true" : "false") << "\n"
        << "show_hidden=" << (config.showHidden ? "true" : "false") << "\n\n"
        << "[emulator]\n"
        << "mgba_nro=" << config.mgbaNro << "\n"
        << "confirm_launch=" << (config.confirmLaunch ? "true" : "false") << "\n\n"
        << "[ui]\n"
        << "columns=" << config.columns << "\n"
        << "rows=" << config.rows << "\n"
        << "theme=" << config.theme << "\n"
        << "use_account_profile=" << (config.useAccountProfile ? "true" : "false") << "\n"
        << "show_system_status=" << (config.showSystemStatus ? "true" : "false") << "\n"
        << "touch_enabled=" << (config.touchEnabled ? "true" : "false") << "\n"
        << "motion_enabled=" << (config.motionEnabled ? "true" : "false") << "\n"
        << "ui_sounds=" << (config.uiSounds ? "true" : "false") << "\n"
        << "ui_volume=" << config.uiVolume << "\n"
        << "dynamic_backdrop=" << (config.dynamicBackdrop ? "true" : "false") << "\n"
        << "backdrop_intensity=" << config.backdropIntensity << "\n"
        << "show_cover_labels=" << (config.showCoverLabels ? "true" : "false") << "\n";
    return true;
}

bool saveDefaultConfig(const std::string& path, const Config& config) { return saveConfig(path, config); }

} // namespace advance
