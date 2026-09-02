#include "advance/collections.hpp"
#include "advance/library.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace advance {
namespace fs = std::filesystem;
namespace {

std::string clean(std::string value) {
    for (char& c : value) if (c == '\t' || c == '\r' || c == '\n') c = ' ';
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());
    while (!value.empty() && value.back() == ' ') value.pop_back();
    return value;
}

bool containsNormalized(const std::string& haystack, const std::string& needle) {
    return normalizeName(haystack).find(normalizeName(needle)) != std::string::npos;
}

template <typename Predicate>
void addIf(Collection& c, const std::vector<Game>& games, const Predicate& predicate) {
    for (std::size_t i = 0; i < games.size(); ++i) if (predicate(games[i])) c.games.push_back(i);
}

} // namespace

std::vector<CustomCollection> loadCustomCollections(const std::string& path) {
    std::vector<CustomCollection> result;
    std::ifstream in(path);
    std::string line;
    while (in && std::getline(in, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string name = clean(line.substr(0, tab));
        const std::string rom = clean(line.substr(tab + 1));
        if (name.empty() || rom.empty()) continue;
        auto it = std::find_if(result.begin(), result.end(), [&](const CustomCollection& c) { return c.name == name; });
        if (it == result.end()) result.push_back(CustomCollection{name, {rom}});
        else if (std::find(it->romPaths.begin(), it->romPaths.end(), rom) == it->romPaths.end()) it->romPaths.push_back(rom);
    }
    std::sort(result.begin(), result.end(), [](const CustomCollection& a, const CustomCollection& b) {
        return normalizeName(a.name) < normalizeName(b.name);
    });
    return result;
}

bool saveCustomCollections(const std::string& path, const std::vector<CustomCollection>& collections) {
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    for (const auto& collection : collections) {
        const std::string name = clean(collection.name);
        if (name.empty()) continue;
        std::vector<std::string> rows = collection.romPaths;
        std::sort(rows.begin(), rows.end());
        rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
        for (const auto& rom : rows) if (!rom.empty()) out << name << '\t' << rom << '\n';
    }
    return true;
}

bool collectionContains(const CustomCollection& collection, const std::string& romPath) {
    return std::find(collection.romPaths.begin(), collection.romPaths.end(), romPath) != collection.romPaths.end();
}

void setCollectionMembership(CustomCollection& collection, const std::string& romPath, bool member) {
    auto it = std::find(collection.romPaths.begin(), collection.romPaths.end(), romPath);
    if (member) {
        if (it == collection.romPaths.end()) collection.romPaths.push_back(romPath);
    } else if (it != collection.romPaths.end()) collection.romPaths.erase(it);
}

std::vector<Collection> buildCollections(const std::vector<Game>& games,
                                         const std::vector<CustomCollection>& customCollections,
                                         bool includeHiddenCollection) {
    std::vector<Collection> out;

    Collection favorites{"favorites", "Favorites", "Games you starred", true, {}};
    addIf(favorites, games, [](const Game& g) { return g.favorite && !g.hidden; });
    out.push_back(std::move(favorites));

    Collection completed{"completed", "Completed", "Games you finished", true, {}};
    addIf(completed, games, [](const Game& g) { return g.completed && !g.hidden; });
    out.push_back(std::move(completed));

    Collection neverPlayed{"never-played", "Never Played", "Fresh picks from your library", true, {}};
    addIf(neverPlayed, games, [](const Game& g) { return g.launches == 0 && !g.hidden; });
    out.push_back(std::move(neverPlayed));

    Collection pokemon{"pokemon", "Pokémon", "Pokémon games and ROM hacks", true, {}};
    addIf(pokemon, games, [](const Game& g) {
        const std::string hay = searchableText(g);
        return (hay.find("pokemon") != std::string::npos || hay.find("pokémon") != std::string::npos) && !g.hidden;
    });
    if (!pokemon.games.empty()) out.push_back(std::move(pokemon));

    Collection rpg{"rpg", "RPG", "Role-playing adventures", true, {}};
    addIf(rpg, games, [](const Game& g) {
        if (g.hidden) return false;
        for (const auto& genre : g.genres) if (containsNormalized(genre, "rpg") || containsNormalized(genre, "role playing")) return true;
        for (const auto& tag : g.tags) if (containsNormalized(tag, "rpg")) return true;
        return false;
    });
    if (!rpg.games.empty()) out.push_back(std::move(rpg));

    Collection hacks{"rom-hacks", "ROM Hacks", "Fan-made adventures and enhancements", true, {}};
    addIf(hacks, games, [](const Game& g) {
        if (g.hidden) return false;
        const std::string hay = searchableText(g);
        if (!g.baseGame.empty()) return true;
        return hay.find("rom hack") != std::string::npos || hay.find("romhack") != std::string::npos ||
               hay.find("hack") != std::string::npos || hay.find("recreated") != std::string::npos;
    });
    if (!hacks.games.empty()) out.push_back(std::move(hacks));

    Collection recent{"recently-added", "Recently Added", "Newest games on this SD card", true, {}};
    addIf(recent, games, [](const Game& g) { return !g.hidden; });
    std::stable_sort(recent.games.begin(), recent.games.end(), [&](std::size_t a, std::size_t b) {
        return games[a].addedAt > games[b].addedAt;
    });
    if (recent.games.size() > 30) recent.games.resize(30);
    out.push_back(std::move(recent));

    if (includeHiddenCollection) {
        Collection hidden{"hidden", "Hidden", "Games hidden from your main library", true, {}};
        addIf(hidden, games, [](const Game& g) { return g.hidden; });
        if (!hidden.games.empty()) out.push_back(std::move(hidden));
    }

    for (const auto& custom : customCollections) {
        Collection c;
        c.key = "custom:" + normalizeName(custom.name);
        c.name = custom.name;
        c.description = "Your custom collection";
        c.automatic = false;
        std::unordered_set<std::string> members(custom.romPaths.begin(), custom.romPaths.end());
        for (std::size_t i = 0; i < games.size(); ++i) if (members.count(games[i].path)) c.games.push_back(i);
        out.push_back(std::move(c));
    }

    return out;
}

} // namespace advance
