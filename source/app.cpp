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
                }
            }
        }
    }
}

void App::rebuildVisible() {
    std::string selectedPath;
    if (selected_ < visible_.size() && visible_[selected_] < games_.size()) selectedPath = games_[visible_[selected_]].path;

    visible_.clear();
    if (shelf_ == Shelf::Collection) {
        auto it = std::find_if(collections_.begin(), collections_.end(), [&](const Collection& c) { return c.key == activeCollectionKey_; });
        if (it != collections_.end()) {
            for (auto idx : it->games) if (idx < games_.size()) visible_.push_back(idx);
        }
    } else {
        for (std::size_t i = 0; i < games_.size(); ++i) {
            const auto& g = games_[i];
            if (g.hidden && !config_.showHidden && shelf_ != Shelf::Hidden) continue;
            if (shelf_ == Shelf::Favorites && !g.favorite) continue;
            if (shelf_ == Shelf::Recent && g.lastPlayed <= 0) continue;
            if (shelf_ == Shelf::Completed && !g.completed) continue;
            if (shelf_ == Shelf::Hidden && !g.hidden) continue;
            if (shelf_ == Shelf::Search && !gameMatchesQuery(g, searchQuery_)) continue;
            visible_.push_back(i);
        }
    }

    auto byTitle = [&](std::size_t a, std::size_t b) { return normalizeName(games_[a].title) < normalizeName(games_[b].title); };
    switch (sortMode_) {
        case SortMode::Recent:
            std::stable_sort(visible_.begin(), visible_.end(), [&](std::size_t a, std::size_t b) {
                if (games_[a].lastPlayed != games_[b].lastPlayed) return games_[a].lastPlayed > games_[b].lastPlayed;
                return byTitle(a, b);
            }); break;
        case SortMode::Launches:
            std::stable_sort(visible_.begin(), visible_.end(), [&](std::size_t a, std::size_t b) {
                if (games_[a].launches != games_[b].launches) return games_[a].launches > games_[b].launches;
                return byTitle(a, b);
            }); break;
        case SortMode::PlayTime:
            std::stable_sort(visible_.begin(), visible_.end(), [&](std::size_t a, std::size_t b) {
                if (games_[a].playSeconds != games_[b].playSeconds) return games_[a].playSeconds > games_[b].playSeconds;
                return byTitle(a, b);
            }); break;
        case SortMode::Added:
            std::stable_sort(visible_.begin(), visible_.end(), [&](std::size_t a, std::size_t b) {
                if (games_[a].addedAt != games_[b].addedAt) return games_[a].addedAt > games_[b].addedAt;
                return byTitle(a, b);
            }); break;
        case SortMode::Favorites:
            std::stable_sort(visible_.begin(), visible_.end(), [&](std::size_t a, std::size_t b) {
                if (games_[a].favorite != games_[b].favorite) return games_[a].favorite > games_[b].favorite;
                return byTitle(a, b);
            }); break;
        default:
            std::stable_sort(visible_.begin(), visible_.end(), byTitle); break;
    }

    if (!selectedPath.empty()) {
        for (std::size_t i = 0; i < visible_.size(); ++i) if (games_[visible_[i]].path == selectedPath) { selected_ = i; break; }
    }
    clampSelection();
}

void App::clampSelection() {
    if (visible_.empty()) selected_ = 0;
    else if (selected_ >= visible_.size()) selected_ = visible_.size() - 1;
    if (homeRow_ >= homeRows_.size()) homeRow_ = 0;
    if (homeColumns_.size() < homeRows_.size()) homeColumns_.resize(homeRows_.size(), 0);
    if (!homeRows_.empty() && !homeRows_[homeRow_].games.empty()) {
        homeColumns_[homeRow_] = std::min(homeColumns_[homeRow_], homeRows_[homeRow_].games.size() - 1);
    }
}

bool App::repeatPressed(std::uint64_t button, std::uint64_t down, std::uint64_t held) {
    const std::uint64_t now = SDL_GetTicks64();
    if (down & button) {
        navHeldMask_ = button;
        nextNavRepeat_ = now + 260;
        return true;
    }
    if ((held & button) && navHeldMask_ == button && now >= nextNavRepeat_) {
        nextNavRepeat_ = now + 68;
        return true;
    }
    if (!(held & navHeldMask_)) navHeldMask_ = 0;
    return false;
}

