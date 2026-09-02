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
