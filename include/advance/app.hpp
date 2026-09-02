#pragma once

#include "advance/collections.hpp"
#include "advance/library.hpp"
#include "advance/model.hpp"
#include "advance/renderer.hpp"

#include <switch.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace advance {

class App {
public:
    int run();

private:
    static constexpr const char* kConfigPath = "sdmc:/switch/advance/config.ini";
    static constexpr const char* kStatePath  = "sdmc:/switch/advance/state.tsv";
    static constexpr const char* kTitlesPath = "sdmc:/switch/advance/titles.tsv";
    static constexpr const char* kCollectionsPath = "sdmc:/switch/advance/collections.tsv";
    static constexpr const char* kPendingSessionPath = "sdmc:/switch/advance/session.pending";

    Config config_{};
    std::vector<Game> games_;
    std::vector<std::size_t> visible_;
    std::vector<HomeRow> homeRows_;
    std::vector<Collection> collections_;
    std::vector<CustomCollection> customCollections_;
    TitleOverrideMap titleOverrides_;
    Renderer renderer_{};
    PadState pad_{};

    Screen screen_{Screen::Home};
    Screen previousScreen_{Screen::Home};
    Shelf shelf_{Shelf::All};
    SortMode sortMode_{SortMode::Title};
    std::string activeCollectionKey_;
    std::string activeCollectionName_;

    std::size_t selected_{0};
    std::size_t homeRow_{0};
    std::vector<std::size_t> homeColumns_;
    std::size_t collectionSelected_{0};
    std::size_t pickerSelected_{0};
    std::size_t settingsSelected_{0};

    bool running_{true};
    bool sidebarFocused_{false};
    int sidebarSelected_{0};
    std::string toast_;
    std::string searchQuery_;
    std::string detailGamePath_;
    std::uint64_t toastUntil_{0};
    std::uint64_t nextNavRepeat_{0};
    std::uint64_t navHeldMask_{0};
    std::uint64_t randomSeed_{0};
    std::string launchConfirmPath_;
    std::uint64_t launchConfirmUntil_{0};

    bool touchWasDown_{false};
    std::uint64_t lastTouchTime_{0};
    std::size_t lastTouchedIndex_{static_cast<std::size_t>(-1)};

    void ensureDataFolders();
    void rescan(bool preserveSelection = false);
    void rebuildDerived();
    void rebuildVisible();
    void rebuildHomeRows();
    void rebuildCollections();
    void clampSelection();
    void updateInput();
    void updateTouch();
    void handleTouchPress(int x, int y);
    void handleHomeInput(std::uint64_t down, std::uint64_t held);
    void handleLibraryInput(std::uint64_t down, std::uint64_t held);
    void handleCollectionsInput(std::uint64_t down, std::uint64_t held);
    void handleDetailsInput(std::uint64_t down);
    void handleCollectionPickerInput(std::uint64_t down);
    void handleSettingsInput(std::uint64_t down);
    void handleAboutInput(std::uint64_t down);
    void handleSidebarInput(std::uint64_t down, std::uint64_t held);
    bool repeatPressed(std::uint64_t button, std::uint64_t down, std::uint64_t held);

    void navigateLibrary(int dx, int dy);
    void focusSidebar();
    void closeSidebar();
    void activateSidebarItem(int index);
    int activeSidebarIndex() const;
    void navigateHome(int dx, int dy);
    void changePage(int direction);
    void setScreen(Screen screen);
    void setShelf(Shelf shelf);
    void openCollection(const Collection& collection);
    void cycleSort();
    void openSearch();
    void randomPick();

    void toggleFavorite();
    void toggleCompleted();
    void toggleHidden();
    void launchSelected();
    void editSelectedTitle();
    void resetSelectedTitle();
    void createCollection();
    void togglePickerMembership();
    void changeSetting(int direction);
    void activateSetting();
    void persistConfig();
    void persistState();
    void showToast(const std::string& text, std::uint64_t durationMs = 2200, UiSound sound = UiSound::Confirm);

    const Game* selectedGame() const;
    Game* selectedGame();
    const Game* homeSelectedGame() const;
    Game* homeSelectedGame();
    const Collection* selectedCollection() const;
    std::size_t currentHomeColumn() const;
    void setCurrentHomeColumn(std::size_t column);
    void syncLibrarySelectionToPath(const std::string& path);
    void render();
};

} // namespace advance
