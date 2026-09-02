#pragma once

#include "advance/model.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <switch.h>

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace advance {

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(const Config& config);
    void beginFrame();
    void endFrame();
    void playSound(UiSound sound, const Config& config);
    void setSidebarState(bool focused, int selected, const Config& config);

    void renderBoot(const Config& config, const std::string& message);
    void renderHome(const std::vector<Game>& games,
                    const std::vector<HomeRow>& rows,
                    std::size_t selectedRow,
                    std::size_t selectedColumn,
                    const Config& config,
                    const std::string& toast);
    void renderLibrary(const std::vector<Game>& games,
                       const std::vector<std::size_t>& visible,
                       std::size_t selected,
                       Shelf shelf,
                       SortMode sortMode,
                       const std::string& collectionName,
                       const Config& config,
                       const std::string& toast);
    void renderCollections(const std::vector<Game>& games,
                           const std::vector<Collection>& collections,
                           std::size_t selected,
                           const Config& config,
                           const std::string& toast);
    void renderDetails(const Game& game, const Config& config, const std::string& toast);
    void renderCollectionPicker(const Game& game,
                                const std::vector<CustomCollection>& collections,
                                std::size_t selected,
                                const Config& config,
                                const std::string& toast);
    void renderSettings(const Config& config, std::size_t gameCount, std::size_t selectedSetting,
                        const std::string& toast);
    void renderAbout(const Config& config, const std::string& toast);
    void renderLaunch(const Game& game, const Config& config);

    std::optional<std::size_t> hitTestLibrary(int x, int y,
                                              std::size_t visibleCount,
                                              std::size_t selected,
                                              const Config& config) const;
    std::optional<std::pair<std::size_t, std::size_t>> hitTestHome(int x, int y,
                                                                   const std::vector<HomeRow>& rows,
                                                                   std::size_t selectedRow,
                                                                   std::size_t selectedColumn) const;
    int hitTestSidebar(int x, int y, bool expanded = false) const;

private:
    struct FontSet {
        TTF_Font* tiny{nullptr};
        TTF_Font* small{nullptr};
        TTF_Font* body{nullptr};
        TTF_Font* medium{nullptr};
        TTF_Font* large{nullptr};
        TTF_Font* hero{nullptr};
        TTF_Font* mega{nullptr};
    };

    struct TextureEntry {
        SDL_Texture* texture{nullptr};
        int w{0};
        int h{0};
        SDL_Color average{20, 20, 24, 255};
        std::uint64_t lastUse{0};
    };

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    SDL_AudioDeviceID audioDevice_{0};
    SDL_AudioSpec audioSpec_{};
    FontSet fonts_{};
    PlFontData fontData_{};
    bool plReady_{false};
    std::uint64_t frame_{0};
    std::unordered_map<std::string, TextureEntry> coverCache_;
    std::unordered_set<std::string> failedCoverCache_;
    std::unordered_map<std::string, TextureEntry> textCache_;

    bool accountReady_{false};
    bool psmReady_{false};
    bool nifmReady_{false};
    SDL_Texture* avatarTexture_{nullptr};
    int avatarW_{0};
    int avatarH_{0};
    std::string nickname_{"Player"};
    std::string profileSource_{"Fallback"};
    std::string profileDiagnostic_;
    std::uint32_t batteryPercent_{0};
    bool charging_{false};
    bool connected_{false};
    std::uint32_t wifiStrength_{0};
    std::uint64_t nextSystemRefresh_{0};

    bool sidebarFocused_{false};
    int sidebarSelected_{0};
    float sidebarProgress_{0.0f};
    std::uint64_t sidebarFrameAt_{0};
    bool sidebarMotionEnabled_{true};
    Screen lastRenderedScreen_{Screen::Home};
    bool hasRenderedScreen_{false};
    std::uint64_t screenChangedAt_{0};

    std::string lastFocusPath_;
    std::uint64_t selectionChangedAt_{0};

    SDL_Color accentColor(const Config& config) const;
    SDL_Color secondaryColor(const Config& config) const;
    SDL_Color surfaceColor(const Config& config, Uint8 alpha = 255) const;
    SDL_Texture* loadImage(const std::string& path, int& w, int& h, SDL_Color* average = nullptr);
    SDL_Texture* loadCover(const Game& game, int& w, int& h, SDL_Color* average = nullptr);
    void pruneCoverCache();
    void pruneTextCache();

    void initUserProfile(const Config& config);
    void initSystemStatus(const Config& config);
    void initAudio();
    void refreshSystemStatus();
    bool loadLocalProfileFallback();
    SDL_Texture* createCircularAvatar(SDL_Surface* source, int size);
    void writeProfileDiagnostic(const std::string& message);
    std::vector<std::int16_t> synthTone(UiSound sound, int volume) const;

    void fill(SDL_Rect rect, SDL_Color color);
    void outline(SDL_Rect rect, SDL_Color color, int thickness = 1);
    void line(int x1, int y1, int x2, int y2, SDL_Color color);
    void circle(int cx, int cy, int radius, SDL_Color color, bool filled);
    void roundedFill(SDL_Rect rect, int radius, SDL_Color color);
    void roundedOutline(SDL_Rect rect, int radius, SDL_Color color, int thickness = 1);

    SDL_Texture* textTexture(TTF_Font* font, const std::string& text, SDL_Color color, int& w, int& h);
    void text(TTF_Font* font, const std::string& value, int x, int y, SDL_Color color,
              int maxWidth = 0, bool centered = false);
    void textRight(TTF_Font* font, const std::string& value, int rightX, int y, SDL_Color color);
    void wrappedText(TTF_Font* font, const std::string& value, SDL_Rect rect, SDL_Color color, int lineGap = 4);

    void drawBaseBackdrop(const Config& config);
    void drawDynamicBackdrop(const Game* game, const Config& config, int strength = -1);
    void drawSidebar(Screen screen, Shelf shelf, const Config& config);
    void drawSidebarOverlay(Screen screen, Shelf shelf, const Config& config);
    void drawSidebarIcon(int centerX, int centerY, int type, SDL_Color color);
    void drawTopChrome(const std::string& eyebrow, const std::string& title, const std::string& subtitle,
                       const Config& config, bool compactProfile = false);
    void drawProfileChip(const Config& config, bool compact = false);
    void drawAvatar(int x, int y, int diameter, const Config& config);
    void drawWifiIcon(int x, int y, SDL_Color color);
    void drawBatteryIcon(int x, int y, SDL_Color color);
    void drawFooter(const std::vector<std::pair<std::string, std::string>>& hints);
    void drawButtonHint(int& x, int y, const std::string& button, const std::string& label);
    void drawCoverCard(const Game& game, SDL_Rect rect, bool selected, const Config& config, float focusProgress,
                       bool compact = false);
    void drawPlaceholderCover(const Game& game, SDL_Rect rect, const Config& config);
    void drawHero(const Game* game, const std::vector<Game>& games, const Config& config);
    void drawHomeShelf(const std::vector<Game>& games, const HomeRow& row, std::size_t rowIndex,
                       std::size_t selectedRow, std::size_t selectedColumn, int y, const Config& config);
    void drawSelectedStrip(const Game* game, std::size_t selected, std::size_t count,
                           std::size_t perPage, Shelf shelf, const Config& config);
    void drawCollectionCard(const std::vector<Game>& games, const Collection& collection, SDL_Rect rect,
                            bool selected, const Config& config);
    void drawToast(const std::string& toast);
    void drawScreenTransition(Screen screen, const Config& config);
    void drawBrandHandheld(int x, int y, int w, int h, SDL_Color accent, Uint8 alpha = 255);
    void markFocus(const Game* game);

    std::string shelfName(Shelf shelf, const std::string& collectionName = {}) const;
    std::string sortName(SortMode mode) const;
    std::string formatTimestamp(std::int64_t ts) const;
    std::string formatClock() const;
    std::string formatPlayTime(std::uint64_t seconds) const;
    std::string join(const std::vector<std::string>& values, const std::string& separator, std::size_t max = 4) const;
};

} // namespace advance
