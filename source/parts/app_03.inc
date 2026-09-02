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