void App::updateInput() {
    padUpdate(&pad_);
    const std::uint64_t down = padGetButtonsDown(&pad_);
    const std::uint64_t held = padGetButtons(&pad_);
    if (!toast_.empty() && SDL_GetTicks64() >= toastUntil_) toast_.clear();
    if (!launchConfirmPath_.empty() && SDL_GetTicks64() > launchConfirmUntil_) launchConfirmPath_.clear();

    if (sidebarFocused_) {
        handleSidebarInput(down, held);
    } else {
        switch (screen_) {
            case Screen::Home: handleHomeInput(down, held); break;
            case Screen::Library: handleLibraryInput(down, held); break;
            case Screen::Collections: handleCollectionsInput(down, held); break;
            case Screen::Details: handleDetailsInput(down); break;
            case Screen::CollectionPicker: handleCollectionPickerInput(down); break;
            case Screen::Settings: handleSettingsInput(down); break;
            case Screen::About: handleAboutInput(down); break;
        }
    }
    if (config_.touchEnabled) updateTouch();
}

void App::handleHomeInput(std::uint64_t down, std::uint64_t held) {
    if ((down & HidNpadButton_AnyLeft) && currentHomeColumn() == 0) { focusSidebar(); return; }
    if (repeatPressed(HidNpadButton_AnyLeft, down, held)) navigateHome(-1, 0);
    if (repeatPressed(HidNpadButton_AnyRight, down, held)) navigateHome(1, 0);
    if (repeatPressed(HidNpadButton_AnyUp, down, held)) navigateHome(0, -1);
    if (repeatPressed(HidNpadButton_AnyDown, down, held)) navigateHome(0, 1);

    if (down & HidNpadButton_A) launchSelected();
    if (down & HidNpadButton_X) {
        if (const Game* g = homeSelectedGame()) { detailGamePath_ = g->path; syncLibrarySelectionToPath(g->path); previousScreen_ = Screen::Home; screen_ = Screen::Details; renderer_.playSound(UiSound::Confirm, config_); }
    }
    if (down & HidNpadButton_Y) toggleFavorite();
    if (down & HidNpadButton_R) randomPick();
    if (down & HidNpadButton_L) setScreen(Screen::Library);
    if (down & HidNpadButton_Minus) openSearch();
    if (down & HidNpadButton_Plus) setScreen(Screen::Settings);
    if (down & HidNpadButton_B) { renderer_.playSound(UiSound::Back, config_); running_ = false; }
}

void App::handleLibraryInput(std::uint64_t down, std::uint64_t held) {
    const int cols = std::clamp(config_.columns, 4, 7);
    if ((down & HidNpadButton_AnyLeft) && (visible_.empty() || selected_ % static_cast<std::size_t>(cols) == 0)) { focusSidebar(); return; }
    if (repeatPressed(HidNpadButton_AnyLeft, down, held)) navigateLibrary(-1, 0);
    if (repeatPressed(HidNpadButton_AnyRight, down, held)) navigateLibrary(1, 0);
    if (repeatPressed(HidNpadButton_AnyUp, down, held)) navigateLibrary(0, -1);
    if (repeatPressed(HidNpadButton_AnyDown, down, held)) navigateLibrary(0, 1);

    if (down & HidNpadButton_A) launchSelected();
    if (down & HidNpadButton_X) { if (selectedGame()) { detailGamePath_ = selectedGame()->path; previousScreen_ = Screen::Library; screen_ = Screen::Details; renderer_.playSound(UiSound::Confirm, config_); } }
    if (down & HidNpadButton_Y) cycleSort();
    if (down & HidNpadButton_L) setShelf(Shelf::Favorites);
    if (down & HidNpadButton_R) setShelf(Shelf::Recent);
    if (down & HidNpadButton_ZL) changePage(-1);
    if (down & HidNpadButton_ZR) changePage(1);
    if (down & HidNpadButton_Minus) openSearch();
    if (down & HidNpadButton_Plus) setScreen(Screen::Settings);
    if (down & HidNpadButton_B) setScreen(Screen::Home);
}

