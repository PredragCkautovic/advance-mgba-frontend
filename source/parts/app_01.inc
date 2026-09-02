#include "advance/app.hpp"

#include "advance/config.hpp"
#include "advance/state.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <array>
#include <ctime>
#include <filesystem>
#include <random>

namespace advance {

namespace {
constexpr std::size_t npos = static_cast<std::size_t>(-1);
}

void App::ensureDataFolders() {
    std::error_code ec;
    std::filesystem::create_directories("sdmc:/switch/advance/covers", ec);
}

int App::run() {
    ensureDataFolders();
    if (!std::filesystem::exists(kConfigPath)) saveDefaultConfig(kConfigPath, Config{});
    config_ = loadConfig(kConfigPath);
    titleOverrides_ = loadTitleOverrides(kTitlesPath);
    customCollections_ = loadCustomCollections(kCollectionsPath);
    randomSeed_ = static_cast<std::uint64_t>(std::time(nullptr)) ^ SDL_GetTicks64();

    if (!renderer_.init(config_)) return 2;
    renderer_.beginFrame();
    renderer_.renderBoot(config_, "Building your Advance library…");
    renderer_.endFrame();

    rescan(false);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_);
    hidInitializeTouchScreen();

    while (running_ && appletMainLoop()) {
        updateInput();
        render();
    }

    persistState();
    return 0;
}

void App::rescan(bool preserveSelection) {
    std::string selectedPath;
    if (preserveSelection) {
        if (const Game* g = selectedGame()) selectedPath = g->path;
    }

    auto state = loadState(kStatePath);
    reconcilePendingSession(state, kPendingSessionPath, static_cast<std::int64_t>(std::time(nullptr)));
    titleOverrides_ = loadTitleOverrides(kTitlesPath);
    customCollections_ = loadCustomCollections(kCollectionsPath);
    games_ = scanLibrary(config_, state, titleOverrides_);
    rebuildDerived();
    persistState();

    selected_ = 0;
    if (!selectedPath.empty()) syncLibrarySelectionToPath(selectedPath);
    clampSelection();
}

void App::rebuildDerived() {
    rebuildCollections();
    rebuildHomeRows();
    rebuildVisible();
}

void App::rebuildCollections() {
    collections_ = buildCollections(games_, customCollections_, true);
    if (collectionSelected_ >= collections_.size()) collectionSelected_ = collections_.empty() ? 0 : collections_.size() - 1;
}

void App::rebuildHomeRows() {
    const std::string selectedPath = homeSelectedGame() ? homeSelectedGame()->path : std::string{};
    homeRows_.clear();

    auto sorted = [&](auto predicate, auto compare, std::size_t limit, const std::string& key,
                      const std::string& title, const std::string& subtitle) {
        HomeRow row{key, title, subtitle, {}};
        for (std::size_t i = 0; i < games_.size(); ++i) if (!games_[i].hidden && predicate(games_[i])) row.games.push_back(i);
        std::stable_sort(row.games.begin(), row.games.end(), [&](std::size_t a, std::size_t b) { return compare(games_[a], games_[b]); });
        if (limit && row.games.size() > limit) row.games.resize(limit);
        if (!row.games.empty()) homeRows_.push_back(std::move(row));
    };

    sorted([](const Game& g) { return g.lastPlayed > 0; },
           [](const Game& a, const Game& b) { return a.lastPlayed > b.lastPlayed; },
           16, "continue", "Continue Playing", "Jump back into your latest adventures");

    sorted([](const Game&) { return true; },
           [](const Game& a, const Game& b) { return a.addedAt > b.addedAt; },
           18, "recently-added", "Recently Added", "The newest games discovered on your SD card");

    sorted([](const Game& g) { return g.favorite; },
           [](const Game& a, const Game& b) { return a.lastPlayed > b.lastPlayed; },
           18, "favorites", "Favorites", "Your hand-picked shelf");

    sorted([](const Game& g) { return g.launches > 0; },
           [](const Game& a, const Game& b) {
               if (a.playSeconds != b.playSeconds) return a.playSeconds > b.playSeconds;
               return a.launches > b.launches;
           },
           18, "most-played", "Most Played", "The games that keep pulling you back");

    for (const auto& c : collections_) {
        if (c.key != "pokemon" && c.key != "rom-hacks" && c.key != "completed") continue;
        HomeRow row{c.key, c.name, c.description, {}};
        for (auto idx : c.games) if (idx < games_.size() && !games_[idx].hidden) row.games.push_back(idx);
        if (row.games.size() > 18) row.games.resize(18);
        if (!row.games.empty()) homeRows_.push_back(std::move(row));
    }

    if (homeRows_.empty()) {
        HomeRow all{"all", "Your Library", "Add games to get started", {}};
        for (std::size_t i = 0; i < games_.size(); ++i) if (!games_[i].hidden) all.games.push_back(i);
        if (!all.games.empty()) homeRows_.push_back(std::move(all));
    }

    if (homeRow_ >= homeRows_.size()) homeRow_ = 0;
    homeColumns_.resize(homeRows_.size(), 0);
    if (!selectedPath.empty()) {
        for (std::size_t r = 0; r < homeRows_.size(); ++r) {
            for (std::size_t c = 0; c < homeRows_[r].games.size(); ++c) {
                if (games_[homeRows_[r].games[c]].path == selectedPath) {
                    homeRow_ = r;
                    homeColumns_[r] = c;
                    return;
