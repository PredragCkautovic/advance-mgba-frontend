#include "advance/state.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace advance {

StateMap loadState(const std::string& path) {
    StateMap result;
    std::ifstream in(path);
    if (!in) return result;

    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> fields;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, '\t')) fields.push_back(field);
        if (fields.size() < 4) continue;

        GameState st;
        st.favorite = fields[1] == "1";
        try { st.launches = static_cast<std::uint32_t>(std::stoul(fields[2])); } catch (...) {}
        try { st.lastPlayed = static_cast<std::int64_t>(std::stoll(fields[3])); } catch (...) {}
        // Advance 3.x fields. Old 2.x state files remain valid.
        if (fields.size() >= 5) st.hidden = fields[4] == "1";
        if (fields.size() >= 6) st.completed = fields[5] == "1";
        if (fields.size() >= 7) { try { st.playSeconds = static_cast<std::uint64_t>(std::stoull(fields[6])); } catch (...) {} }
        if (fields.size() >= 8) { try { st.addedAt = static_cast<std::int64_t>(std::stoll(fields[7])); } catch (...) {} }
        result[fields[0]] = st;
    }
    return result;
}

bool saveState(const std::string& path, const std::vector<Game>& games) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    for (const auto& g : games) {
        // Persist every discovered game once addedAt is known so recently-added
        // ordering remains stable across rescans and SD-card timestamp changes.
        out << g.path << '\t'
            << (g.favorite ? 1 : 0) << '\t'
            << g.launches << '\t'
            << g.lastPlayed << '\t'
            << (g.hidden ? 1 : 0) << '\t'
            << (g.completed ? 1 : 0) << '\t'
            << g.playSeconds << '\t'
            << g.addedAt << '\n';
    }
    return true;
}

PendingSession loadPendingSession(const std::string& path) {
    PendingSession result;
    std::ifstream in(path);
    if (!in) return result;
    std::getline(in, result.romPath);
    std::string started;
    std::getline(in, started);
    try { result.startedAt = static_cast<std::int64_t>(std::stoll(started)); } catch (...) { result.startedAt = 0; }
    return result;
}

bool savePendingSession(const std::string& path, const PendingSession& session) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << session.romPath << '\n' << session.startedAt << '\n';
    return true;
}

void clearPendingSession(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

bool reconcilePendingSession(StateMap& state, const std::string& pendingPath,
                             std::int64_t now, std::uint64_t maxSessionSeconds) {
    const PendingSession pending = loadPendingSession(pendingPath);
    if (pending.romPath.empty() || pending.startedAt <= 0 || now <= pending.startedAt) {
        clearPendingSession(pendingPath);
        return false;
    }
    std::uint64_t elapsed = static_cast<std::uint64_t>(now - pending.startedAt);
    elapsed = std::min(elapsed, maxSessionSeconds);
    // Ignore tiny accidental relaunches; everything else contributes to the
    // user's play-time history when Advance is opened after mGBA.
    if (elapsed >= 15) state[pending.romPath].playSeconds += elapsed;
    clearPendingSession(pendingPath);
    return elapsed >= 15;
}

} // namespace advance