void App::handleCollectionsInput(std::uint64_t down, std::uint64_t held) {
    if ((down & HidNpadButton_AnyLeft) && (collections_.empty() || collectionSelected_ % 3 == 0)) { focusSidebar(); return; }
    if (collections_.empty()) {
        if (down & HidNpadButton_X) createCollection();
        if (down & HidNpadButton_B) setScreen(Screen::Home);
        return;
    }
    constexpr int cols = 3;
    if (repeatPressed(HidNpadButton_AnyLeft, down, held) && collectionSelected_ > 0) { --collectionSelected_; renderer_.playSound(UiSound::Navigate, config_); }
    if (repeatPressed(HidNpadButton_AnyRight, down, held) && collectionSelected_ + 1 < collections_.size()) { ++collectionSelected_; renderer_.playSound(UiSound::Navigate, config_); }
    if (repeatPressed(HidNpadButton_AnyUp, down, held) && collectionSelected_ >= cols) { collectionSelected_ -= cols; renderer_.playSound(UiSound::Navigate, config_); }
    if (repeatPressed(HidNpadButton_AnyDown, down, held)) {
        const std::size_t next = collectionSelected_ + cols;
        if (next < collections_.size()) collectionSelected_ = next;
        else if (collectionSelected_ / cols != (collections_.size() - 1) / cols) collectionSelected_ = collections_.size() - 1;
        renderer_.playSound(UiSound::Navigate, config_);
    }
    if (down & HidNpadButton_A) if (const Collection* c = selectedCollection()) openCollection(*c);
    if (down & HidNpadButton_X) createCollection();
    if (down & HidNpadButton_R) randomPick();
    if (down & HidNpadButton_Plus) setScreen(Screen::Settings);
    if (down & HidNpadButton_B) setScreen(Screen::Home);
}

void App::handleDetailsInput(std::uint64_t down) {
    if (down & HidNpadButton_AnyLeft) { focusSidebar(); return; }
    if (down & HidNpadButton_B) { screen_ = previousScreen_; renderer_.playSound(UiSound::Back, config_); return; }
    if (down & HidNpadButton_A) launchSelected();
    if (down & HidNpadButton_Y) toggleFavorite();
    if (down & HidNpadButton_X) editSelectedTitle();
    if (down & HidNpadButton_R) { pickerSelected_ = 0; screen_ = Screen::CollectionPicker; renderer_.playSound(UiSound::Confirm, config_); }
    if (down & HidNpadButton_ZR) toggleCompleted();
    if (down & HidNpadButton_ZL) toggleHidden();
    if (down & HidNpadButton_Minus) resetSelectedTitle();
}

void App::handleCollectionPickerInput(std::uint64_t down) {
    if (down & HidNpadButton_AnyLeft) { focusSidebar(); return; }
    if (down & HidNpadButton_B) { screen_ = Screen::Details; renderer_.playSound(UiSound::Back, config_); return; }
    if (down & HidNpadButton_AnyUp) {
        if (!customCollections_.empty()) pickerSelected_ = pickerSelected_ == 0 ? customCollections_.size() - 1 : pickerSelected_ - 1;
        renderer_.playSound(UiSound::Navigate, config_);
    }
    if (down & HidNpadButton_AnyDown) {
        if (!customCollections_.empty()) pickerSelected_ = (pickerSelected_ + 1) % customCollections_.size();
        renderer_.playSound(UiSound::Navigate, config_);
    }
    if (down & HidNpadButton_A) togglePickerMembership();
    if (down & HidNpadButton_X) createCollection();
}

void App::handleSettingsInput(std::uint64_t down) {
    constexpr std::size_t kSettingsCount = 17;
    if (down & HidNpadButton_B) { persistConfig(); setScreen(previousScreen_ == Screen::Settings ? Screen::Home : previousScreen_); return; }
    if (down & HidNpadButton_AnyUp) { settingsSelected_ = settingsSelected_ == 0 ? kSettingsCount - 1 : settingsSelected_ - 1; renderer_.playSound(UiSound::Navigate, config_); }
    if (down & HidNpadButton_AnyDown) { settingsSelected_ = (settingsSelected_ + 1) % kSettingsCount; renderer_.playSound(UiSound::Navigate, config_); }
    if (down & HidNpadButton_AnyLeft) changeSetting(-1);
    if (down & HidNpadButton_AnyRight) changeSetting(1);
    if (down & HidNpadButton_A) activateSetting();
    if (down & HidNpadButton_X) { rescan(true); showToast("Library refreshed"); }
    if (down & HidNpadButton_R) { previousScreen_ = Screen::Settings; screen_ = Screen::About; renderer_.playSound(UiSound::Confirm, config_); }
}

void App::handleAboutInput(std::uint64_t down) {
    if (down & HidNpadButton_B || down & HidNpadButton_A) { screen_ = Screen::Settings; renderer_.playSound(UiSound::Back, config_); }
}

