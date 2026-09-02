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
