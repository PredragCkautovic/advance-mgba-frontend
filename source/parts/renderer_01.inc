#include "advance/renderer.hpp"
#include "advance/collections.hpp"

#include <SDL2/SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <tuple>
#include <vector>

namespace advance {
namespace {

constexpr int W = 1280;
constexpr int H = 720;
constexpr int SIDEBAR_W = 98;
constexpr int SIDEBAR_EXPANDED_W = 384;
constexpr int HEADER_H = 78;
constexpr int FOOTER_H = 38;
constexpr int CONTENT_X = SIDEBAR_W + 20;
constexpr int CONTENT_R = 20;
constexpr std::uint64_t FOCUS_ANIM_MS = 170;
constexpr std::uint64_t SHINE_ANIM_MS = 680;

SDL_Color rgba(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 255) { return {r, g, b, a}; }
float clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }
float easeOutCubic(float t) {
    t = clamp01(t);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}
float easeOutBack(float t) {
    t = clamp01(t);
    constexpr float c1 = 1.35f;
    constexpr float c3 = c1 + 1.0f;
    const float x = t - 1.0f;
    return 1.0f + c3 * x * x * x + c1 * x * x;
}
SDL_Color mixColor(SDL_Color a, SDL_Color b, float t, Uint8 alpha = 255) {
    t = clamp01(t);
    auto lerp = [t](Uint8 x, Uint8 y) -> Uint8 {
        return static_cast<Uint8>(std::round(x + (y - x) * t));
    };
    return rgba(lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), alpha);
}
std::string colorKey(SDL_Color c) {
    return std::to_string(c.r) + "," + std::to_string(c.g) + "," + std::to_string(c.b) + "," + std::to_string(c.a);
}
std::string ellipsize(TTF_Font* font, const std::string& value, int maxWidth) {
    if (!font || maxWidth <= 0) return value;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(font, value.c_str(), &w, &h) == 0 && w <= maxWidth) return value;
    std::string s = value;
    while (!s.empty()) {
        s.pop_back();
        const std::string trial = s + "…";
        if (TTF_SizeUTF8(font, trial.c_str(), &w, &h) == 0 && w <= maxWidth) return trial;
    }
    return "…";
}

struct GalleryLayout {
    int x{CONTENT_X};
    int y{HEADER_H + 10};
    int w{W - CONTENT_X - CONTENT_R};
    int h{0};
    int cols{6};
    int rows{2};
    int gap{14};
    int cardW{0};
    int cardH{0};
    std::size_t perPage{12};
};

GalleryLayout galleryLayout(const Config& config) {
    GalleryLayout l;
    constexpr int stripH = 88;
    l.h = H - FOOTER_H - stripH - l.y - 10;
    l.cols = std::clamp(config.columns, 4, 7);
    l.rows = std::clamp(config.rows, 2, 3);
    l.cardW = (l.w - l.gap * (l.cols - 1)) / l.cols;
    l.cardH = (l.h - l.gap * (l.rows - 1)) / l.rows;
    l.perPage = static_cast<std::size_t>(l.cols * l.rows);
    return l;
}

} // namespace

Renderer::Renderer() = default;

Renderer::~Renderer() {
    for (auto& [_, e] : coverCache_) if (e.texture) SDL_DestroyTexture(e.texture);
    for (auto& [_, e] : textCache_) if (e.texture) SDL_DestroyTexture(e.texture);
    if (avatarTexture_) SDL_DestroyTexture(avatarTexture_);
    if (audioDevice_) SDL_CloseAudioDevice(audioDevice_);

    if (accountReady_) accountExit();
    if (psmReady_) psmExit();
    if (nifmReady_) nifmExit();

    if (fonts_.tiny) TTF_CloseFont(fonts_.tiny);
    if (fonts_.small) TTF_CloseFont(fonts_.small);
    if (fonts_.body) TTF_CloseFont(fonts_.body);
    if (fonts_.medium) TTF_CloseFont(fonts_.medium);
    if (fonts_.large) TTF_CloseFont(fonts_.large);
    if (fonts_.hero) TTF_CloseFont(fonts_.hero);
    if (fonts_.mega) TTF_CloseFont(fonts_.mega);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    if (plReady_) plExit();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

bool Renderer::init(const Config& config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) != 0) return false;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    const int imageFlags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    if ((IMG_Init(imageFlags) & (IMG_INIT_PNG | IMG_INIT_JPG)) == 0) return false;
    if (TTF_Init() != 0) return false;

    window_ = SDL_CreateWindow("Advance", 0, 0, W, H, SDL_WINDOW_FULLSCREEN);
    if (!window_) return false;
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) return false;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_DISABLE);

    if (R_FAILED(plInitialize(PlServiceType_User))) return false;
    plReady_ = true;
    if (R_FAILED(plGetSharedFontByType(&fontData_, PlSharedFontType_Standard))) return false;

    auto openFont = [&](int size) -> TTF_Font* {
        SDL_RWops* rw = SDL_RWFromConstMem(fontData_.address, static_cast<int>(fontData_.size));
        return rw ? TTF_OpenFontRW(rw, 1, size) : nullptr;
    };
    fonts_.tiny = openFont(14);
    fonts_.small = openFont(17);
    fonts_.body = openFont(21);
    fonts_.medium = openFont(27);
    fonts_.large = openFont(35);
    fonts_.hero = openFont(46);
    fonts_.mega = openFont(58);
    if (!fonts_.tiny || !fonts_.small || !fonts_.body || !fonts_.medium || !fonts_.large || !fonts_.hero || !fonts_.mega) return false;
