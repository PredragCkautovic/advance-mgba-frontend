#pragma once

#include "advance/model.hpp"
#include <string>

namespace advance {

Config loadConfig(const std::string& path);
bool saveConfig(const std::string& path, const Config& config);
bool saveDefaultConfig(const std::string& path, const Config& config);

} // namespace advance