int App::activeSidebarIndex() const {
    if (screen_ == Screen::Home || ((screen_ == Screen::Details || screen_ == Screen::CollectionPicker) && previousScreen_ == Screen::Home)) return 0;
    if (screen_ == Screen::Collections || ((screen_ == Screen::Details || screen_ == Screen::CollectionPicker) && previousScreen_ == Screen::Collections)) return 2;
    if (screen_ == Screen::Settings || screen_ == Screen::About) return 6;
    if (shelf_ == Shelf::Favorites) return 3;
    if (shelf_ == Shelf::Recent) return 4;
    if (shelf_ == Shelf::Search) return 5;
    return 1;
}

void App::focusSidebar() {
    sidebarSelected_ = activeSidebarIndex();
    sidebarFocused_ = true;
    navHeldMask_ = 0;
    renderer_.playSound(UiSound::Page, config_);
}

void App::closeSidebar() {
    sidebarFocused_ = false;
    navHeldMask_ = 0;
    renderer_.playSound(UiSound::Back, config_);
}

void App::activateSidebarItem(int index) {
    sidebarSelected_ = std::clamp(index, 0, 6);
    sidebarFocused_ = false;
    switch (sidebarSelected_) {
        case 0: setScreen(Screen::Home); break;
        case 1: setShelf(Shelf::All); break;
        case 2: setScreen(Screen::Collections); break;
        case 3: setShelf(Shelf::Favorites); break;
        case 4: setShelf(Shelf::Recent); break;
        case 5: openSearch(); break;
        case 6: setScreen(Screen::Settings); break;
        default: break;
    }
}

void App::handleSidebarInput(std::uint64_t down, std::uint64_t held) {
    if (repeatPressed(HidNpadButton_AnyUp, down, held)) {
        sidebarSelected_ = sidebarSelected_ == 0 ? 6 : sidebarSelected_ - 1;
        renderer_.playSound(UiSound::Navigate, config_);
    }
    if (repeatPressed(HidNpadButton_AnyDown, down, held)) {
        sidebarSelected_ = (sidebarSelected_ + 1) % 7;
        renderer_.playSound(UiSound::Navigate, config_);
    }
    if (down & HidNpadButton_A) { activateSidebarItem(sidebarSelected_); return; }
    if (down & (HidNpadButton_B | HidNpadButton_AnyRight)) { closeSidebar(); return; }
}

void App::navigateLibrary(int dx, int dy) {
    if (visible_.empty()) return;
    const std::size_t before = selected_;
    const int cols = std::clamp(config_.columns, 4, 7);
    if (dx < 0 && selected_ > 0) --selected_;
    else if (dx > 0 && selected_ + 1 < visible_.size()) ++selected_;
    else if (dy < 0 && selected_ >= static_cast<std::size_t>(cols)) selected_ -= cols;
    else if (dy > 0) {
        if (selected_ + cols < visible_.size()) selected_ += cols;
        else if (selected_ / cols != (visible_.size() - 1) / cols) selected_ = visible_.size() - 1;
    }
    if (before != selected_) renderer_.playSound(UiSound::Navigate, config_);
}

void App::navigateHome(int dx, int dy) {
    if (homeRows_.empty()) return;
    const std::size_t beforeRow = homeRow_;
    const std::size_t beforeCol = currentHomeColumn();
    if (dy < 0 && homeRow_ > 0) --homeRow_;
    else if (dy > 0 && homeRow_ + 1 < homeRows_.size()) ++homeRow_;
    if (homeColumns_.size() < homeRows_.size()) homeColumns_.resize(homeRows_.size(), 0);
    if (!homeRows_[homeRow_].games.empty()) {
        auto& col = homeColumns_[homeRow_];
        if (dx < 0 && col > 0) --col;
        else if (dx > 0 && col + 1 < homeRows_[homeRow_].games.size()) ++col;
        col = std::min(col, homeRows_[homeRow_].games.size() - 1);
    }
    if (beforeRow != homeRow_ || beforeCol != currentHomeColumn()) renderer_.playSound(UiSound::Navigate, config_);
}

void App::changePage(int direction) {
    if (visible_.empty()) return;
    const std::size_t perPage = static_cast<std::size_t>(std::clamp(config_.columns, 4, 7) * std::clamp(config_.rows, 2, 3));
    const std::size_t pages = (visible_.size() + perPage - 1) / perPage;
    if (pages <= 1) return;
    std::size_t page = selected_ / perPage;
    page = direction < 0 ? (page == 0 ? pages - 1 : page - 1) : (page + 1) % pages;
    selected_ = std::min(page * perPage, visible_.size() - 1);
    renderer_.playSound(UiSound::Page, config_);
}

