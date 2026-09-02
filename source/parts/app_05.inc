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
