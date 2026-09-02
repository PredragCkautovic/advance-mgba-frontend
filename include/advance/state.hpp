#pragma once

#include "advance/library.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace advance {

StateMap loadState(const std::string& path);
bool saveState(const std::string& path, const std::vector<Game>& games);

struct PendingSession {
    std::string romPath;
    std::int64_t startedAt{0};
};

PendingSession loadPendingSession(const std::string& path);
bool savePendingSession(const std::string& path, const PendingSession& session);
void clearPendingSession(const std::string& path);
bool reconcilePendingSession(StateMap& state, const std::string& pendingPath,
                             std::int64_t now, std::uint64_t maxSessionSeconds = 12 * 60 * 60);

} // namespace advance