void App::setScreen(Screen screen) {
    if (screen_ == screen) return;
    previousScreen_ = screen_;
    screen_ = screen;
    renderer_.playSound(UiSound::Page, config_);
}

void App::setShelf(Shelf shelf) {
    shelf_ = shelf;
    activeCollectionKey_.clear();
    activeCollectionName_.clear();
    if (shelf_ == Shelf::Recent) sortMode_ = SortMode::Recent;
    rebuildVisible();
    selected_ = 0;
    screen_ = Screen::Library;
    renderer_.playSound(UiSound::Page, config_);
}

void App::openCollection(const Collection& collection) {
    shelf_ = Shelf::Collection;
    activeCollectionKey_ = collection.key;
    activeCollectionName_ = collection.name;
    sortMode_ = collection.key == "recently-added" ? SortMode::Added : SortMode::Title;
    rebuildVisible();
    selected_ = 0;
    previousScreen_ = Screen::Collections;
    screen_ = Screen::Library;
    renderer_.playSound(UiSound::Confirm, config_);
}

void App::cycleSort() {
    sortMode_ = static_cast<SortMode>((static_cast<int>(sortMode_) + 1) % 6);
    rebuildVisible();
    renderer_.playSound(UiSound::Page, config_);
}

void App::openSearch() {
    SwkbdConfig kbd{};
    if (R_FAILED(swkbdCreate(&kbd, 0))) { showToast("Search keyboard unavailable", 2600, UiSound::Error); return; }
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, "Search Advance");
    swkbdConfigSetSubText(&kbd, "Search title, author, tags, genre, base game, or folder.");
    swkbdConfigSetOkButtonText(&kbd, "Search");
    if (!searchQuery_.empty()) swkbdConfigSetInitialText(&kbd, searchQuery_.c_str());
    swkbdConfigSetStringLenMax(&kbd, 80);
    std::array<char, 256> out{};
    const Result rc = swkbdShow(&kbd, out.data(), out.size());
    swkbdClose(&kbd);
    if (R_FAILED(rc)) return;
    searchQuery_ = out.data();
    if (searchQuery_.empty()) { setShelf(Shelf::All); return; }
    shelf_ = Shelf::Search;
    activeCollectionName_ = "Search: " + searchQuery_;
    rebuildVisible();
    selected_ = 0;
    screen_ = Screen::Library;
    renderer_.playSound(UiSound::Confirm, config_);
}

void App::randomPick() {
    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < games_.size(); ++i) if (!games_[i].hidden) candidates.push_back(i);
    if (candidates.empty()) { showToast("No games available", 2200, UiSound::Error); return; }
    std::mt19937_64 rng(randomSeed_ ^= (SDL_GetTicks64() + 0x9e3779b97f4a7c15ULL));
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    const auto idx = candidates[pick(rng)];
    shelf_ = Shelf::All;
    activeCollectionKey_.clear();
    activeCollectionName_.clear();
    rebuildVisible();
    syncLibrarySelectionToPath(games_[idx].path);
    detailGamePath_ = games_[idx].path;
    previousScreen_ = screen_;
    screen_ = Screen::Details;
    showToast("Surprise pick", 1400, UiSound::Page);
}

void App::toggleFavorite() {
    Game* game = selectedGame();
    if (!game) return;
    const std::string path = game->path;
    game->favorite = !game->favorite;
    persistState();
    rebuildCollections();
    rebuildHomeRows();
    rebuildVisible();
    syncLibrarySelectionToPath(path);
    showToast(game->favorite ? "Added to Favorites" : "Removed from Favorites", 1800, UiSound::Favorite);
}

void App::toggleCompleted() {
    Game* game = selectedGame();
    if (!game) return;
    game->completed = !game->completed;
    persistState();
    rebuildDerived();
    showToast(game->completed ? "Marked completed" : "Marked unfinished");
}

void App::toggleHidden() {
    Game* game = selectedGame();
    if (!game) return;
    game->hidden = !game->hidden;
    persistState();
    rebuildDerived();
    showToast(game->hidden ? "Hidden from main library" : "Restored to library");
}

