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
    static const std::array<const char*, 6> themes = {"crimson", "atomic-purple", "emerald", "midnight-gold", "ice-blue", "neon-coral"};
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
