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
