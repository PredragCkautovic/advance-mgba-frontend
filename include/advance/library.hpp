#pragma once

#include "advance/model.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace advance {

using StateMap = std::unordered_map<std::string, GameState>;
using TitleOverrideMap = std::unordered_map<std::string, std::string>;

std::vector<Game> scanLibrary(const Config& config, const StateMap& state, const TitleOverrideMap& titleOverrides);
std::string normalizeName(const std::string& value);
std::string prettyTitleFromFilename(const std::string& filename);
std::string searchableText(const Game& game);
bool gameMatchesQuery(const Game& game, const std::string& query);

TitleOverrideMap loadTitleOverrides(const std::string& path);
bool saveTitleOverrides(const std::string& path, const TitleOverrideMap& overrides);

} // namespace advance
