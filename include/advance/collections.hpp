#pragma once

#include "advance/model.hpp"

#include <string>
#include <vector>

namespace advance {

std::vector<CustomCollection> loadCustomCollections(const std::string& path);
bool saveCustomCollections(const std::string& path, const std::vector<CustomCollection>& collections);
std::vector<Collection> buildCollections(const std::vector<Game>& games,
                                         const std::vector<CustomCollection>& customCollections,
                                         bool includeHiddenCollection = true);
bool collectionContains(const CustomCollection& collection, const std::string& romPath);
void setCollectionMembership(CustomCollection& collection, const std::string& romPath, bool member);

} // namespace advance
