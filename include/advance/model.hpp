#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace advance {

enum class Shelf {
    All,
    Favorites,
    Recent,
    Search,
    Collection,
    Completed,
    Hidden,
};

enum class SortMode {
    Title,
    Recent,
    Launches,
    PlayTime,
    Added,
    Favorites,
};

enum class Screen {
    Home,
    Library,
    Collections,
    Details,
    CollectionPicker,
    Settings,
    About,
};

enum class UiSound {
    Navigate,
    Confirm,
    Back,
    Favorite,
    Page,
    Launch,
    Error,
};

struct Game {
    std::string path;
    std::string folderPath;
    std::string title;
    std::string shortTitle;
    std::string automaticTitle;
    std::string titleSource;
    std::string coverPath;
    std::string bannerPath;
    std::vector<std::string> screenshotPaths;
    std::string metadataPath;
    std::string gameCode;

    std::string author;
    std::string version;
    std::string baseGame;
    std::string description;
    std::vector<std::string> genres;
    std::vector<std::string> tags;
    int releaseYear{0};

    bool customTitle{false};
    bool favorite{false};
    bool hidden{false};
    bool completed{false};
    std::uint32_t launches{0};
    std::uint64_t playSeconds{0};
    std::int64_t lastPlayed{0};
    std::int64_t addedAt{0};
};

struct Config {
    std::string romDir{"sdmc:/mGBA/Roms"};
    std::string coverDir{"sdmc:/switch/advance/covers"};
    std::string mgbaNro{"sdmc:/switch/mgba.nro"};
    bool scanRecursively{true};
    bool includeGb{false};
    bool includeGbc{false};

    int columns{6};
    int rows{2};
    std::string theme{"crimson"};
    std::string accent{"red"}; // Legacy 2.x compatibility; theme wins in 3.x.
    bool useAccountProfile{true};
    bool showSystemStatus{true};
    bool touchEnabled{true};
    bool motionEnabled{true};
    bool uiSounds{true};
    int uiVolume{72};
    bool dynamicBackdrop{true};
    int backdropIntensity{58};
    bool adaptiveAccent{true};
    bool screenTransitions{true};
    bool launchTransition{true};
    bool showCoverLabels{false};
    bool showHidden{false};
    bool confirmLaunch{false};
};

struct GameState {
    bool favorite{false};
    bool hidden{false};
    bool completed{false};
    std::uint32_t launches{0};
    std::uint64_t playSeconds{0};
    std::int64_t lastPlayed{0};
    std::int64_t addedAt{0};
};

struct HomeRow {
    std::string key;
    std::string title;
    std::string subtitle;
    std::vector<std::size_t> games;
};

struct Collection {
    std::string key;
    std::string name;
    std::string description;
    bool automatic{true};
    std::vector<std::size_t> games;
};

struct CustomCollection {
    std::string name;
    std::vector<std::string> romPaths;
};

} // namespace advance
