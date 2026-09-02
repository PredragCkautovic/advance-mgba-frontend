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