void App::launchSelected() {
    Game* game = selectedGame();
    if (!game) return;
    std::error_code ec;
    if (!std::filesystem::exists(config_.mgbaNro, ec)) { showToast("mGBA not found — check Settings", 3200, UiSound::Error); return; }
    ec.clear();
    if (!std::filesystem::exists(game->path, ec)) { showToast("ROM file is missing", 2800, UiSound::Error); return; }

    if (config_.confirmLaunch) {
        const std::uint64_t now = SDL_GetTicks64();
        if (launchConfirmPath_ != game->path || now > launchConfirmUntil_) {
            launchConfirmPath_ = game->path;
            launchConfirmUntil_ = now + 2400;
            showToast("Press A again to launch", 2200, UiSound::Confirm);
            return;
        }
        launchConfirmPath_.clear();
    }

    ++game->launches;
    game->lastPlayed = static_cast<std::int64_t>(std::time(nullptr));
    persistState();
    savePendingSession(kPendingSessionPath, PendingSession{game->path, game->lastPlayed});
    renderer_.playSound(UiSound::Launch, config_);
    if (config_.launchTransition) {
        renderer_.beginFrame();
        renderer_.renderLaunch(*game, config_);
        renderer_.endFrame();
    }

    const std::string args = "\"" + config_.mgbaNro + "\" \"" + game->path + "\"";
    envSetNextLoad(config_.mgbaNro.c_str(), args.c_str());
    running_ = false;
}

void App::editSelectedTitle() {
    Game* game = selectedGame();
    if (!game) return;
    SwkbdConfig kbd{};
    if (R_FAILED(swkbdCreate(&kbd, 0))) { showToast("Software keyboard unavailable", 2200, UiSound::Error); return; }
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, "Display title");
    swkbdConfigSetSubText(&kbd, "This changes only how Advance displays the ROM.");
    swkbdConfigSetOkButtonText(&kbd, "Save");
    swkbdConfigSetInitialText(&kbd, game->title.c_str());
    swkbdConfigSetStringLenMax(&kbd, 96);
    std::array<char, 256> out{};
    const Result rc = swkbdShow(&kbd, out.data(), out.size());
    swkbdClose(&kbd);
    if (R_FAILED(rc) || out[0] == '\0') return;
    std::string title(out.data());
    title.erase(std::remove(title.begin(), title.end(), '\t'), title.end());
    if (title.empty()) return;
    titleOverrides_[game->path] = title;
    saveTitleOverrides(kTitlesPath, titleOverrides_);
    game->title = title;
    game->shortTitle = title;
    game->customTitle = true;
    game->titleSource = "Custom override";
    rebuildDerived();
    showToast("Display title saved");
}

void App::resetSelectedTitle() {
    Game* game = selectedGame();
    if (!game || !game->customTitle) { showToast("Using automatic metadata"); return; }
    titleOverrides_.erase(game->path);
    saveTitleOverrides(kTitlesPath, titleOverrides_);
    rescan(true);
    showToast("Automatic title restored");
}

void App::createCollection() {
    SwkbdConfig kbd{};
    if (R_FAILED(swkbdCreate(&kbd, 0))) { showToast("Keyboard unavailable", 2200, UiSound::Error); return; }
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, "New collection");
    swkbdConfigSetSubText(&kbd, "Create a personal shelf for your games.");
    swkbdConfigSetOkButtonText(&kbd, "Create");
    swkbdConfigSetStringLenMax(&kbd, 48);
    std::array<char, 160> out{};
    const Result rc = swkbdShow(&kbd, out.data(), out.size());
    swkbdClose(&kbd);
    if (R_FAILED(rc) || out[0] == '\0') return;
    std::string name(out.data());
    name.erase(std::remove(name.begin(), name.end(), '\t'), name.end());
    if (name.empty()) return;
    auto exists = std::find_if(customCollections_.begin(), customCollections_.end(), [&](const CustomCollection& c) {
        return normalizeName(c.name) == normalizeName(name);
    });
    if (exists != customCollections_.end()) { showToast("Collection already exists", 2200, UiSound::Error); return; }
    customCollections_.push_back(CustomCollection{name, {}});
    saveCustomCollections(kCollectionsPath, customCollections_);
    rebuildCollections();
    pickerSelected_ = customCollections_.size() - 1;
    showToast("Collection created");
}

void App::togglePickerMembership() {
    Game* game = selectedGame();
    if (!game) return;
    if (customCollections_.empty()) { createCollection(); return; }
    if (pickerSelected_ >= customCollections_.size()) pickerSelected_ = customCollections_.size() - 1;
    auto& collection = customCollections_[pickerSelected_];
    const bool member = collectionContains(collection, game->path);
    setCollectionMembership(collection, game->path, !member);
    saveCustomCollections(kCollectionsPath, customCollections_);
    rebuildCollections();
    showToast(member ? "Removed from " + collection.name : "Added to " + collection.name, 1800, UiSound::Favorite);
}

void App::persistConfig() { saveConfig(kConfigPath, config_); }
void App::persistState() { saveState(kStatePath, games_); }

void App::changeSetting(int direction) {
    static const std::array<const char*, 7> themes = {"crimson", "aurora-violet", "atomic-purple", "emerald", "midnight-gold", "ice-blue", "neon-coral"};
    switch (settingsSelected_) {
        case 0: {
            auto it = std::find_if(themes.begin(), themes.end(), [&](const char* t) { return config_.theme == t; });
            int idx = it == themes.end() ? 0 : static_cast<int>(std::distance(themes.begin(), it));
            idx = (idx + direction + static_cast<int>(themes.size())) % static_cast<int>(themes.size());
            config_.theme = themes[static_cast<std::size_t>(idx)]; break;
        }
        case 1: config_.columns = std::clamp(config_.columns + direction, 4, 7); break;
        case 2: config_.rows = std::clamp(config_.rows + direction, 2, 3); break;
        case 3: config_.dynamicBackdrop = !config_.dynamicBackdrop; break;
        case 4: config_.backdropIntensity = std::clamp(config_.backdropIntensity + direction * 5, 0, 100); break;
        case 5: config_.adaptiveAccent = !config_.adaptiveAccent; break;
        case 6: config_.screenTransitions = !config_.screenTransitions; break;
        case 7: config_.launchTransition = !config_.launchTransition; break;
        case 8: config_.uiSounds = !config_.uiSounds; break;
        case 9: config_.uiVolume = std::clamp(config_.uiVolume + direction * 5, 0, 100); break;
        case 10: config_.showCoverLabels = !config_.showCoverLabels; break;
        case 11: config_.touchEnabled = !config_.touchEnabled; break;
        case 12: config_.motionEnabled = !config_.motionEnabled; break;
        case 13: config_.showSystemStatus = !config_.showSystemStatus; break;
        case 14: config_.showHidden = !config_.showHidden; rebuildVisible(); break;
        case 15: config_.confirmLaunch = !config_.confirmLaunch; break;
        default: break;
    }
    persistConfig();
    renderer_.playSound(UiSound::Navigate, config_);
}

void App::activateSetting() {
    switch (settingsSelected_) {
        case 3: config_.dynamicBackdrop = !config_.dynamicBackdrop; persistConfig(); break;
        case 5: config_.adaptiveAccent = !config_.adaptiveAccent; persistConfig(); break;
        case 6: config_.screenTransitions = !config_.screenTransitions; persistConfig(); break;
        case 7: config_.launchTransition = !config_.launchTransition; persistConfig(); break;
        case 8: config_.uiSounds = !config_.uiSounds; persistConfig(); break;
        case 10: config_.showCoverLabels = !config_.showCoverLabels; persistConfig(); break;
        case 11: config_.touchEnabled = !config_.touchEnabled; persistConfig(); break;
        case 12: config_.motionEnabled = !config_.motionEnabled; persistConfig(); break;
        case 13: config_.showSystemStatus = !config_.showSystemStatus; persistConfig(); break;
        case 14: config_.showHidden = !config_.showHidden; persistConfig(); rebuildVisible(); break;
        case 15: config_.confirmLaunch = !config_.confirmLaunch; persistConfig(); break;
        case 16: rescan(true); showToast("Library rebuilt"); break;
        default: changeSetting(1); break;
    }
}

void App::showToast(const std::string& text, std::uint64_t durationMs, UiSound sound) {
    toast_ = text;
    toastUntil_ = SDL_GetTicks64() + durationMs;
    renderer_.playSound(sound, config_);
}

const Game* App::selectedGame() const {
    if (screen_ == Screen::Home) return homeSelectedGame();
    if ((screen_ == Screen::Details || screen_ == Screen::CollectionPicker) && !detailGamePath_.empty()) {
        auto it = std::find_if(games_.begin(), games_.end(), [&](const Game& g) { return g.path == detailGamePath_; });
        if (it != games_.end()) return &*it;
    }
    if (visible_.empty() || selected_ >= visible_.size()) return nullptr;
    const std::size_t idx = visible_[selected_];
    return idx < games_.size() ? &games_[idx] : nullptr;
}

Game* App::selectedGame() {
    return const_cast<Game*>(static_cast<const App*>(this)->selectedGame());
}

const Game* App::homeSelectedGame() const {
    if (homeRows_.empty() || homeRow_ >= homeRows_.size() || homeRows_[homeRow_].games.empty()) return nullptr;
    const std::size_t col = std::min(currentHomeColumn(), homeRows_[homeRow_].games.size() - 1);
    const std::size_t idx = homeRows_[homeRow_].games[col];
    return idx < games_.size() ? &games_[idx] : nullptr;
}

Game* App::homeSelectedGame() {
    return const_cast<Game*>(static_cast<const App*>(this)->homeSelectedGame());
}

const Collection* App::selectedCollection() const {
    if (collections_.empty() || collectionSelected_ >= collections_.size()) return nullptr;
    return &collections_[collectionSelected_];
}

std::size_t App::currentHomeColumn() const {
    return homeRow_ < homeColumns_.size() ? homeColumns_[homeRow_] : 0;
}

void App::setCurrentHomeColumn(std::size_t column) {
    if (homeColumns_.size() < homeRows_.size()) homeColumns_.resize(homeRows_.size(), 0);
    if (homeRow_ < homeColumns_.size()) homeColumns_[homeRow_] = column;
}

void App::syncLibrarySelectionToPath(const std::string& path) {
    shelf_ = Shelf::All;
    activeCollectionKey_.clear();
    activeCollectionName_.clear();
    rebuildVisible();
    for (std::size_t i = 0; i < visible_.size(); ++i) if (games_[visible_[i]].path == path) { selected_ = i; return; }
}

void App::updateTouch() {
    HidTouchScreenState state{};
    const size_t n = hidGetTouchScreenStates(&state, 1);
    const bool down = n > 0 && state.count > 0;
    if (down && !touchWasDown_) {
        const auto& touch = state.touches[0];
        handleTouchPress(static_cast<int>(touch.x), static_cast<int>(touch.y));
    }
    touchWasDown_ = down;
}

void App::handleTouchPress(int x, int y) {
    const int side = renderer_.hitTestSidebar(x, y, sidebarFocused_);
    if (side >= 0) {
        if (!sidebarFocused_) {
            sidebarSelected_ = side;
            sidebarFocused_ = true;
            renderer_.playSound(UiSound::Page, config_);
        } else if (side == sidebarSelected_) {
            activateSidebarItem(side);
        } else {
            sidebarSelected_ = side;
            renderer_.playSound(UiSound::Navigate, config_);
        }
        return;
    }
    if (sidebarFocused_) { closeSidebar(); return; }

    if (screen_ == Screen::Library) {
        auto hit = renderer_.hitTestLibrary(x, y, visible_.size(), selected_, config_);
        if (hit) {
            const std::uint64_t now = SDL_GetTicks64();
            const bool doubleTap = *hit == lastTouchedIndex_ && now - lastTouchTime_ <= 450;
            selected_ = *hit;
            lastTouchedIndex_ = *hit;
            lastTouchTime_ = now;
            renderer_.playSound(UiSound::Navigate, config_);
            if (doubleTap) launchSelected();
        }
    } else if (screen_ == Screen::Home) {
        auto hit = renderer_.hitTestHome(x, y, homeRows_, homeRow_, currentHomeColumn());
        if (hit && hit->first < homeRows_.size() && hit->second < homeRows_[hit->first].games.size()) {
            homeRow_ = hit->first;
            setCurrentHomeColumn(hit->second);
            renderer_.playSound(UiSound::Navigate, config_);
        }
    }
}

void App::render() {
    renderer_.setSidebarState(sidebarFocused_, sidebarSelected_, config_);
    renderer_.beginFrame();
    switch (screen_) {
        case Screen::Home:
            renderer_.renderHome(games_, homeRows_, homeRow_, currentHomeColumn(), config_, toast_); break;
        case Screen::Library:
            renderer_.renderLibrary(games_, visible_, selected_, shelf_, sortMode_, activeCollectionName_, config_, toast_); break;
        case Screen::Collections:
            renderer_.renderCollections(games_, collections_, collectionSelected_, config_, toast_); break;
        case Screen::Details:
            if (const Game* game = selectedGame()) renderer_.renderDetails(*game, config_, toast_);
            else screen_ = previousScreen_;
            break;
        case Screen::CollectionPicker:
            if (const Game* game = selectedGame()) renderer_.renderCollectionPicker(*game, customCollections_, pickerSelected_, config_, toast_);
            else screen_ = Screen::Details;
            break;
        case Screen::Settings:
            renderer_.renderSettings(config_, games_.size(), settingsSelected_, toast_); break;
        case Screen::About:
            renderer_.renderAbout(config_, toast_); break;
    }
    renderer_.endFrame();
}

} // namespace advance
