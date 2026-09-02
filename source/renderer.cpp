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
constexpr int SIDEBAR_EXPANDED_W = 408;
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
std::string prettyThemeName(const std::string& theme) {
    if (theme == "aurora-violet") return "Aurora Violet";
    if (theme == "atomic-purple") return "Atomic Purple";
    if (theme == "midnight-gold") return "Midnight Gold";
    if (theme == "ice-blue") return "Ice Blue";
    if (theme == "neon-coral") return "Neon Coral";
    if (theme == "emerald") return "Emerald";
    return "Crimson";
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

    initUserProfile(config);
    initSystemStatus(config);
    initAudio();
    refreshSystemStatus();
    return true;
}

void Renderer::setSidebarState(bool focused, int selected, const Config& config) {
    sidebarFocused_ = focused;
    sidebarSelected_ = std::clamp(selected, 0, 6);
    sidebarMotionEnabled_ = config.motionEnabled;
}

void Renderer::writeProfileDiagnostic(const std::string& message) {
    std::ofstream out("sdmc:/switch/advance/diagnostics.log", std::ios::app);
    if (out) out << message << '\n';
}

SDL_Texture* Renderer::createCircularAvatar(SDL_Surface* source, int size) {
    if (!source || size <= 0) return nullptr;
    SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32);
    if (!scaled) return nullptr;
    const float aspect = source->w > 0 && source->h > 0 ? static_cast<float>(source->w) / source->h : 1.0f;
    SDL_Rect src{0, 0, source->w, source->h};
    if (aspect > 1.0f) { src.w = source->h; src.x = (source->w - src.w) / 2; }
    else if (aspect < 1.0f) { src.h = source->w; src.y = (source->h - src.h) / 2; }
    SDL_Rect dst{0, 0, size, size};
    SDL_BlitScaled(source, &src, scaled, &dst);

    if (SDL_MUSTLOCK(scaled)) SDL_LockSurface(scaled);
    auto* pixels = static_cast<Uint32*>(scaled->pixels);
    SDL_PixelFormat* fmt = scaled->format;
    const float center = (size - 1) * 0.5f;
    const float radius = center - 1.0f;
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        const float dx = x - center, dy = y - center;
        if (dx * dx + dy * dy <= radius * radius) continue;
        Uint8 r, g, b, a;
        SDL_GetRGBA(pixels[y * (scaled->pitch / 4) + x], fmt, &r, &g, &b, &a);
        pixels[y * (scaled->pitch / 4) + x] = SDL_MapRGBA(fmt, r, g, b, 0);
    }
    if (SDL_MUSTLOCK(scaled)) SDL_UnlockSurface(scaled);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, scaled);
    SDL_FreeSurface(scaled);
    if (texture) SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    return texture;
}

bool Renderer::loadLocalProfileFallback() {
    const char* images[] = {
        "sdmc:/switch/advance/profile.png", "sdmc:/switch/advance/profile.jpg", "sdmc:/switch/advance/profile.jpeg"
    };
    for (const char* path : images) {
        SDL_Surface* surface = IMG_Load(path);
        if (!surface) continue;
        avatarTexture_ = createCircularAvatar(surface, 128);
        SDL_FreeSurface(surface);
        if (avatarTexture_) { avatarW_ = avatarH_ = 128; break; }
    }
    std::ifstream nicknameFile("sdmc:/switch/advance/profile.txt");
    std::string localName;
    if (nicknameFile && std::getline(nicknameFile, localName) && !localName.empty()) nickname_ = localName;
    if (avatarTexture_ || nickname_ != "Player") { profileSource_ = "Local profile"; return true; }
    return false;
}

void Renderer::initUserProfile(const Config& config) {
    std::ofstream clear("sdmc:/switch/advance/diagnostics.log", std::ios::trunc);
    if (clear) clear << "Advance 0.4 diagnostics\n";
    nickname_ = "Player";
    profileSource_ = "Profile unavailable";
    profileDiagnostic_.clear();
    if (!config.useAccountProfile) {
        profileDiagnostic_ = "Switch profile disabled in config";
        loadLocalProfileFallback();
        writeProfileDiagnostic(profileDiagnostic_);
        return;
    }

    auto sameUid = [](const AccountUid& a, const AccountUid& b) { return a.uid[0] == b.uid[0] && a.uid[1] == b.uid[1]; };
    struct Candidate { AccountUid uid{}; std::string method; };
    std::ostringstream diagnostic;

    auto tryService = [&](AccountServiceType serviceType, const char* serviceName) -> bool {
        Result rc = accountInitialize(serviceType);
        diagnostic << serviceName << " init=0x" << std::hex << rc << std::dec << '\n';
        if (R_FAILED(rc)) return false;
        accountReady_ = true;
        std::vector<Candidate> candidates;
        auto pushCandidate = [&](const AccountUid& uid, const char* method) {
            if (!accountUidIsValid(&uid)) return;
            for (const auto& c : candidates) if (sameUid(c.uid, uid)) return;
            candidates.push_back(Candidate{uid, method});
        };

        AccountUid uid{};
        rc = accountGetPreselectedUser(&uid); if (R_SUCCEEDED(rc)) pushCandidate(uid, "preselected");
        uid = {}; rc = accountGetLastOpenedUser(&uid); if (R_SUCCEEDED(rc)) pushCandidate(uid, "last-opened");
        uid = {}; rc = accountTrySelectUserWithoutInteraction(&uid, false); if (R_SUCCEEDED(rc)) pushCandidate(uid, "automatic");
        std::array<AccountUid, ACC_USER_LIST_SIZE> users{};
        s32 total = 0;
        rc = accountListAllUsers(users.data(), static_cast<s32>(users.size()), &total);
        if (R_SUCCEEDED(rc)) {
            const s32 safeTotal = std::max<s32>(0, std::min<s32>(total, static_cast<s32>(users.size())));
            for (s32 i = 0; i < safeTotal; ++i) pushCandidate(users[static_cast<std::size_t>(i)], "profile-list");
        }

        for (const auto& candidate : candidates) {
            AccountProfile profile{};
            rc = accountGetProfile(&profile, candidate.uid);
            if (R_FAILED(rc)) continue;
            AccountProfileBase base{};
            const Result baseRc = accountProfileGet(&profile, nullptr, &base);
            if (R_FAILED(baseRc)) { accountProfileClose(&profile); continue; }

            std::string candidateName;
            std::size_t len = 0;
            while (len < sizeof(base.nickname) && base.nickname[len] != '\0') ++len;
            if (len > 0) candidateName.assign(base.nickname, len);

            SDL_Texture* candidateAvatar = nullptr;
            u32 imageSize = 0;
            if (R_SUCCEEDED(accountProfileGetImageSize(&profile, &imageSize)) && imageSize > 0 && imageSize < 8 * 1024 * 1024) {
                std::vector<unsigned char> image(imageSize);
                u32 loaded = 0;
                if (R_SUCCEEDED(accountProfileLoadImage(&profile, image.data(), image.size(), &loaded)) && loaded > 0) {
                    SDL_RWops* rw = SDL_RWFromConstMem(image.data(), static_cast<int>(loaded));
                    SDL_Surface* surface = rw ? IMG_Load_RW(rw, 1) : nullptr;
                    if (surface) { candidateAvatar = createCircularAvatar(surface, 128); SDL_FreeSurface(surface); }
                }
            }
            accountProfileClose(&profile);

            if (!candidateName.empty()) {
                nickname_ = candidateName;
                if (avatarTexture_) SDL_DestroyTexture(avatarTexture_);
                avatarTexture_ = candidateAvatar;
                if (avatarTexture_) avatarW_ = avatarH_ = 128;
                profileSource_ = "Switch profile";
                diagnostic << "selected=" << candidate.method << " nickname=" << nickname_ << " avatar=" << (avatarTexture_ ? "yes" : "no") << '\n';
                accountExit(); accountReady_ = false; return true;
            }
            if (candidateAvatar) SDL_DestroyTexture(candidateAvatar);
        }
        accountExit(); accountReady_ = false; return false;
    };

    bool loaded = tryService(AccountServiceType_Application, "acc:u0");
    if (!loaded) loaded = tryService(AccountServiceType_System, "acc:u1");
    if (!loaded) {
        diagnostic << "No readable Switch profile; local fallback attempted\n";
        loadLocalProfileFallback();
    } else if (!avatarTexture_) {
        const std::string realName = nickname_, realSource = profileSource_;
        loadLocalProfileFallback();
        nickname_ = realName; profileSource_ = realSource;
    }
    profileDiagnostic_ = diagnostic.str();
    writeProfileDiagnostic(profileDiagnostic_);
}

void Renderer::initSystemStatus(const Config& config) {
    if (!config.showSystemStatus) return;
    if (R_SUCCEEDED(psmInitialize())) psmReady_ = true;
    if (R_SUCCEEDED(nifmInitialize(NifmServiceType_User))) nifmReady_ = true;
}

void Renderer::initAudio() {
    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    audioDevice_ = SDL_OpenAudioDevice(nullptr, 0, &want, &audioSpec_, 0);
    if (audioDevice_) SDL_PauseAudioDevice(audioDevice_, 0);
}

void Renderer::refreshSystemStatus() {
    const std::uint64_t now = SDL_GetTicks64();
    if (now < nextSystemRefresh_) return;
    nextSystemRefresh_ = now + 2500;
    if (psmReady_) {
        u32 battery = 0;
        if (R_SUCCEEDED(psmGetBatteryChargePercentage(&battery))) batteryPercent_ = std::min<u32>(battery, 100);
        PsmChargerType charger = PsmChargerType_Unconnected;
        if (R_SUCCEEDED(psmGetChargerType(&charger))) charging_ = charger != PsmChargerType_Unconnected;
    }
    connected_ = false; wifiStrength_ = 0;
    if (nifmReady_) {
        NifmInternetConnectionType type{};
        NifmInternetConnectionStatus status{};
        u32 strength = 0;
        if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &strength, &status))) {
            connected_ = status == NifmInternetConnectionStatus_Connected;
            wifiStrength_ = std::min<u32>(strength, 3);
            if (connected_ && type == NifmInternetConnectionType_Ethernet) wifiStrength_ = 3;
        }
    }
}

std::vector<std::int16_t> Renderer::synthTone(UiSound sound, int volume) const {
    const int rate = audioSpec_.freq > 0 ? audioSpec_.freq : 48000;
    double f1 = 520.0, f2 = 0.0, seconds = 0.035;
    switch (sound) {
        case UiSound::Confirm: f1 = 740; f2 = 980; seconds = 0.055; break;
        case UiSound::Back: f1 = 430; f2 = 320; seconds = 0.055; break;
        case UiSound::Favorite: f1 = 820; f2 = 1240; seconds = 0.075; break;
        case UiSound::Page: f1 = 580; f2 = 720; seconds = 0.045; break;
        case UiSound::Launch: f1 = 560; f2 = 1120; seconds = 0.105; break;
        case UiSound::Error: f1 = 220; f2 = 180; seconds = 0.09; break;
        default: break;
    }
    const int count = std::max(1, static_cast<int>(rate * seconds));
    std::vector<std::int16_t> samples(static_cast<std::size_t>(count));
    const double amp = 3600.0 * (std::clamp(volume, 0, 100) / 100.0);
    constexpr double pi = 3.14159265358979323846;
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / rate;
        const double env = std::sin(pi * static_cast<double>(i) / count);
        double v = std::sin(2.0 * pi * f1 * t);
        if (f2 > 0.0) v = 0.7 * v + 0.3 * std::sin(2.0 * pi * f2 * t);
        samples[static_cast<std::size_t>(i)] = static_cast<std::int16_t>(amp * env * v);
    }
    return samples;
}

void Renderer::playSound(UiSound sound, const Config& config) {
    if (!config.uiSounds || config.uiVolume <= 0 || !audioDevice_) return;
    if (SDL_GetQueuedAudioSize(audioDevice_) > 48000) SDL_ClearQueuedAudio(audioDevice_);
    const auto samples = synthTone(sound, config.uiVolume);
    if (!samples.empty()) SDL_QueueAudio(audioDevice_, samples.data(), static_cast<Uint32>(samples.size() * sizeof(std::int16_t)));
}

void Renderer::beginFrame() {
    SDL_PumpEvents();
    ++frame_;
    refreshSystemStatus();
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
}

void Renderer::endFrame() {
    pruneCoverCache();
    pruneTextCache();
    SDL_RenderPresent(renderer_);
}

SDL_Color Renderer::accentColor(const Config& config) const {
    if (config.theme == "aurora-violet") return rgba(146, 96, 255);
    if (config.theme == "atomic-purple") return rgba(158, 92, 255);
    if (config.theme == "emerald") return rgba(34, 218, 131);
    if (config.theme == "midnight-gold") return rgba(255, 184, 61);
    if (config.theme == "ice-blue") return rgba(67, 184, 255);
    if (config.theme == "neon-coral") return rgba(255, 75, 125);
    return rgba(255, 39, 65);
}

SDL_Color Renderer::secondaryColor(const Config& config) const {
    if (config.theme == "aurora-violet") return rgba(45, 211, 222);
    if (config.theme == "atomic-purple") return rgba(86, 48, 170);
    if (config.theme == "emerald") return rgba(13, 105, 69);
    if (config.theme == "midnight-gold") return rgba(130, 79, 10);
    if (config.theme == "ice-blue") return rgba(23, 92, 158);
    if (config.theme == "neon-coral") return rgba(133, 28, 67);
    return rgba(128, 12, 31);
}

SDL_Color Renderer::surfaceColor(const Config&, Uint8 alpha) const { return rgba(8, 8, 12, alpha); }

void Renderer::fill(SDL_Rect rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer_, &rect);
}

void Renderer::outline(SDL_Rect rect, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int i = 0; i < thickness; ++i) {
        SDL_Rect r{rect.x - i, rect.y - i, rect.w + i * 2, rect.h + i * 2};
        SDL_RenderDrawRect(renderer_, &r);
    }
}

void Renderer::line(int x1, int y1, int x2, int y2, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer_, x1, y1, x2, y2);
}

void Renderer::circle(int cx, int cy, int radius, SDL_Color color, bool filledCircle) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int y = -radius; y <= radius; ++y) {
        const int x = static_cast<int>(std::sqrt(std::max(0, radius * radius - y * y)));
        if (filledCircle) SDL_RenderDrawLine(renderer_, cx - x, cy + y, cx + x, cy + y);
        else {
            SDL_RenderDrawPoint(renderer_, cx - x, cy + y);
            SDL_RenderDrawPoint(renderer_, cx + x, cy + y);
        }
    }
}

void Renderer::roundedFill(SDL_Rect rect, int radius, SDL_Color color) {
    radius = std::max(0, std::min(radius, std::min(rect.w, rect.h) / 2));
    if (radius == 0) { fill(rect, color); return; }
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int y = 0; y < rect.h; ++y) {
        int inset = 0;
        if (y < radius) {
            const int dy = radius - y - 1;
            inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        } else if (y >= rect.h - radius) {
            const int dy = y - (rect.h - radius);
            inset = radius - static_cast<int>(std::sqrt(std::max(0, radius * radius - dy * dy)));
        }
        SDL_RenderDrawLine(renderer_, rect.x + inset, rect.y + y, rect.x + rect.w - 1 - inset, rect.y + y);
    }
}

void Renderer::roundedOutline(SDL_Rect rect, int radius, SDL_Color color, int thickness) {
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    for (int t = 0; t < thickness; ++t) {
        SDL_Rect r{rect.x + t, rect.y + t, rect.w - t * 2, rect.h - t * 2};
        const int rr = std::max(1, radius - t);
        line(r.x + rr, r.y, r.x + r.w - rr - 1, r.y, color);
        line(r.x + rr, r.y + r.h - 1, r.x + r.w - rr - 1, r.y + r.h - 1, color);
        line(r.x, r.y + rr, r.x, r.y + r.h - rr - 1, color);
        line(r.x + r.w - 1, r.y + rr, r.x + r.w - 1, r.y + r.h - rr - 1, color);
        for (int deg = 0; deg <= 90; deg += 3) {
            const double a = deg * 3.14159265358979323846 / 180.0;
            const int dx = static_cast<int>(std::cos(a) * rr);
            const int dy = static_cast<int>(std::sin(a) * rr);
            SDL_RenderDrawPoint(renderer_, r.x + rr - dx, r.y + rr - dy);
            SDL_RenderDrawPoint(renderer_, r.x + r.w - rr - 1 + dx, r.y + rr - dy);
            SDL_RenderDrawPoint(renderer_, r.x + rr - dx, r.y + r.h - rr - 1 + dy);
            SDL_RenderDrawPoint(renderer_, r.x + r.w - rr - 1 + dx, r.y + r.h - rr - 1 + dy);
        }
    }
}


void Renderer::drawBrandHandheld(int x, int y, int w, int h, SDL_Color accent, Uint8 alpha) {
    const SDL_Color shell = mixColor(rgba(101, 63, 232), accent, 0.22f, alpha);
    const SDL_Color shellDark = rgba(35, 25, 84, alpha);
    const SDL_Color ink = rgba(10, 12, 39, alpha);
    const SDL_Color screen = rgba(45, 208, 221, alpha);
    roundedFill({x, y, w, h}, std::max(8, h / 4), shellDark);
    roundedFill({x + 3, y + 3, w - 6, h - 6}, std::max(7, h / 4 - 2), shell);
    roundedOutline({x + 3, y + 3, w - 6, h - 6}, std::max(7, h / 4 - 2), rgba(255,255,255, static_cast<Uint8>(alpha * 0.6f)), 1);

    const int bezelX = x + w * 28 / 100;
    const int bezelY = y + h * 22 / 100;
    const int bezelW = w * 45 / 100;
    const int bezelH = h * 55 / 100;
    roundedFill({bezelX, bezelY, bezelW, bezelH}, std::max(5, h / 8), ink);
    roundedFill({bezelX + 4, bezelY + 4, bezelW - 8, bezelH - 8}, std::max(4, h / 10), screen);
    line(bezelX + bezelW / 2 - 8, bezelY + bezelH / 2 + 4,
         bezelX + bezelW / 2 + 8, bezelY + bezelH / 2 - 6, rgba(225,255,255, static_cast<Uint8>(alpha * 0.8f)));
    line(bezelX + bezelW / 2 - 3, bezelY + bezelH / 2 + 9,
         bezelX + bezelW / 2 + 12, bezelY + bezelH / 2, rgba(225,255,255, static_cast<Uint8>(alpha * 0.62f)));

    const int dpadX = x + w * 16 / 100;
    const int dpadY = y + h * 52 / 100;
    const int arm = std::max(3, h / 12);
    roundedFill({dpadX - arm * 2, dpadY - arm / 2, arm * 4, arm}, 2, ink);
    roundedFill({dpadX - arm / 2, dpadY - arm * 2, arm, arm * 4}, 2, ink);

    const int aX = x + w * 84 / 100;
    const int aY = y + h * 44 / 100;
    const int br = std::max(3, h / 11);
    circle(aX, aY, br + 2, ink, true);
    circle(aX, aY, br, rgba(255, 70, 105, alpha), true);
    circle(aX - br * 2, aY + br * 2, br + 2, ink, true);
    circle(aX - br * 2, aY + br * 2, br, rgba(255, 70, 105, alpha), true);
}

void Renderer::drawScreenTransition(Screen screen, const Config& config) {
    if (!hasRenderedScreen_) {
        lastRenderedScreen_ = screen;
        hasRenderedScreen_ = true;
        screenChangedAt_ = SDL_GetTicks64();
        return;
    }
    if (screen != lastRenderedScreen_) {
        lastRenderedScreen_ = screen;
        screenChangedAt_ = SDL_GetTicks64();
    }
    if (!config.motionEnabled || !config.screenTransitions) return;
    const std::uint64_t elapsed = SDL_GetTicks64() - screenChangedAt_;
    constexpr std::uint64_t duration = 250;
    if (elapsed >= duration) return;
    const float t = clamp01(static_cast<float>(elapsed) / duration);
    const float eased = easeOutCubic(t);
    const float inv = 1.0f - eased;
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);

    // Brief cinematic shade followed by an Advance diagonal light sweep.
    fill({SIDEBAR_W, 0, W - SIDEBAR_W, H}, rgba(0,0,0,static_cast<Uint8>(118 * inv)));
    const int sweepX = SIDEBAR_W - 240 + static_cast<int>((W - SIDEBAR_W + 520) * eased);
    for (int i = -10; i <= 10; ++i) {
        const int distance = std::abs(i);
        const Uint8 a = static_cast<Uint8>(std::max(0, 46 - distance * 4) * inv);
        const SDL_Color c = i < 0 ? rgba(second.r,second.g,second.b,a) : rgba(accent.r,accent.g,accent.b,a);
        line(sweepX + i * 4, -30, sweepX - 170 + i * 4, H + 30, c);
    }
    const Uint8 runwayA = static_cast<Uint8>(72 * inv);
    roundedFill({CONTENT_X, HEADER_H - 4, static_cast<int>(320 + 280 * eased), 2}, 1,
                rgba(accent.r,accent.g,accent.b,runwayA));
}

SDL_Texture* Renderer::textTexture(TTF_Font* font, const std::string& value, SDL_Color color, int& w, int& h) {
    if (!font || value.empty()) { w = h = 0; return nullptr; }
    std::ostringstream key;
    key << reinterpret_cast<std::uintptr_t>(font) << '|' << colorKey(color) << '|' << value;
    const std::string cacheKey = key.str();
    if (auto found = textCache_.find(cacheKey); found != textCache_.end()) {
        found->second.lastUse = frame_;
        w = found->second.w; h = found->second.h;
        return found->second.texture;
    }
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, value.c_str(), color);
    if (!surface) { w = h = 0; return nullptr; }
    w = surface->w; h = surface->h;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_FreeSurface(surface);
    if (!texture) return nullptr;
    textCache_[cacheKey] = TextureEntry{texture, w, h, rgba(0,0,0), frame_};
    return texture;
}

void Renderer::text(TTF_Font* font, const std::string& value, int x, int y, SDL_Color color,
                    int maxWidth, bool centered) {
    const std::string shown = maxWidth > 0 ? ellipsize(font, value, maxWidth) : value;
    int w = 0, h = 0;
    SDL_Texture* texture = textTexture(font, shown, color, w, h);
    if (!texture) return;
    SDL_Rect dst{x, y, w, h};
    if (centered) dst.x -= w / 2;
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void Renderer::textRight(TTF_Font* font, const std::string& value, int rightX, int y, SDL_Color color) {
    int w = 0, h = 0;
    SDL_Texture* texture = textTexture(font, value, color, w, h);
    if (!texture) return;
    SDL_Rect dst{rightX - w, y, w, h};
    SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void Renderer::wrappedText(TTF_Font* font, const std::string& value, SDL_Rect rect, SDL_Color color, int lineGap) {
    if (!font || value.empty() || rect.w <= 0 || rect.h <= 0) return;
    std::istringstream words(value);
    std::string word, lineText;
    int y = rect.y;
    int lineH = 0, dummy = 0;
    TTF_SizeUTF8(font, "Ag", &dummy, &lineH);
    while (words >> word) {
        const std::string trial = lineText.empty() ? word : lineText + " " + word;
        int w = 0, h = 0;
        TTF_SizeUTF8(font, trial.c_str(), &w, &h);
        if (w <= rect.w) { lineText = trial; continue; }
        if (!lineText.empty()) {
            if (y + lineH > rect.y + rect.h) break;
            text(font, lineText, rect.x, y, color, rect.w);
            y += lineH + lineGap;
        }
        lineText = word;
    }
    if (!lineText.empty() && y + lineH <= rect.y + rect.h) text(font, lineText, rect.x, y, color, rect.w);
}

SDL_Texture* Renderer::loadImage(const std::string& path, int& w, int& h, SDL_Color* average) {
    w = h = 0;
    if (path.empty() || failedCoverCache_.count(path)) return nullptr;
    if (auto found = coverCache_.find(path); found != coverCache_.end()) {
        found->second.lastUse = frame_;
        w = found->second.w; h = found->second.h;
        if (average) *average = found->second.average;
        return found->second.texture;
    }

    SDL_Surface* source = IMG_Load(path.c_str());
    if (!source) { failedCoverCache_.insert(path); return nullptr; }

    SDL_Color avg = rgba(28, 28, 34);
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGBA32, 0);
    if (rgbaSurface) {
        if (SDL_MUSTLOCK(rgbaSurface)) SDL_LockSurface(rgbaSurface);
        auto* px = static_cast<Uint32*>(rgbaSurface->pixels);
        std::uint64_t rr = 0, gg = 0, bb = 0, count = 0;
        const int sx = std::max(1, rgbaSurface->w / 18);
        const int sy = std::max(1, rgbaSurface->h / 18);
        for (int y = sy / 2; y < rgbaSurface->h; y += sy) for (int x = sx / 2; x < rgbaSurface->w; x += sx) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(px[y * (rgbaSurface->pitch / 4) + x], rgbaSurface->format, &r, &g, &b, &a);
            if (a < 80) continue;
            rr += r; gg += g; bb += b; ++count;
        }
        if (SDL_MUSTLOCK(rgbaSurface)) SDL_UnlockSurface(rgbaSurface);
        if (count) avg = rgba(static_cast<Uint8>(rr / count), static_cast<Uint8>(gg / count), static_cast<Uint8>(bb / count));
        SDL_FreeSurface(rgbaSurface);
    }

    constexpr int maxW = 760, maxH = 720;
    SDL_Surface* surface = source;
    SDL_Surface* scaled = nullptr;
    if (source->w > maxW || source->h > maxH) {
        const float scale = std::min(static_cast<float>(maxW) / source->w, static_cast<float>(maxH) / source->h);
        const int dw = std::max(1, static_cast<int>(source->w * scale));
        const int dh = std::max(1, static_cast<int>(source->h * scale));
        scaled = SDL_CreateRGBSurfaceWithFormat(0, dw, dh, 32, SDL_PIXELFORMAT_RGBA32);
        if (scaled) {
            SDL_Rect dst{0, 0, dw, dh};
            if (SDL_BlitScaled(source, nullptr, scaled, &dst) == 0) surface = scaled;
        }
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    w = surface->w; h = surface->h;
    if (scaled) SDL_FreeSurface(scaled);
    SDL_FreeSurface(source);
    if (!texture) { failedCoverCache_.insert(path); return nullptr; }
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    coverCache_[path] = TextureEntry{texture, w, h, avg, frame_};
    if (average) *average = avg;
    return texture;
}

SDL_Texture* Renderer::loadCover(const Game& game, int& w, int& h, SDL_Color* average) {
    return loadImage(game.coverPath, w, h, average);
}

void Renderer::pruneCoverCache() {
    if (coverCache_.size() <= 30) return;
    std::vector<std::pair<std::string, std::uint64_t>> ages;
    for (const auto& [path, entry] : coverCache_) ages.emplace_back(path, entry.lastUse);
    std::sort(ages.begin(), ages.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    while (coverCache_.size() > 22 && !ages.empty()) {
        auto it = coverCache_.find(ages.front().first);
        if (it != coverCache_.end()) { SDL_DestroyTexture(it->second.texture); coverCache_.erase(it); }
        ages.erase(ages.begin());
    }
}

void Renderer::pruneTextCache() {
    if (textCache_.size() <= 220) return;
    std::vector<std::pair<std::string, std::uint64_t>> ages;
    for (const auto& [key, entry] : textCache_) ages.emplace_back(key, entry.lastUse);
    std::sort(ages.begin(), ages.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    const std::size_t remove = textCache_.size() - 160;
    for (std::size_t i = 0; i < remove; ++i) {
        auto it = textCache_.find(ages[i].first);
        if (it != textCache_.end()) { SDL_DestroyTexture(it->second.texture); textCache_.erase(it); }
    }
}

void Renderer::drawBaseBackdrop(const Config& config) {
    fill({0, 0, W, H}, rgba(0, 0, 0));
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const std::uint64_t now = SDL_GetTicks64();

    // Deep OLED-black foundation with two large color fields.
    for (int i = 0; i < 9; ++i) {
        const int pad = i * 42;
        roundedFill({W - 470 - pad / 2, -180 + pad / 3, 560 + pad, 320 + pad}, 120,
                    rgba(accent.r, accent.g, accent.b, static_cast<Uint8>(12 - std::min(i, 11))));
        roundedFill({SIDEBAR_W - 190 - pad / 2, H - 230 - pad / 4, 430 + pad, 330 + pad / 2}, 110,
                    rgba(second.r, second.g, second.b, static_cast<Uint8>(10 - std::min(i, 9))));
    }

    // Tiny drifting light points add depth without giving up true black.
    const int drift = config.motionEnabled ? static_cast<int>((now / 38) % 36) : 0;
    for (int i = 0; i < 24; ++i) {
        const int px = SIDEBAR_W + 40 + ((i * 167 + drift * (i % 3 + 1)) % (W - SIDEBAR_W - 80));
        const int py = 34 + ((i * 83 + drift / 2) % (H - 90));
        const Uint8 a = static_cast<Uint8>(5 + (i % 4) * 3);
        circle(px, py, (i % 5 == 0) ? 2 : 1, i % 2 ? rgba(accent.r,accent.g,accent.b,a) : rgba(second.r,second.g,second.b,a), true);
    }

    // Signature diagonal traces; nearly invisible until viewed on OLED.
    for (int i = 0; i < 4; ++i) {
        const int x = SIDEBAR_W + 240 + i * 270;
        line(x, 0, x + 170, H, rgba(i % 2 ? second.r : accent.r,
                                     i % 2 ? second.g : accent.g,
                                     i % 2 ? second.b : accent.b, 6));
    }
}

void Renderer::drawDynamicBackdrop(const Game* game, const Config& config, int strength) {
    drawBaseBackdrop(config);
    if (!game || !config.dynamicBackdrop || (game->coverPath.empty() && game->bannerPath.empty())) return;
    strength = strength < 0 ? config.backdropIntensity : strength;
    strength = std::clamp(strength, 0, 100);

    int iw = 0, ih = 0;
    SDL_Color avg{};
    SDL_Texture* texture = !game->bannerPath.empty()
        ? loadImage(game->bannerPath, iw, ih, &avg)
        : loadCover(*game, iw, ih, &avg);
    if (!texture || iw <= 0 || ih <= 0) return;

    const int contentW = W - SIDEBAR_W;
    const float scale = std::max(static_cast<float>(contentW) / iw, static_cast<float>(H) / ih);
    const int dw = static_cast<int>(iw * scale);
    const int dh = static_cast<int>(ih * scale);
    const int baseX = SIDEBAR_W + (contentW - dw) / 2;
    const int baseY = (H - dh) / 2;

    // Multiple translucent copies simulate a soft-focus backdrop without a blur shader.
    const Uint8 ghostAlpha = static_cast<Uint8>(13 + strength / 5);
    SDL_SetTextureAlphaMod(texture, ghostAlpha);
    for (int oy : {-24, -8, 8, 24}) for (int ox : {-30, -10, 10, 30}) {
        SDL_Rect dst{baseX + ox, baseY + oy, dw, dh};
        SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    }
    SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(20 + strength / 3));
    SDL_Rect center{baseX, baseY, dw, dh};
    SDL_RenderCopy(renderer_, texture, nullptr, &center);
    SDL_SetTextureAlphaMod(texture, 255);

    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const SDL_Color adaptive = config.adaptiveAccent ? mixColor(accent, avg, 0.44f) : accent;

    // Color atmosphere and readable black veil.
    fill({SIDEBAR_W, 0, contentW, H}, rgba(adaptive.r, adaptive.g, adaptive.b, static_cast<Uint8>(16 + strength / 5)));
    fill({SIDEBAR_W, 0, contentW, H}, rgba(second.r, second.g, second.b, static_cast<Uint8>(5 + strength / 12)));
    fill({SIDEBAR_W, 0, contentW, H}, rgba(0, 0, 0, static_cast<Uint8>(202 - strength / 2)));

    // Top-right bloom from the selected game color.
    for (int i = 0; i < 7; ++i) {
        const int pad = i * 32;
        roundedFill({W - 360 - pad / 2, 42 - pad / 3, 300 + pad, 160 + pad}, 90,
                    rgba(adaptive.r, adaptive.g, adaptive.b, static_cast<Uint8>(12 - i)));
    }

    // Readability ramps keep text crisp while preserving the atmosphere.
    for (int i = 0; i < 8; ++i) {
        fill({SIDEBAR_W, H - 260 + i * 28, contentW, 32}, rgba(0, 0, 0, static_cast<Uint8>(18 + i * 16)));
    }
    for (int i = 0; i < 6; ++i) {
        fill({SIDEBAR_W, i * 18, contentW, 22}, rgba(0, 0, 0, static_cast<Uint8>(30 - i * 4)));
    }
}

void Renderer::drawSidebarIcon(int cx, int cy, int type, SDL_Color c) {
    static const std::array<const char*, 7> iconNames = {
        "house", "layout-grid", "layers", "star", "history", "search", "settings"
    };
    const int safeType = std::clamp(type, 0, 6);
    int iw = 0, ih = 0;
    const std::string sdPath = std::string("sdmc:/switch/advance/assets/icons/") + iconNames[safeType] + ".png";
    const std::string localPath = std::string("assets/icons/") + iconNames[safeType] + ".png";
    SDL_Texture* icon = loadImage(sdPath, iw, ih);
    if (!icon) icon = loadImage(localPath, iw, ih);
    if (icon && iw > 0 && ih > 0) {
        const int size = safeType == 3 ? 39 : 40;
        SDL_Rect dst{cx - size / 2, cy - size / 2, size, size};
        SDL_SetTextureColorMod(icon, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(icon, c.a);
        SDL_RenderCopy(renderer_, icon, nullptr, &dst);
        SDL_SetTextureColorMod(icon, 255, 255, 255);
        SDL_SetTextureAlphaMod(icon, 255);
        return;
    }

    // Built-in fallback if packaged assets are missing or unreadable.
    constexpr double pi = 3.14159265358979323846;
    switch (safeType) {
        case 0:
            line(cx - 18, cy + 1, cx, cy - 17, c); line(cx, cy - 17, cx + 18, cy + 1, c);
            roundedOutline({cx - 15, cy - 1, 30, 23}, 8, c, 3); break;
        case 1:
            for (int yy : {-11, 11}) for (int xx : {-11, 11}) roundedOutline({cx + xx - 7, cy + yy - 7, 14, 14}, 4, c, 3);
            break;
        case 2:
            roundedOutline({cx - 18, cy - 13, 28, 20}, 6, c, 3); roundedOutline({cx - 8, cy - 3, 28, 20}, 6, c, 3); break;
        case 3: {
            int px[10], py[10];
            for (int i = 0; i < 10; ++i) {
                const double a = -pi / 2.0 + i * pi / 5.0; const double rr = (i % 2 == 0) ? 18.0 : 8.5;
                px[i] = cx + static_cast<int>(std::cos(a) * rr); py[i] = cy + static_cast<int>(std::sin(a) * rr);
            }
            for (int i = 0; i < 10; ++i) line(px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], c);
            break;
        }
        case 4:
            circle(cx, cy, 18, c, false); line(cx, cy, cx, cy - 9, c); line(cx, cy, cx + 9, cy + 5, c); break;
        case 5:
            circle(cx - 4, cy - 4, 13, c, false); line(cx + 6, cy + 6, cx + 19, cy + 19, c); break;
        default:
            circle(cx, cy, 12, c, false); circle(cx, cy, 4, c, false);
            for (int i = 0; i < 6; ++i) { const double a = i * pi / 3.0; line(cx + static_cast<int>(std::cos(a) * 14), cy + static_cast<int>(std::sin(a) * 14), cx + static_cast<int>(std::cos(a) * 20), cy + static_cast<int>(std::sin(a) * 20), c); }
            break;
    }
}

void Renderer::drawSidebar(Screen screen, Shelf shelf, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const SDL_Color secondary = secondaryColor(config);
    const std::uint64_t now = SDL_GetTicks64();
    if (sidebarFrameAt_ == 0) sidebarFrameAt_ = now;
    const float dt = std::min(0.05f, static_cast<float>(now - sidebarFrameAt_) / 1000.0f);
    sidebarFrameAt_ = now;
    const float target = sidebarFocused_ ? 1.0f : 0.0f;
    if (!sidebarMotionEnabled_) sidebarProgress_ = target;
    else {
        const float speed = sidebarFocused_ ? 9.0f : 10.4f;
        if (sidebarProgress_ < target) sidebarProgress_ = std::min(target, sidebarProgress_ + dt * speed);
        else if (sidebarProgress_ > target) sidebarProgress_ = std::max(target, sidebarProgress_ - dt * speed);
    }

    fill({0, 0, SIDEBAR_W, H}, rgba(2, 2, 5, 252));
    fill({0, 0, 5, H}, accent);
    fill({5, 0, 5, H}, rgba(secondary.r,secondary.g,secondary.b,18));
    line(SIDEBAR_W - 1, 0, SIDEBAR_W - 1, H, rgba(43, 43, 53));

    // Brand tile — prefers the packaged sidebar icon, falls back to the vector handheld mark.
    roundedFill({10, 8, 78, 78}, 26, rgba(19, 15, 48, 255));
    roundedFill({12, 10, 74, 74}, 24, rgba(44, 28, 108, 255));
    roundedOutline({12, 10, 74, 74}, 24, rgba(255,255,255,150), 1);
    roundedFill({16, 14, 66, 66}, 22, rgba(97, 74, 219, 112));
    int bw = 0, bh = 0;
    SDL_Texture* brand = loadImage("sdmc:/switch/advance/assets/sidebar-mark.png", bw, bh);
    if (!brand) brand = loadImage("sdmc:/switch/advance/assets/store-icon.png", bw, bh);
    if (!brand) brand = loadImage("assets/sidebar-mark.png", bw, bh);
    if (!brand) brand = loadImage("assets/store-icon.png", bw, bh);
    if (brand && bw > 0 && bh > 0) {
        SDL_Rect dst{15, 13, 68, 68};
        SDL_SetTextureAlphaMod(brand, 255);
        SDL_RenderCopy(renderer_, brand, nullptr, &dst);
    } else {
        drawBrandHandheld(18, 19, 58, 42, accent);
    }
    text(fonts_.tiny, "ADV", 49, 90, rgba(220,220,230), 0, true);

    int active = 1;
    if (screen == Screen::Home) active = 0;
    else if (screen == Screen::Collections) active = 2;
    else if (screen == Screen::Settings || screen == Screen::About) active = 6;
    else if (shelf == Shelf::Favorites) active = 3;
    else if (shelf == Shelf::Recent) active = 4;
    else if (shelf == Shelf::Search) active = 5;

    const std::array<int, 7> ys = {146, 226, 306, 386, 466, 546, 642};
    for (int i = 0; i < 7; ++i) {
        const bool on = i == active;
        const bool focus = sidebarFocused_ && i == sidebarSelected_;
        const int cx = 49, cy = ys[i];

        if (focus) {
            roundedFill({8, cy - 35, 82, 70}, 25, rgba(accent.r,accent.g,accent.b,38));
            roundedFill({12, cy - 31, 74, 62}, 23, rgba(accent.r,accent.g,accent.b,98));
            roundedFill({15, cy - 28, 68, 56}, 21, rgba(255,255,255,14));
            roundedOutline({12, cy - 31, 74, 62}, 23, rgba(255,255,255,214), 2);
        } else if (on) {
            roundedFill({11, cy - 32, 76, 64}, 23, rgba(accent.r,accent.g,accent.b,26));
            roundedOutline({11, cy - 32, 76, 64}, 23, rgba(accent.r,accent.g,accent.b,150), 1);
        }
        if (on) roundedFill({0, cy - 22, 5, 44}, 2, mixColor(accent, secondary, 0.22f));
        drawSidebarIcon(cx, cy, i, (on || focus) ? rgba(255,255,255) : rgba(160,160,176));
        if (on && !focus) circle(79, cy - 20, 3, rgba(secondary.r,secondary.g,secondary.b,220), true);
    }

    // Expansion hint.
    roundedFill({24, H - 28, 50, 16}, 8, rgba(255,255,255,8));
    line(39, H - 20, 46, H - 20, rgba(126,126,140));
    line(52, H - 20, 59, H - 20, rgba(126,126,140));
    line(45, H - 24, 52, H - 24, rgba(126,126,140));
}

void Renderer::drawSidebarOverlay(Screen screen, Shelf shelf, const Config& config) {
    if (sidebarProgress_ <= 0.001f) return;
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const float t = easeOutCubic(sidebarProgress_);
    const int drawerW = SIDEBAR_W + static_cast<int>((SIDEBAR_EXPANDED_W - SIDEBAR_W) * t);
    const Uint8 dim = static_cast<Uint8>(126 * t);

    fill({SIDEBAR_W, 0, W - SIDEBAR_W, H}, rgba(0,0,0,dim));
    for (int i = 0; i < 12; ++i) {
        const Uint8 a = static_cast<Uint8>(std::max(0, 34 - i * 3) * t);
        fill({drawerW + i * 3, 0, 3, H}, rgba(0,0,0,a));
    }
    fill({0, 0, drawerW, H}, rgba(4,4,8, static_cast<Uint8>(250 * t)));
    fill({0, 0, 5, H}, accent);
    fill({5, 0, 6, H}, rgba(255,255,255,10));
    line(drawerW - 1, 0, drawerW - 1, H, rgba(74,74,86, static_cast<Uint8>(185 * t)));

    roundedFill({12, 10, 76, 76}, 25, rgba(21, 17, 54, 255));
    roundedFill({14, 12, 72, 72}, 23, rgba(111, 82, 235, 255));
    roundedOutline({14, 12, 72, 72}, 23, rgba(255,255,255,180), 1);
    int bw = 0, bh = 0;
    SDL_Texture* brand = loadImage("sdmc:/switch/advance/assets/sidebar-mark.png", bw, bh);
    if (!brand) brand = loadImage("sdmc:/switch/advance/assets/store-icon.png", bw, bh);
    if (!brand) brand = loadImage("assets/sidebar-mark.png", bw, bh);
    if (!brand) brand = loadImage("assets/store-icon.png", bw, bh);
    if (brand && bw > 0 && bh > 0) {
        SDL_Rect dst{17, 15, 66, 66};
        SDL_SetTextureAlphaMod(brand, 255);
        SDL_RenderCopy(renderer_, brand, nullptr, &dst);
    } else {
        drawBrandHandheld(19, 22, 60, 42, accent);
    }
    if (t > 0.08f) {
        const Uint8 a = static_cast<Uint8>(255 * clamp01((t - 0.08f) / 0.40f));
        text(fonts_.medium, "ADVANCE", 104, 16, rgba(255,255,255,a), 226);
        text(fonts_.tiny, "DEFINITIVE GBA FRONTEND", 105, 46, rgba(174,174,188,a), 250);
        roundedFill({104, 61, 168, 23}, 11, rgba(accent.r,accent.g,accent.b, static_cast<Uint8>(58 * t)));
        text(fonts_.tiny, nickname_, 188, 65, rgba(255,255,255,a), 148, true);
        textRight(fonts_.tiny, "v0.4", drawerW - 22, 65, rgba(111,111,126,a));
    }

    if (t > 0.16f) {
        const Uint8 a = static_cast<Uint8>(205 * clamp01((t - 0.16f) / 0.36f));
        text(fonts_.tiny, "NAVIGATION", 18, 98, rgba(122,122,136,a));
        line(99, 106, drawerW - 22, 106, rgba(62,62,74,static_cast<Uint8>(110 * t)));
    }

    const std::array<int, 7> ys = {146, 226, 306, 386, 466, 546, 642};
    const std::array<const char*, 7> labels = {"Home", "Library", "Collections", "Favorites", "Recent", "Search", "Settings"};
    const std::array<const char*, 7> subs = {
        "Your personal landing screen", "Browse every game in your library", "Curated shelves and custom sets",
        "The games you saved", "Continue recent sessions", "Find titles, authors, and tags", "Theme, layout, and behavior"
    };

    int active = 1;
    if (screen == Screen::Home) active = 0;
    else if (screen == Screen::Collections) active = 2;
    else if (screen == Screen::Settings || screen == Screen::About) active = 6;
    else if (shelf == Shelf::Favorites) active = 3;
    else if (shelf == Shelf::Recent) active = 4;
    else if (shelf == Shelf::Search) active = 5;

    for (int i = 0; i < 7; ++i) {
        const bool focus = i == sidebarSelected_;
        const bool on = i == active;
        const int top = ys[i] - 36;
        if (focus) {
            roundedFill({14, top, drawerW - 28, 72}, 24, rgba(accent.r,accent.g,accent.b,214));
            roundedFill({14, top + 12, 5, 48}, 2, rgba(255,255,255,220));
            roundedFill({18, top + 5, drawerW - 36, 62}, 20, rgba(255,255,255,16));
            roundedOutline({14, top, drawerW - 28, 72}, 24, rgba(255,255,255,190), 1);
        } else if (on) {
            roundedFill({14, top, drawerW - 28, 72}, 24, rgba(accent.r,accent.g,accent.b,34));
            roundedOutline({14, top, drawerW - 28, 72}, 24, rgba(accent.r,accent.g,accent.b,116), 1);
        }
        drawSidebarIcon(52, ys[i], i, (focus || on) ? rgba(255,255,255) : rgba(172,172,184));
        if (t > 0.18f) {
            const Uint8 a = static_cast<Uint8>(255 * clamp01((t - 0.18f) / 0.40f));
            text(fonts_.small, labels[i], 106, top + 11, (focus || on) ? rgba(255,255,255,a) : rgba(225,225,232,a), 236);
            text(fonts_.tiny, subs[i], 106, top + 41, focus ? rgba(255,242,244,a) : rgba(129,129,143,a), 250);
            if (on && !focus) {
                roundedFill({drawerW - 81, ys[i] - 10, 58, 20}, 9, rgba(second.r, second.g, second.b, 46));
                text(fonts_.tiny, "CURRENT", drawerW - 52, ys[i] - 9, rgba(second.r,second.g,second.b,a), 0, true);
            }
        }
    }

    if (t > 0.56f) {
        const Uint8 a = static_cast<Uint8>(235 * clamp01((t - 0.56f) / 0.30f));
        roundedFill({18, 686, drawerW - 36, 24}, 12, rgba(17,17,24,a));
        text(fonts_.tiny, "A  OPEN", 32, 690, rgba(238,238,244,a));
        textRight(fonts_.tiny, "B / →  CLOSE", drawerW - 30, 690, rgba(155,155,169,a));
    }
}

void Renderer::drawAvatar(int x, int y, int diameter, const Config& config) {
    const SDL_Color accent = accentColor(config);
    circle(x + diameter / 2, y + diameter / 2, diameter / 2 + 2, rgba(accent.r, accent.g, accent.b, 170), true);
    if (avatarTexture_) {
        SDL_Rect dst{x, y, diameter, diameter};
        SDL_RenderCopy(renderer_, avatarTexture_, nullptr, &dst);
    } else {
        circle(x + diameter / 2, y + diameter / 2, diameter / 2, rgba(24,24,30), true);
        text(fonts_.body, nickname_.empty() ? "P" : std::string(1, nickname_[0]), x + diameter / 2, y + 7,
             rgba(255,255,255), 0, true);
    }
}

void Renderer::drawWifiIcon(int x, int y, SDL_Color color) {
    if (!connected_) { circle(x, y + 10, 2, rgba(110,110,120), true); return; }
    const int bars = static_cast<int>(std::max<std::uint32_t>(1, wifiStrength_));
    for (int i = 0; i < 4; ++i) {
        const int h = 4 + i * 4;
        roundedFill({x + i * 5, y + 16 - h, 3, h}, 1, i < bars ? color : rgba(75,75,85));
    }
}

void Renderer::drawBatteryIcon(int x, int y, SDL_Color color) {
    roundedOutline({x, y, 28, 13}, 4, color, 1);
    fill({x + 29, y + 4, 3, 5}, color);
    const int inner = static_cast<int>(24 * batteryPercent_ / 100);
    if (inner > 0) roundedFill({x + 2, y + 2, inner, 9}, 2,
        batteryPercent_ <= 15 ? rgba(255,75,75) : color);
    if (charging_) text(fonts_.tiny, "+", x + 12, y - 4, rgba(255,255,255));
}

std::string Renderer::formatClock() const {
    std::time_t now = std::time(nullptr);
    std::tm* tm = std::localtime(&now);
    if (!tm) return "--:--";
    char buf[16]; std::strftime(buf, sizeof(buf), "%H:%M", tm); return buf;
}

void Renderer::drawProfileChip(const Config& config, bool compact) {
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const int w = compact ? 168 : 198;
    const int x = W - w - 16;
    const int y = 10;
    const int h = 54;

    roundedFill({x + 3, y + 5, w, h}, 18, rgba(0,0,0,120));
    roundedFill({x, y, w, h}, 18, rgba(9,9,14,232));
    roundedOutline({x, y, w, h}, 18, rgba(74,74,88), 1);
    roundedFill({x + 1, y + 1, w - 2, 2}, 1, rgba(255,255,255,22));
    roundedFill({x + w - 5, y + 7, 3, h - 14}, 2, mixColor(accent, second, 0.22f));
    drawAvatar(x + 8, y + 7, 40, config);
    text(fonts_.small, nickname_, x + 56, y + 8, rgba(252,252,255), w - 70);
    text(fonts_.tiny, profileSource_ == "Switch profile" ? "SWITCH PROFILE" : "ADVANCE PROFILE",
         x + 56, y + 32, rgba(132,132,146), w - 70);
}

void Renderer::drawTopChrome(const std::string& eyebrow, const std::string& title, const std::string& subtitle,
                             const Config& config, bool compactProfile) {
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    fill({SIDEBAR_W, 0, W - SIDEBAR_W, HEADER_H}, rgba(0,0,0,184));
    line(SIDEBAR_W, HEADER_H - 1, W, HEADER_H - 1, rgba(42,42,53));

    // Left identity stack.
    roundedFill({CONTENT_X, 8, 184, 20}, 10, rgba(accent.r,accent.g,accent.b,32));
    roundedOutline({CONTENT_X, 8, 184, 20}, 10, rgba(accent.r,accent.g,accent.b,58), 1);
    text(fonts_.tiny, eyebrow, CONTENT_X + 11, 9, accent, 390);
    TTF_Font* chromeTitleFont = title.size() <= 24 ? fonts_.large : fonts_.medium;
    text(chromeTitleFont, title, CONTENT_X, title.size() <= 24 ? 27 : 30, rgba(255,255,255), 590);
    if (!subtitle.empty()) {
        roundedFill({CONTENT_X + 435, 39, 280, 24}, 11, rgba(255,255,255,9));
        roundedOutline({CONTENT_X + 435, 39, 280, 24}, 11, rgba(255,255,255,18), 1);
        text(fonts_.tiny, subtitle, CONTENT_X + 447, 43, rgba(162,162,176), 256);
    }

    // Compact system capsule instead of three floating widgets.
    if (config.showSystemStatus) {
        const int profileW = compactProfile ? 168 : 198;
        const int statusW = compactProfile ? 152 : 166;
        const int profileX = W - profileW - 16;
        const int statusX = profileX - statusW - 10;
        roundedFill({statusX + 2, 15, statusW, 43}, 16, rgba(0,0,0,100));
        roundedFill({statusX, 12, statusW, 43}, 16, rgba(8,8,13,222));
        roundedOutline({statusX, 12, statusW, 43}, 16, rgba(66,66,79), 1);
        drawWifiIcon(statusX + 13, 28, connected_ ? rgba(238,238,244) : rgba(103,103,116));
        text(fonts_.small, formatClock(), statusX + 48, 20, rgba(248,248,252));
        drawBatteryIcon(statusX + statusW - 56, 27, batteryPercent_ <= 15 ? rgba(255,86,86) : rgba(232,232,238));
        roundedFill({statusX + 8, 49, statusW - 16, 2}, 1, rgba(second.r,second.g,second.b,28));
    }
    drawProfileChip(config, compactProfile);

    // Accent runway ties the content area to the drawer visually.
    roundedFill({CONTENT_X, HEADER_H - 4, 132, 2}, 1, accent);
    roundedFill({CONTENT_X + 136, HEADER_H - 4, 42, 2}, 1, rgba(second.r,second.g,second.b,155));
}

void Renderer::drawButtonHint(int& x, int y, const std::string& button, const std::string& label) {
    const int badgeW = button.size() > 3 ? 48 : (button.size() > 1 ? 36 : 24);
    int tw = 0, th = 0;
    TTF_SizeUTF8(fonts_.tiny, label.c_str(), &tw, &th);
    const int groupW = badgeW + tw + 22;
    roundedFill({x, y - 3, groupW, 27}, 13, rgba(255,255,255,7));
    roundedOutline({x, y - 3, groupW, 27}, 13, rgba(255,255,255,13), 1);
    roundedFill({x + 4, y, badgeW - 4, 21}, 10, rgba(235,235,242));
    text(fonts_.tiny, button, x + 2 + badgeW / 2, y + 1, rgba(13,13,17), 0, true);
    text(fonts_.tiny, label, x + badgeW + 7, y + 1, rgba(201,201,211));
    x += groupW + 7;
}

void Renderer::drawFooter(const std::vector<std::pair<std::string, std::string>>& hints) {
    fill({SIDEBAR_W, H - FOOTER_H, W - SIDEBAR_W, FOOTER_H}, rgba(3,3,7,248));
    line(SIDEBAR_W, H - FOOTER_H, W, H - FOOTER_H, rgba(45,45,56));
    roundedFill({CONTENT_X - 8, H - FOOTER_H + 3, W - CONTENT_X - CONTENT_R + 8, FOOTER_H - 6}, 16, rgba(255,255,255,3));
    int x = CONTENT_X;
    const int y = H - FOOTER_H + 9;
    for (const auto& hint : hints) {
        if (x > W - 170) break;
        drawButtonHint(x, y, hint.first, hint.second);
    }
    roundedFill({W - CONTENT_R - 92, y - 2, 92, 24}, 11, rgba(255,255,255,6));
    text(fonts_.tiny, "ALPHA 0.4", W - CONTENT_R - 46, y + 1, rgba(96,96,110), 0, true);
}

void Renderer::markFocus(const Game* game) {
    const std::string path = game ? game->path : std::string{};
    if (path != lastFocusPath_) {
        lastFocusPath_ = path;
        selectionChangedAt_ = SDL_GetTicks64();
    }
}

void Renderer::drawPlaceholderCover(const Game& game, SDL_Rect rect, const Config& config) {
    const SDL_Color accent = accentColor(config);
    roundedFill(rect, 10, rgba(7,7,11,245));
    roundedOutline(rect, 10, rgba(55,55,66), 1);
    for (int i = 0; i < 5; ++i) {
        const int inset = 12 + i * 10;
        if (rect.w - inset * 2 <= 0 || rect.h - inset * 2 <= 0) break;
        roundedOutline({rect.x + inset, rect.y + inset, rect.w - inset * 2, rect.h - inset * 2}, 16,
                       rgba(accent.r, accent.g, accent.b, static_cast<Uint8>(55 - i * 8)), 1);
    }
    int markW = 0, markH = 0;
    SDL_Texture* mark = loadImage("sdmc:/switch/advance/assets/sidebar-mark.png", markW, markH);
    if (!mark) mark = loadImage("assets/sidebar-mark.png", markW, markH);
    if (mark && markW > 0 && markH > 0) {
        const int logo = std::min({rect.w - 30, rect.h / 2, 92});
        SDL_Rect dst{rect.x + rect.w / 2 - logo / 2, rect.y + rect.h / 2 - logo / 2 - 15, logo, logo};
        SDL_SetTextureAlphaMod(mark, 210);
        SDL_RenderCopy(renderer_, mark, nullptr, &dst);
        SDL_SetTextureAlphaMod(mark, 255);
    } else {
        const int logoW = std::min(rect.w - 24, 92);
        const int logoH = std::min(rect.h / 2, 62);
        drawBrandHandheld(rect.x + rect.w / 2 - logoW / 2, rect.y + rect.h / 2 - logoH / 2 - 18,
                          logoW, logoH, accent, 235);
    }
    text(fonts_.tiny, "ARTWORK MISSING", rect.x + rect.w / 2, rect.y + rect.h / 2 + 34,
         rgba(160,160,173), rect.w - 18, true);
    if (rect.h > 150)
        text(fonts_.tiny, game.shortTitle.empty() ? game.title : game.shortTitle, rect.x + rect.w / 2,
             rect.y + rect.h / 2 + 56, rgba(112,112,126), rect.w - 24, true);
}

void Renderer::drawCoverCard(const Game& game, SDL_Rect rect, bool selected, const Config& config,
                             float focusProgress, bool compact) {
    const SDL_Color accent = accentColor(config);
    const float eased = easeOutBack(focusProgress);
    const int grow = selected ? (config.motionEnabled ? static_cast<int>((compact ? 4.0f : 6.0f) * eased) : (compact ? 4 : 6)) : 0;
    SDL_Rect card{rect.x - grow, rect.y - grow, rect.w + grow * 2, rect.h + grow * 2};

    int iw = 0, ih = 0;
    SDL_Color avg = accent;
    SDL_Texture* cover = loadCover(game, iw, ih, &avg);
    const SDL_Color cardAccent = config.adaptiveAccent && cover ? mixColor(accent, avg, 0.34f) : accent;

    if (selected) {
        const float pulse = config.motionEnabled ? 0.5f + 0.5f * std::sin(SDL_GetTicks64() / 210.0f) : 0.55f;
        roundedFill({card.x - 16, card.y - 16, card.w + 32, card.h + 32}, 26,
                    rgba(cardAccent.r,cardAccent.g,cardAccent.b,static_cast<Uint8>(12 + pulse * 10)));
        roundedFill({card.x - 10, card.y - 10, card.w + 20, card.h + 20}, 21,
                    rgba(cardAccent.r,cardAccent.g,cardAccent.b,static_cast<Uint8>(34 + pulse * 18)));
    }
    roundedFill({card.x + 5, card.y + 8, card.w, card.h}, 13, rgba(0,0,0,188));
    roundedFill(card, 13, rgba(7,7,10,248));
    roundedFill({card.x + 2, card.y + 2, card.w - 4, 2}, 1, rgba(255,255,255,18));

    SDL_Rect art{card.x + 3, card.y + 3, card.w - 6, card.h - 6};
    if (cover && iw > 0 && ih > 0) {
        // Atmospheric crop behind the full cover fills unused aspect-ratio space without cutting the box art.
        const float fillScale = std::max(static_cast<float>(art.w) / iw, static_cast<float>(art.h) / ih);
        const int fw = std::max(1, static_cast<int>(iw * fillScale));
        const int fh = std::max(1, static_cast<int>(ih * fillScale));
        SDL_Rect fillDst{art.x + (art.w - fw) / 2, art.y + (art.h - fh) / 2, fw, fh};
        SDL_RenderSetClipRect(renderer_, &art);
        SDL_SetTextureAlphaMod(cover, selected ? 78 : 54);
        SDL_RenderCopy(renderer_, cover, nullptr, &fillDst);
        SDL_SetTextureAlphaMod(cover, 255);
        fill(art, rgba(2,2,6, selected ? 72 : 104));
        SDL_RenderSetClipRect(renderer_, nullptr);

        const float scale = std::min(static_cast<float>(art.w) / iw, static_cast<float>(art.h) / ih);
        const int dw = std::max(1, static_cast<int>(iw * scale));
        const int dh = std::max(1, static_cast<int>(ih * scale));
        SDL_Rect dst{art.x + (art.w - dw) / 2, art.y + (art.h - dh) / 2, dw, dh};
        SDL_RenderCopy(renderer_, cover, nullptr, &dst);

        if (selected && config.motionEnabled) {
            const std::uint64_t elapsed = SDL_GetTicks64() - selectionChangedAt_;
            if (elapsed < SHINE_ANIM_MS) {
                const float t = static_cast<float>(elapsed) / SHINE_ANIM_MS;
                const int shineX = art.x - 34 + static_cast<int>((art.w + 68) * t);
                SDL_RenderSetClipRect(renderer_, &art);
                for (int i = -7; i <= 7; ++i) {
                    const Uint8 a = static_cast<Uint8>(std::max(0, 34 - std::abs(i) * 4));
                    line(shineX + i, art.y, shineX + i + 28, art.y + art.h, rgba(255,255,255,a));
                }
                SDL_RenderSetClipRect(renderer_, nullptr);
            }
        }
    } else drawPlaceholderCover(game, art, config);

    if (config.showCoverLabels && cover) {
        roundedFill({card.x + 5, card.y + card.h - 32, card.w - 10, 27}, 8, rgba(0,0,0,220));
        text(fonts_.tiny, game.shortTitle.empty() ? game.title : game.shortTitle, card.x + 10, card.y + card.h - 29,
             rgba(255,255,255), card.w - 20);
    } else if (selected && !compact && cover) {
        const std::string label = game.shortTitle.empty() ? game.title : game.shortTitle;
        roundedFill({card.x + 8, card.y + card.h - 31, card.w - 16, 25}, 11, rgba(2,2,5,218));
        roundedOutline({card.x + 8, card.y + card.h - 31, card.w - 16, 25}, 11, rgba(cardAccent.r,cardAccent.g,cardAccent.b,90), 1);
        text(fonts_.tiny, label, card.x + card.w / 2, card.y + card.h - 28, rgba(255,255,255), card.w - 28, true);
    }

    const std::int64_t now = static_cast<std::int64_t>(std::time(nullptr));
    const bool newlyAdded = game.addedAt > 0 && now >= game.addedAt && (now - game.addedAt) <= 7 * 24 * 60 * 60;
    if (newlyAdded && game.launches == 0) {
        roundedFill({card.x + 7, card.y + card.h - 28, 42, 20}, 9, rgba(cardAccent.r,cardAccent.g,cardAccent.b,225));
        text(fonts_.tiny, "NEW", card.x + 28, card.y + card.h - 27, rgba(255,255,255), 0, true);
    }
    if (game.favorite) {
        roundedFill({card.x + card.w - 31, card.y + 7, 24, 24}, 12, rgba(3,3,6,225));
        text(fonts_.tiny, "★", card.x + card.w - 19, card.y + 9, rgba(255,219,72), 0, true);
    }
    if (game.completed) {
        roundedFill({card.x + 7, card.y + 7, 26, 22}, 10, rgba(3,3,6,225));
        text(fonts_.tiny, "✓", card.x + 20, card.y + 8, rgba(80,234,147), 0, true);
    }
    if (selected) {
        roundedOutline(card, 13, rgba(255,255,255,230), 2);
        roundedOutline({card.x - 3, card.y - 3, card.w + 6, card.h + 6}, 16, rgba(cardAccent.r,cardAccent.g,cardAccent.b,230), 2);
        roundedFill({card.x + 12, card.y + card.h - 7, card.w - 24, 4}, 2, cardAccent);
    } else roundedOutline(card, 13, rgba(47,47,58), 1);
}

std::string Renderer::formatPlayTime(std::uint64_t seconds) const {
    if (seconds < 60) return seconds == 0 ? "Not tracked yet" : "< 1 min";
    const std::uint64_t hours = seconds / 3600;
    const std::uint64_t minutes = (seconds % 3600) / 60;
    std::ostringstream ss;
    if (hours) ss << hours << "h ";
    ss << minutes << "m";
    return ss.str();
}

std::string Renderer::formatTimestamp(std::int64_t ts) const {
    if (ts <= 0) return "Not played yet";
    std::time_t t = static_cast<std::time_t>(ts);
    std::tm* tm = std::localtime(&t);
    if (!tm) return "Unknown";
    char buf[64]; std::strftime(buf, sizeof(buf), "%d %b %Y · %H:%M", tm); return buf;
}

std::string Renderer::join(const std::vector<std::string>& values, const std::string& separator, std::size_t max) const {
    std::ostringstream ss;
    const std::size_t n = std::min(max, values.size());
    for (std::size_t i = 0; i < n; ++i) { if (i) ss << separator; ss << values[i]; }
    return ss.str();
}

void Renderer::drawHero(const Game* game, const std::vector<Game>& games, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    const int x = CONTENT_X, y = HEADER_H + 8, w = W - CONTENT_X - CONTENT_R, h = 230;

    roundedFill({x + 7, y + 11, w, h}, 28, rgba(0,0,0,172));
    roundedFill({x, y, w, h}, 28, rgba(5,5,9,230));
    roundedOutline({x, y, w, h}, 28, rgba(61,61,75), 1);
    roundedFill({x + 1, y + 1, w - 2, 2}, 1, rgba(255,255,255,18));

    if (!game) {
        roundedFill({x, y, 7, h}, 4, accent);
        roundedFill({x + 24, y + 28, 118, 24}, 11, rgba(accent.r,accent.g,accent.b,27));
        text(fonts_.tiny, "WELCOME TO ADVANCE", x + 38, y + 32, accent);
        text(fonts_.hero, "Your GBA universe is ready.", x + 32, y + 66, rgba(255,255,255), 760);
        text(fonts_.body, "Add games to your ROM folder and Advance will build a polished library with artwork, history, collections, and personalized shelves.",
             x + 32, y + 128, rgba(165,165,178), 860);
        roundedFill({x + 32, y + 184, 190, 32}, 14, rgba(accent.r,accent.g,accent.b,28));
        text(fonts_.tiny, "SD:/mGBA/Roms", x + 52, y + 192, rgba(213,213,224));
        return;
    }

    markFocus(game);
    int iw = 0, ih = 0;
    SDL_Color coverAvg = accent;
    SDL_Texture* cover = loadCover(*game, iw, ih, &coverAvg);
    const SDL_Color adaptive = config.adaptiveAccent && cover ? mixColor(accent, coverAvg, 0.40f) : accent;

    // Full-bleed artwork atmosphere on the right side of the hero.
    int hw = 0, hh = 0;
    SDL_Color heroAvg = coverAvg;
    SDL_Texture* heroArt = !game->bannerPath.empty()
        ? loadImage(game->bannerPath, hw, hh, &heroAvg)
        : cover;
    if (heroArt && hw > 0 && hh > 0) {
        SDL_Rect clip{x + w - 535, y + 2, 533, h - 4};
        const float scale = std::max(static_cast<float>(clip.w) / hw, static_cast<float>(clip.h) / hh);
        const int dw = std::max(1, static_cast<int>(hw * scale));
        const int dh = std::max(1, static_cast<int>(hh * scale));
        SDL_Rect dst{clip.x + (clip.w - dw) / 2, clip.y + (clip.h - dh) / 2, dw, dh};
        SDL_RenderSetClipRect(renderer_, &clip);
        SDL_SetTextureAlphaMod(heroArt, 82);
        SDL_RenderCopy(renderer_, heroArt, nullptr, &dst);
        SDL_SetTextureAlphaMod(heroArt, 255);
        SDL_RenderSetClipRect(renderer_, nullptr);
        // Fade the image under the information layers.
        for (int i = 0; i < 8; ++i) {
            fill({clip.x + i * 34, clip.y, 38, clip.h}, rgba(5,5,9, static_cast<Uint8>(185 - i * 18)));
        }
        fill({clip.x, clip.y, clip.w, clip.h}, rgba(adaptive.r,adaptive.g,adaptive.b,16));
    }

    roundedFill({x, y, 7, h}, 4, adaptive);
    roundedFill({x + 7, y, 260, 3}, 1, rgba(adaptive.r, adaptive.g, adaptive.b, 218));
    roundedFill({x + 18, y + 13, 164, h - 26}, 21, rgba(adaptive.r,adaptive.g,adaptive.b,17));
    SDL_Rect coverRect{x + 26, y + 17, 148, h - 34};
    drawCoverCard(*game, coverRect, false, config, 1.0f, true);

    const int tx = x + 198;
    const int statX = x + w - 276;
    const int textRightEdge = statX - 20;
    const std::string kicker = game->completed ? "✓ COMPLETED SPOTLIGHT" : (game->launches > 0 ? "CONTINUE PLAYING" : "SPOTLIGHT");
    roundedFill({tx, y + 15, std::min(192, 26 + static_cast<int>(kicker.size()) * 7), 24}, 11,
                rgba(adaptive.r,adaptive.g,adaptive.b,28));
    text(fonts_.tiny, kicker, tx + 12, y + 19,
         game->completed ? rgba(73,232,145) : adaptive, textRightEdge - tx - 12);

    TTF_Font* titleFont = game->title.size() > 46 ? fonts_.medium : (game->title.size() > 30 ? fonts_.large : fonts_.hero);
    text(titleFont, game->title, tx, y + 46, rgba(255,255,255), textRightEdge - tx);

    std::vector<std::string> chips;
    if (!game->version.empty()) chips.push_back("v" + game->version);
    if (!game->genres.empty()) chips.push_back(game->genres.front());
    if (!game->author.empty()) chips.push_back(game->author);
    if (game->releaseYear > 0) chips.push_back(std::to_string(game->releaseYear));
    if (chips.empty()) chips.push_back(game->baseGame.empty() ? "Game Boy Advance" : "Based on " + game->baseGame);
    int chipX = tx;
    const int chipY = y + 98;
    for (std::size_t i = 0; i < chips.size() && i < 3; ++i) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(fonts_.tiny, chips[i].c_str(), &tw, &th);
        const int cw = std::min(164, tw + 24);
        if (chipX + cw > textRightEdge) break;
        roundedFill({chipX, chipY, cw, 25}, 12, rgba(255,255,255,10));
        roundedOutline({chipX, chipY, cw, 25}, 12, rgba(adaptive.r,adaptive.g,adaptive.b,62), 1);
        text(fonts_.tiny, chips[i], chipX + 12, chipY + 4, rgba(216,216,226), cw - 20);
        chipX += cw + 8;
    }

    const std::string description = game->description.empty()
        ? "Ready when you are. Details lets you enrich this game with metadata, screenshots, collections, and completion state."
        : game->description;
    wrappedText(fonts_.tiny, description, {tx, y + 132, textRightEdge - tx, 42}, rgba(154,154,168), 3);

    const int actionY = y + 187;
    const char* actionLabel = game->launches > 0 ? "RESUME" : "PLAY";
    const int actionW = game->launches > 0 ? 132 : 112;
    roundedFill({tx + 2, actionY + 3, actionW, 34}, 15, rgba(0,0,0,120));
    roundedFill({tx, actionY, actionW, 34}, 15, adaptive);
    circle(tx + 19, actionY + 17, 10, rgba(255,255,255), true);
    text(fonts_.tiny, "A", tx + 19, actionY + 9, adaptive, 0, true);
    text(fonts_.tiny, actionLabel, tx + 40, actionY + 8, rgba(255,255,255));

    roundedFill({tx + actionW + 10, actionY, 128, 34}, 15, rgba(23,23,31,238));
    roundedOutline({tx + actionW + 10, actionY, 128, 34}, 15, rgba(81,81,95), 1);
    text(fonts_.tiny, "X  DETAILS", tx + actionW + 30, actionY + 8, rgba(234,234,241));

    const int surpriseX = tx + actionW + 148;
    roundedFill({surpriseX, actionY, 128, 34}, 15, rgba(23,23,31,224));
    roundedOutline({surpriseX, actionY, 128, 34}, 15, rgba(second.r,second.g,second.b,80), 1);
    text(fonts_.tiny, "R  SURPRISE", surpriseX + 22, actionY + 8, rgba(212,212,222));

    // Activity glass panel floats above the hero artwork.
    std::uint64_t totalPlay = 0;
    std::size_t favorites = 0, completed = 0;
    for (const auto& g : games) {
        totalPlay += g.playSeconds;
        if (g.favorite) ++favorites;
        if (g.completed) ++completed;
    }
    roundedFill({statX + 4, y + 22, 250, 168}, 22, rgba(0,0,0,120));
    roundedFill({statX, y + 18, 250, 168}, 22, rgba(8,8,13,220));
    roundedOutline({statX, y + 18, 250, 168}, 22, rgba(adaptive.r,adaptive.g,adaptive.b,95), 1);
    roundedFill({statX + 16, y + 31, 92, 21}, 10, rgba(adaptive.r,adaptive.g,adaptive.b,28));
    text(fonts_.tiny, "YOUR ACTIVITY", statX + 28, y + 34, rgba(190,190,201));
    text(fonts_.medium,
         game->playSeconds > 0 ? formatPlayTime(game->playSeconds) : std::to_string(game->launches) + (game->launches == 1 ? " launch" : " launches"),
         statX + 18, y + 60, rgba(255,255,255), 214);
    text(fonts_.tiny, game->playSeconds > 0 ? "THIS GAME" : "THIS GAME · TIME TRACKS ON RETURN", statX + 18, y + 96, rgba(124,124,139), 214);
    line(statX + 18, y + 118, statX + 232, y + 118, rgba(69,69,82));
    roundedFill({statX + 18, y + 128, 104, 24}, 11, rgba(255,219,79,18));
    roundedOutline({statX + 18, y + 128, 104, 24}, 11, rgba(255,219,79,55), 1);
    text(fonts_.tiny, "★ " + std::to_string(favorites) + " FAVORITES", statX + 70, y + 131, rgba(255,224,103), 94, true);
    roundedFill({statX + 130, y + 128, 102, 24}, 11, rgba(82,230,147,16));
    roundedOutline({statX + 130, y + 128, 102, 24}, 11, rgba(82,230,147,50), 1);
    text(fonts_.tiny, "✓ " + std::to_string(completed) + " DONE", statX + 181, y + 131, rgba(99,236,162), 92, true);
    text(fonts_.tiny, "TOTAL  " + formatPlayTime(totalPlay), statX + 18, y + 158, rgba(151,151,165), 214);

    if (config.motionEnabled) {
        const std::uint64_t elapsed = SDL_GetTicks64() - selectionChangedAt_;
        if (elapsed < 150) {
            const float ft = clamp01(static_cast<float>(elapsed) / 150.0f);
            fill({x, y, w, h}, rgba(0,0,0,static_cast<Uint8>(70 * (1.0f - easeOutCubic(ft)))));
        }
    }
}

void Renderer::drawHomeShelf(const std::vector<Game>& games, const HomeRow& row, std::size_t rowIndex,
                             std::size_t selectedRow, std::size_t selectedColumn, int y, const Config& config) {
    const SDL_Color accent = accentColor(config);
    const bool active = rowIndex == selectedRow;
    if (active) {
        roundedFill({CONTENT_X, y - 1, 6, 26}, 3, accent);
        roundedFill({CONTENT_X + 11, y - 2, 230, 28}, 13, rgba(accent.r,accent.g,accent.b,18));
    }
    text(fonts_.small, row.title, CONTENT_X + (active ? 20 : 2), y, active ? rgba(255,255,255) : rgba(196,196,206), 286);
    text(fonts_.tiny, row.subtitle, CONTENT_X + 310, y + 3, active ? rgba(153,153,168) : rgba(114,114,128), 520);

    constexpr int cardW = 148, cardH = 144, gap = 14, visibleCards = 7;
    std::size_t start = 0;
    if (active && selectedColumn >= visibleCards) start = selectedColumn - visibleCards + 1;
    if (row.games.size() > visibleCards && start + visibleCards > row.games.size()) start = row.games.size() - visibleCards;

    if (active && selectedColumn < row.games.size()) {
        const std::size_t gi = row.games[selectedColumn];
        if (gi < games.size()) {
            const std::string focusTitle = games[gi].shortTitle.empty() ? games[gi].title : games[gi].shortTitle;
            int tw = 0, th = 0;
            TTF_SizeUTF8(fonts_.tiny, focusTitle.c_str(), &tw, &th);
            const int chipW = std::min(265, tw + 28);
            const int chipX = W - CONTENT_R - chipW - (row.games.size() > visibleCards ? 72 : 0);
            roundedFill({chipX, y - 1, chipW, 25}, 12, rgba(255,255,255,10));
            roundedOutline({chipX, y - 1, chipW, 25}, 12, rgba(accent.r,accent.g,accent.b,55), 1);
            text(fonts_.tiny, focusTitle, chipX + 14, y + 3, rgba(220,220,229), chipW - 24);
        }
    }

    const int cardY = y + 34;
    for (int slot = 0; slot < visibleCards; ++slot) {
        const std::size_t c = start + static_cast<std::size_t>(slot);
        if (c >= row.games.size()) break;
        const std::size_t gi = row.games[c];
        if (gi >= games.size()) continue;
        const bool selected = active && c == selectedColumn;
        if (selected) markFocus(&games[gi]);
        const float ft = selected ? (SDL_GetTicks64() - selectionChangedAt_) / static_cast<float>(FOCUS_ANIM_MS) : 1.0f;
        drawCoverCard(games[gi], {CONTENT_X + slot * (cardW + gap), cardY, cardW, cardH}, selected, config, ft, true);
    }
    if (active && start > 0) {
        roundedFill({CONTENT_X - 5, cardY + cardH / 2 - 16, 28, 32}, 14, rgba(4,4,8,230));
        line(CONTENT_X + 10, cardY + cardH / 2 - 6, CONTENT_X + 4, cardY + cardH / 2, rgba(220,220,230));
        line(CONTENT_X + 4, cardY + cardH / 2, CONTENT_X + 10, cardY + cardH / 2 + 6, rgba(220,220,230));
    }
    if (active && start + visibleCards < row.games.size()) {
        const int rx = CONTENT_X + visibleCards * (cardW + gap) - gap - 20;
        roundedFill({rx, cardY + cardH / 2 - 16, 28, 32}, 14, rgba(4,4,8,230));
        line(rx + 10, cardY + cardH / 2 - 6, rx + 16, cardY + cardH / 2, rgba(220,220,230));
        line(rx + 16, cardY + cardH / 2, rx + 10, cardY + cardH / 2 + 6, rgba(220,220,230));
    }
    if (row.games.size() > visibleCards) {
        std::ostringstream count; count << (active ? selectedColumn + 1 : 1) << " / " << row.games.size();
        roundedFill({W - CONTENT_R - 64, y - 1, 64, 25}, 12, rgba(255,255,255,8));
        text(fonts_.tiny, count.str(), W - CONTENT_R - 32, y + 3, rgba(129,129,143), 0, true);
    }
}

void Renderer::renderHome(const std::vector<Game>& games,
                          const std::vector<HomeRow>& rows,
                          std::size_t selectedRow,
                          std::size_t selectedColumn,
                          const Config& config,
                          const std::string& toast) {
    const Game* selected = nullptr;
    if (!rows.empty() && selectedRow < rows.size() && !rows[selectedRow].games.empty()) {
        selectedColumn = std::min(selectedColumn, rows[selectedRow].games.size() - 1);
        const std::size_t idx = rows[selectedRow].games[selectedColumn];
        if (idx < games.size()) selected = &games[idx];
    }
    drawDynamicBackdrop(selected, config, 84);
    drawSidebar(Screen::Home, Shelf::All, config);
    std::string greeting = "Welcome back";
    if (const std::time_t now = std::time(nullptr); now > 0) {
        if (std::tm* tm = std::localtime(&now)) {
            if (tm->tm_hour >= 5 && tm->tm_hour < 12) greeting = "Good morning";
            else if (tm->tm_hour >= 12 && tm->tm_hour < 18) greeting = "Good afternoon";
            else if (tm->tm_hour >= 18 && tm->tm_hour < 23) greeting = "Good evening";
        }
    }
    std::size_t favoriteCount = 0;
    for (const auto& g : games) if (g.favorite) ++favoriteCount;
    const std::string homeSubtitle = std::to_string(games.size()) + (games.size() == 1 ? " game" : " games") +
        "  ·  " + std::to_string(favoriteCount) + (favoriteCount == 1 ? " favorite" : " favorites");
    drawTopChrome("ADVANCE  /  HOME", greeting + ", " + nickname_, homeSubtitle, config);
    drawHero(selected, games, config);

    if (!rows.empty()) {
        const std::size_t r0 = std::min(selectedRow, rows.size() - 1);
        drawHomeShelf(games, rows[r0], r0, selectedRow, selectedColumn, HEADER_H + 242, config);
        std::size_t r1 = r0 + 1 < rows.size() ? r0 + 1 : (r0 > 0 ? r0 - 1 : r0);
        if (r1 != r0) drawHomeShelf(games, rows[r1], r1, selectedRow, 0, HEADER_H + 424, config);
    }
    drawFooter({{"A","Play"},{"X","Details"},{"Y","Favorite"},{"R","Surprise Me"},{"L","Library"},{"−","Search"},{"+","Settings"}});
    drawScreenTransition(Screen::Home, config);
    drawSidebarOverlay(Screen::Home, Shelf::All, config);
    drawToast(toast);
}

std::string Renderer::shelfName(Shelf shelf, const std::string& collectionName) const {
    switch (shelf) {
        case Shelf::Favorites: return "Favorites";
        case Shelf::Recent: return "Recently Played";
        case Shelf::Search: return collectionName.empty() ? "Search Results" : collectionName;
        case Shelf::Collection: return collectionName.empty() ? "Collection" : collectionName;
        case Shelf::Completed: return "Completed";
        case Shelf::Hidden: return "Hidden";
        default: return "Game Boy Advance";
    }
}

std::string Renderer::sortName(SortMode mode) const {
    switch (mode) {
        case SortMode::Recent: return "Recent";
        case SortMode::Launches: return "Most launches";
        case SortMode::PlayTime: return "Play time";
        case SortMode::Added: return "Recently added";
        case SortMode::Favorites: return "Favorites first";
        default: return "A–Z";
    }
}

void Renderer::drawSelectedStrip(const Game* game, std::size_t selected, std::size_t count,
                                 std::size_t perPage, Shelf shelf, const Config& config) {
    const GalleryLayout l = galleryLayout(config);
    const SDL_Color accent = accentColor(config);
    const SDL_Color second = secondaryColor(config);
    SDL_Color adaptive = accent;
    int coverW = 0, coverH = 0;
    SDL_Texture* cover = nullptr;
    if (game) {
        SDL_Color avg = accent;
        cover = loadCover(*game, coverW, coverH, &avg);
        adaptive = config.adaptiveAccent ? mixColor(accent, avg, 0.38f) : accent;
    }

    const int y = H - FOOTER_H - 82;
    roundedFill({l.x + 5, y + 6, l.w, 72}, 19, rgba(0,0,0,160));
    roundedFill({l.x, y, l.w, 72}, 19, rgba(6,6,10,245));
    roundedOutline({l.x, y, l.w, 72}, 19, rgba(56,56,68), 1);
    roundedFill({l.x, y, 6, 72}, 3, adaptive);

    if (!game) {
        const std::string empty = shelf == Shelf::Favorites ? "Nothing favorited yet" :
                                  shelf == Shelf::Search ? "No games match that search" : "No games found";
        text(fonts_.body, empty, l.x + 22, y + 14, rgba(255,255,255));
        text(fonts_.tiny, "Try another shelf, search, or rebuild the library in Settings.", l.x + 22, y + 46, rgba(139,139,151), 720);
        return;
    }

    SDL_Rect thumb{l.x + 16, y + 7, 46, 58};
    roundedFill({thumb.x + 3, thumb.y + 4, thumb.w, thumb.h}, 9, rgba(0,0,0,145));
    roundedFill(thumb, 9, rgba(9,9,14));
    if (cover && coverW > 0 && coverH > 0) {
        const float scale = std::min(static_cast<float>(thumb.w - 4) / coverW, static_cast<float>(thumb.h - 4) / coverH);
        SDL_Rect dst{thumb.x + 2, thumb.y + 2, std::max(1, static_cast<int>(coverW * scale)), std::max(1, static_cast<int>(coverH * scale))};
        dst.x += (thumb.w - 4 - dst.w) / 2; dst.y += (thumb.h - 4 - dst.h) / 2;
        SDL_RenderCopy(renderer_, cover, nullptr, &dst);
    } else {
        drawBrandHandheld(thumb.x + 5, thumb.y + 15, thumb.w - 10, 26, adaptive, 210);
    }
    roundedOutline(thumb, 9, rgba(adaptive.r,adaptive.g,adaptive.b,110), 1);

    const int infoX = l.x + 78;
    roundedFill({infoX, y + 7, 102, 18}, 8, rgba(adaptive.r,adaptive.g,adaptive.b,24));
    text(fonts_.tiny, "SELECTED", infoX + 12, y + 8, adaptive);
    TTF_Font* titleFont = game->title.size() > 42 ? fonts_.body : fonts_.medium;
    text(titleFont, game->title, infoX, y + 26, rgba(255,255,255), 560);

    int chipX = infoX + 420;
    const int chipY = y + 40;
    auto drawChip = [&](const std::string& label, SDL_Color color, int maxW) {
        int tw=0, th=0; TTF_SizeUTF8(fonts_.tiny, label.c_str(), &tw, &th);
        const int cw = std::min(maxW, tw + 20);
        roundedFill({chipX, chipY, cw, 22}, 10, rgba(color.r,color.g,color.b,22));
        roundedOutline({chipX, chipY, cw, 22}, 10, rgba(color.r,color.g,color.b,70), 1);
        text(fonts_.tiny, label, chipX + 10, chipY + 2, rgba(196,196,207), cw - 16);
        chipX += cw + 7;
    };
    if (game->favorite) drawChip("★ FAVORITE", rgba(255,218,78), 110);
    if (game->completed) drawChip("✓ COMPLETED", rgba(75,232,145), 122);
    if (chipX < l.x + l.w - 300) drawChip(formatPlayTime(game->playSeconds), adaptive, 112);

    const int playX = l.x + l.w - 144;
    roundedFill({playX + 3, y + 15, 124, 44}, 18, rgba(0,0,0,125));
    roundedFill({playX, y + 12, 124, 44}, 18, adaptive);
    circle(playX + 22, y + 34, 11, rgba(255,255,255), true);
    text(fonts_.tiny, "A", playX + 22, y + 26, adaptive, 0, true);
    text(fonts_.small, game->launches > 0 ? "RESUME" : "PLAY", playX + 45, y + 22, rgba(255,255,255));

    if (count > 0 && perPage > 0) {
        const std::size_t pages = (count + perPage - 1) / perPage;
        const std::size_t page = selected / perPage;
        const int pageRight = playX - 14;
        roundedFill({pageRight - 96, y + 9, 96, 25}, 11, rgba(255,255,255,8));
        text(fonts_.tiny, "PAGE " + std::to_string(page + 1) + " / " + std::to_string(pages), pageRight - 48, y + 13, rgba(162,162,176), 0, true);
        const int dots = static_cast<int>(std::min<std::size_t>(pages, 7));
        const int startX = pageRight - dots * 12;
        for (int i = 0; i < dots; ++i) {
            const std::size_t represented = pages <= 7 ? static_cast<std::size_t>(i)
                : std::min(pages - 1, page > 5 ? page - 5 + i : static_cast<std::size_t>(i));
            circle(startX + i * 12, y + 52, represented == page ? 4 : 3,
                   represented == page ? adaptive : rgba(68,68,80), true);
        }
    }
}

void Renderer::renderLibrary(const std::vector<Game>& games,
                             const std::vector<std::size_t>& visible,
                             std::size_t selected,
                             Shelf shelf,
                             SortMode sortMode,
                             const std::string& collectionName,
                             const Config& config,
                             const std::string& toast) {
    const Game* selectedGame = nullptr;
    if (!visible.empty() && selected < visible.size() && visible[selected] < games.size()) selectedGame = &games[visible[selected]];
    markFocus(selectedGame);
    drawDynamicBackdrop(selectedGame, config, 48);
    drawSidebar(Screen::Library, shelf, config);
    const std::string title = shelfName(shelf, collectionName);
    std::ostringstream subtitle;
    subtitle << visible.size() << (visible.size() == 1 ? " game" : " games") << "  ·  " << sortName(sortMode);
    drawTopChrome("ADVANCE  /  LIBRARY", title, subtitle.str(), config);

    const GalleryLayout l = galleryLayout(config);
    const std::size_t page = l.perPage ? selected / l.perPage : 0;
    const std::size_t start = page * l.perPage;
    for (std::size_t slot = 0; slot < l.perPage; ++slot) {
        const std::size_t vi = start + slot;
        if (vi >= visible.size()) break;
        const std::size_t gi = visible[vi];
        if (gi >= games.size()) continue;
        const int row = static_cast<int>(slot / l.cols);
        const int col = static_cast<int>(slot % l.cols);
        SDL_Rect card{l.x + col * (l.cardW + l.gap), l.y + row * (l.cardH + l.gap), l.cardW, l.cardH};
        const bool isSelected = vi == selected;
        const float progress = isSelected ? (SDL_GetTicks64() - selectionChangedAt_) / static_cast<float>(FOCUS_ANIM_MS) : 1.0f;
        drawCoverCard(games[gi], card, isSelected, config, progress);
    }

    drawSelectedStrip(selectedGame, selected, visible.size(), l.perPage, shelf, config);
    drawFooter({{"−","Search"},{"Y","Sort"},{"X","Details"},{"A","Play"},{"L","Favorites"},{"R","Recent"},{"ZL/ZR","Page"},{"B","Home"}});
    drawScreenTransition(Screen::Library, config);
    drawSidebarOverlay(Screen::Library, shelf, config);
    drawToast(toast);
}

void Renderer::drawCollectionCard(const std::vector<Game>& games, const Collection& collection, SDL_Rect rect,
                                  bool selected, const Config& config) {
    const SDL_Color base = accentColor(config);
    SDL_Color adaptive = base;
    if (!collection.games.empty() && collection.games.front() < games.size()) {
        int iw=0, ih=0; SDL_Color avg=base;
        if (loadCover(games[collection.games.front()], iw, ih, &avg) && config.adaptiveAccent)
            adaptive = mixColor(base, avg, 0.34f);
    }

    const int grow = selected ? 5 : 0;
    rect = {rect.x - grow, rect.y - grow, rect.w + grow * 2, rect.h + grow * 2};
    if (selected) {
        const float pulse = config.motionEnabled ? 0.5f + 0.5f * std::sin(SDL_GetTicks64() / 230.0f) : 0.5f;
        roundedFill({rect.x - 12, rect.y - 12, rect.w + 24, rect.h + 24}, 28,
                    rgba(adaptive.r,adaptive.g,adaptive.b,static_cast<Uint8>(14 + pulse * 10)));
        roundedFill({rect.x - 7, rect.y - 7, rect.w + 14, rect.h + 14}, 24,
                    rgba(adaptive.r,adaptive.g,adaptive.b,static_cast<Uint8>(26 + pulse * 12)));
    }
    roundedFill({rect.x + 5, rect.y + 8, rect.w, rect.h}, 20, rgba(0,0,0,180));
    roundedFill(rect, 20, rgba(8,8,13,246));
    roundedOutline(rect, 20, selected ? rgba(adaptive.r,adaptive.g,adaptive.b,225) : rgba(53,53,65), selected ? 2 : 1);
    roundedFill({rect.x + 2, rect.y + 2, rect.w - 4, 2}, 1, rgba(255,255,255,17));

    const int collageH = 126;
    SDL_Rect clip{rect.x + 4, rect.y + 4, rect.w - 8, collageH};
    roundedFill(clip, 17, rgba(14,14,20));
    const int cellW = clip.w / 4;
    if (collection.games.empty()) {
        for (int i = 0; i < 5; ++i) {
            const int inset = 18 + i * 12;
            roundedOutline({clip.x + inset, clip.y + 16 + inset / 3, clip.w - inset * 2, clip.h - 32 - inset / 2}, 18,
                           rgba(adaptive.r,adaptive.g,adaptive.b,static_cast<Uint8>(34 - i * 5)), 1);
        }
        drawBrandHandheld(clip.x + clip.w / 2 - 42, clip.y + 29, 84, 58, adaptive, 150);
        text(fonts_.tiny, "EMPTY SHELF", clip.x + clip.w / 2, clip.y + 94, rgba(116,116,130), 0, true);
    } else {
        for (int i = 0; i < 4; ++i) {
            if (static_cast<std::size_t>(i) >= collection.games.size()) break;
            const std::size_t gi = collection.games[static_cast<std::size_t>(i)];
            if (gi >= games.size()) continue;
            int iw = 0, ih = 0;
            SDL_Texture* tex = loadCover(games[gi], iw, ih, nullptr);
            if (!tex || iw <= 0 || ih <= 0) continue;
            SDL_Rect cell{clip.x + i * cellW, clip.y, cellW, clip.h};
            const float scale = std::max(static_cast<float>(cell.w) / iw, static_cast<float>(cell.h) / ih);
            const int dw = static_cast<int>(iw * scale), dh = static_cast<int>(ih * scale);
            SDL_Rect dst{cell.x + (cell.w - dw) / 2, cell.y + (cell.h - dh) / 2, dw, dh};
            SDL_RenderSetClipRect(renderer_, &cell);
            SDL_RenderCopy(renderer_, tex, nullptr, &dst);
            SDL_RenderSetClipRect(renderer_, nullptr);
        }
    }
    // Image-to-card fade.
    for (int i = 0; i < 4; ++i) fill({clip.x, clip.y + clip.h - 38 + i * 9, clip.w, 10}, rgba(0,0,0,static_cast<Uint8>(80 + i * 38)));

    roundedFill({rect.x + 16, rect.y + 14, collection.automatic ? 58 : 70, 21}, 10,
                rgba(adaptive.r,adaptive.g,adaptive.b,215));
    text(fonts_.tiny, collection.automatic ? "SMART" : "CUSTOM", rect.x + (collection.automatic ? 45 : 51), rect.y + 16,
         rgba(255,255,255), 0, true);

    text(fonts_.body, collection.name, rect.x + 18, rect.y + 142, rgba(255,255,255), rect.w - 36);
    text(fonts_.tiny, collection.description, rect.x + 18, rect.y + 177, rgba(143,143,156), rect.w - 36);
    roundedFill({rect.x + 18, rect.y + rect.h - 35, 92, 22}, 10, rgba(255,255,255,8));
    text(fonts_.tiny, std::to_string(collection.games.size()) + (collection.games.size() == 1 ? " GAME" : " GAMES"),
         rect.x + 64, rect.y + rect.h - 33, selected ? adaptive : rgba(177,177,188), 0, true);
    if (selected) {
        roundedFill({rect.x + rect.w - 90, rect.y + rect.h - 36, 72, 24}, 11, rgba(adaptive.r,adaptive.g,adaptive.b,25));
        text(fonts_.tiny, "A  OPEN", rect.x + rect.w - 54, rect.y + rect.h - 33, rgba(232,232,240), 0, true);
    }
}

void Renderer::renderCollections(const std::vector<Game>& games,
                                 const std::vector<Collection>& collections,
                                 std::size_t selected,
                                 const Config& config,
                                 const std::string& toast) {
    const Game* moodGame = nullptr;
    if (!collections.empty() && selected < collections.size() && !collections[selected].games.empty()) {
        const std::size_t gi = collections[selected].games.front();
        if (gi < games.size()) moodGame = &games[gi];
    }
    drawDynamicBackdrop(moodGame, config, 40);
    drawSidebar(Screen::Collections, Shelf::All, config);
    drawTopChrome("ADVANCE  /  COLLECTIONS", "Your shelves", "Automatic categories + your own collections", config);
    const SDL_Color accent = accentColor(config);

    if (collections.empty()) {
        roundedFill({CONTENT_X, 130, W - CONTENT_X - CONTENT_R, 380}, 28, rgba(7,7,11,235));
        text(fonts_.hero, "Build a collection that feels like yours.", CONTENT_X + 40, 190, rgba(255,255,255), 850);
        wrappedText(fonts_.body, "Press X to create a custom collection. Advance also creates smart shelves automatically as it learns your library.",
                    {CONTENT_X + 40, 260, 760, 100}, rgba(160,160,174), 6);
        roundedFill({CONTENT_X + 40, 390, 180, 44}, 17, accent);
        text(fonts_.small, "X  NEW COLLECTION", CONTENT_X + 61, 402, rgba(255,255,255));
    } else {
        constexpr int cols = 3;
        const int gap = 16;
        const int x = CONTENT_X;
        const int y = HEADER_H + 17;
        const int totalW = W - CONTENT_X - CONTENT_R;
        const int cardW = (totalW - gap * 2) / cols;
        const int cardH = 248;
        const std::size_t pageSize = 6;
        const std::size_t page = selected / pageSize;
        const std::size_t start = page * pageSize;
        for (std::size_t slot = 0; slot < pageSize; ++slot) {
            const std::size_t ci = start + slot;
            if (ci >= collections.size()) break;
            const int row = static_cast<int>(slot / cols), col = static_cast<int>(slot % cols);
            drawCollectionCard(games, collections[ci], {x + col * (cardW + gap), y + row * (cardH + gap), cardW, cardH},
                               ci == selected, config);
        }
        const std::size_t pages = (collections.size() + pageSize - 1) / pageSize;
        if (pages > 1) textRight(fonts_.tiny, "PAGE " + std::to_string(page + 1) + " / " + std::to_string(pages), W - CONTENT_R, H - FOOTER_H - 24, rgba(138,138,151));
    }
    drawFooter({{"A","Open"},{"X","New collection"},{"R","Surprise Me"},{"B","Home"},{"+","Settings"}});
    drawScreenTransition(Screen::Collections, config);
    drawSidebarOverlay(Screen::Collections, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderDetails(const Game& game, const Config& config, const std::string& toast) {
    drawDynamicBackdrop(&game, config, 74);
    drawSidebar(Screen::Details, Shelf::All, config);
    drawTopChrome("ADVANCE  /  DETAILS", game.title, game.titleSource, config, true);
    const SDL_Color baseAccent = accentColor(config);
    int accentW = 0, accentH = 0;
    SDL_Color coverAverage = baseAccent;
    loadCover(game, accentW, accentH, &coverAverage);
    const SDL_Color accent = config.adaptiveAccent ? mixColor(baseAccent, coverAverage, 0.36f) : baseAccent;

    const int coverX = CONTENT_X, coverY = HEADER_H + 20, coverW = 254, coverH = 354;
    drawCoverCard(game, {coverX, coverY, coverW, coverH}, false, config, 1.0f);

    const int x = coverX + coverW + 30;
    const int right = W - CONTENT_R;

    // Details hero art sits behind the title area, giving each game its own identity.
    int heroW = 0, heroH = 0;
    SDL_Texture* heroArt = !game.bannerPath.empty()
        ? loadImage(game.bannerPath, heroW, heroH, nullptr)
        : loadCover(game, heroW, heroH, nullptr);
    if (heroArt && heroW > 0 && heroH > 0) {
        SDL_Rect heroClip{x - 12, coverY - 8, right - x + 12, 118};
        roundedFill(heroClip, 22, rgba(6,6,10,220));
        const float scale = std::max(static_cast<float>(heroClip.w) / heroW, static_cast<float>(heroClip.h) / heroH);
        const int dw = std::max(1, static_cast<int>(heroW * scale));
        const int dh = std::max(1, static_cast<int>(heroH * scale));
        SDL_Rect dst{heroClip.x + (heroClip.w - dw) / 2, heroClip.y + (heroClip.h - dh) / 2, dw, dh};
        SDL_RenderSetClipRect(renderer_, &heroClip);
        SDL_SetTextureAlphaMod(heroArt, 66);
        SDL_RenderCopy(renderer_, heroArt, nullptr, &dst);
        SDL_SetTextureAlphaMod(heroArt, 255);
        SDL_RenderSetClipRect(renderer_, nullptr);
        fill(heroClip, rgba(0,0,0,116));
        for (int i = 0; i < 5; ++i)
            fill({heroClip.x + i * 40, heroClip.y, 44, heroClip.h}, rgba(0,0,0,static_cast<Uint8>(155 - i * 24)));
        roundedOutline(heroClip, 22, rgba(accent.r,accent.g,accent.b,58), 1);
    }

    text(fonts_.tiny, game.completed ? "✓ COMPLETED" : (game.favorite ? "★ FAVORITE" : "GAME PROFILE"), x, coverY,
         game.completed ? rgba(72,232,143) : (game.favorite ? rgba(255,218,75) : accent), 300);
    TTF_Font* titleFont = game.title.size() > 42 ? fonts_.large : fonts_.hero;
    text(titleFont, game.title, x, coverY + 24, rgba(255,255,255), right - x - 15);

    std::vector<std::string> meta;
    if (!game.version.empty()) meta.push_back("v" + game.version);
    if (!game.genres.empty()) meta.push_back(game.genres.front());
    if (!game.author.empty()) meta.push_back(game.author);
    if (game.releaseYear) meta.push_back(std::to_string(game.releaseYear));
    if (meta.empty()) meta.push_back(game.baseGame.empty() ? "Game Boy Advance" : "Based on " + game.baseGame);
    int metaX = x;
    for (std::size_t i = 0; i < meta.size() && i < 4; ++i) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(fonts_.tiny, meta[i].c_str(), &tw, &th);
        const int cw = std::min(180, tw + 24);
        if (metaX + cw > right) break;
        roundedFill({metaX, coverY + 84, cw, 26}, 12, rgba(accent.r,accent.g,accent.b,26));
        roundedOutline({metaX, coverY + 84, cw, 26}, 12, rgba(accent.r,accent.g,accent.b,78), 1);
        text(fonts_.tiny, meta[i], metaX + 12, coverY + 88, rgba(211,211,221), cw - 20);
        metaX += cw + 8;
    }

    roundedFill({x, coverY + 119, right - x, 126}, 19, rgba(6,6,10,224));
    roundedOutline({x, coverY + 119, right - x, 126}, 19, rgba(53,53,65), 1);
    text(fonts_.tiny, "ABOUT THIS GAME", x + 18, coverY + 135, accent);
    const std::string description = game.description.empty()
        ? "This game is ready to launch. You can optionally add metadata and screenshots later to turn this page into a richer showcase."
        : game.description;
    wrappedText(fonts_.small, description, {x + 18, coverY + 161, right - x - 36, 71}, rgba(211,211,220), 4);

    const int statsY = coverY + 260;
    const int statW = (right - x - 20) / 3;
    const std::array<std::pair<std::string,std::string>,3> stats = {{
        {formatPlayTime(game.playSeconds), "PLAY TIME"},
        {std::to_string(game.launches), "LAUNCHES"},
        {game.lastPlayed > 0 ? formatTimestamp(game.lastPlayed) : "Never", "LAST PLAYED"}
    }};
    for (int i = 0; i < 3; ++i) {
        const int sx = x + i * (statW + 10);
        roundedFill({sx, statsY, statW, 88}, 17, rgba(9,9,14,228));
        roundedOutline({sx, statsY, statW, 88}, 17, rgba(48,48,60), 1);
        text(fonts_.body, stats[i].first, sx + 15, statsY + 17, rgba(255,255,255), statW - 30);
        text(fonts_.tiny, stats[i].second, sx + 15, statsY + 55, rgba(128,128,141), statW - 30);
    }

    const int screenshotsY = coverY + coverH + 18;
    text(fonts_.tiny, "MEDIA", coverX, screenshotsY, accent);
    int sx = coverX;
    int shown = 0;
    for (const auto& path : game.screenshotPaths) {
        if (shown >= 4) break;
        int iw = 0, ih = 0;
        SDL_Texture* image = loadImage(path, iw, ih, nullptr);
        if (!image || iw <= 0 || ih <= 0) continue;
        SDL_Rect box{sx, screenshotsY + 25, 200, 102};
        roundedFill(box, 12, rgba(7,7,11));
        const float scale = std::max(static_cast<float>(box.w) / iw, static_cast<float>(box.h) / ih);
        const int dw = static_cast<int>(iw * scale), dh = static_cast<int>(ih * scale);
        SDL_Rect dst{box.x + (box.w - dw) / 2, box.y + (box.h - dh) / 2, dw, dh};
        SDL_RenderSetClipRect(renderer_, &box);
        SDL_RenderCopy(renderer_, image, nullptr, &dst);
        SDL_RenderSetClipRect(renderer_, nullptr);
        roundedOutline(box, 12, rgba(62,62,73), 1);
        sx += 214; ++shown;
    }
    if (shown == 0) {
        roundedFill({coverX, screenshotsY + 25, 414, 102}, 14, rgba(7,7,11,220));
        roundedOutline({coverX, screenshotsY + 25, 414, 102}, 14, rgba(47,47,58), 1);
        text(fonts_.small, "Add screenshots to make this page yours", coverX + 18, screenshotsY + 47, rgba(205,205,215), 380);
        text(fonts_.tiny, "screenshot1.png · screenshot2.png · or list them in advance.json", coverX + 18, screenshotsY + 74, rgba(124,124,138), 380);
    }

    // Quick actions feel like an action palette rather than plain text status.
    const int actionX = right - 300;
    roundedFill({actionX, screenshotsY + 25, 300, 102}, 16, rgba(accent.r,accent.g,accent.b,23));
    roundedOutline({actionX, screenshotsY + 25, 300, 102}, 16, rgba(accent.r,accent.g,accent.b,90), 1);
    const std::array<std::tuple<const char*, std::string, SDL_Color>,3> actions = {{
        {"Y", game.favorite ? "Favorited" : "Favorite", game.favorite ? rgba(255,220,82) : rgba(235,235,242)},
        {"ZR", game.completed ? "Completed" : "Mark complete", game.completed ? rgba(73,232,142) : rgba(190,190,201)},
        {"ZL", game.hidden ? "Hidden" : "Hide game", game.hidden ? rgba(255,137,90) : rgba(160,160,174)}
    }};
    for (int i = 0; i < 3; ++i) {
        const int yy = screenshotsY + 34 + i * 27;
        roundedFill({actionX + 16, yy, i == 0 ? 24 : 30, 21}, 10, rgba(238,238,244));
        text(fonts_.tiny, std::get<0>(actions[i]), actionX + (i == 0 ? 28 : 31), yy + 1, rgba(9,9,12), 0, true);
        text(fonts_.tiny, std::get<1>(actions[i]), actionX + 56, yy + 2, std::get<2>(actions[i]), 210);
    }

    drawFooter({{"A","Play"},{"Y","Favorite"},{"X","Edit title"},{"R","Collections"},{"ZR","Complete"},{"ZL","Hide"},{"−","Auto title"},{"B","Back"}});
    drawScreenTransition(Screen::Details, config);
    drawSidebarOverlay(Screen::Details, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderCollectionPicker(const Game& game,
                                      const std::vector<CustomCollection>& collections,
                                      std::size_t selected,
                                      const Config& config,
                                      const std::string& toast) {
    drawDynamicBackdrop(&game, config, 66);
    drawSidebar(Screen::Details, Shelf::All, config);
    drawTopChrome("ADVANCE  /  COLLECTIONS", "Add to collection", game.title, config, true);
    const SDL_Color accent = accentColor(config);

    const int panelX = 260, panelY = 118, panelW = 760, panelH = 500;
    roundedFill({panelX, panelY, panelW, panelH}, 28, rgba(5,5,9,244));
    roundedOutline({panelX, panelY, panelW, panelH}, 28, rgba(63,63,76), 1);
    text(fonts_.tiny, "PERSONAL SHELVES", panelX + 28, panelY + 24, accent);
    text(fonts_.large, "Where should this game live?", panelX + 28, panelY + 47, rgba(255,255,255), panelW - 56);
    text(fonts_.small, "A toggles membership. X creates another collection.", panelX + 28, panelY + 95, rgba(145,145,158));

    if (collections.empty()) {
        roundedFill({panelX + 28, panelY + 150, panelW - 56, 180}, 20, rgba(11,11,16));
        text(fonts_.medium, "No custom collections yet", panelX + 52, panelY + 189, rgba(255,255,255));
        wrappedText(fonts_.small, "Press X to create your first shelf. You can use collections for must-play games, favorite hacks, testing builds, or anything else.",
                    {panelX + 52, panelY + 235, panelW - 104, 75}, rgba(157,157,170), 5);
    } else {
        const std::size_t maxRows = 7;
        const std::size_t start = selected >= maxRows ? selected - maxRows + 1 : 0;
        for (std::size_t slot = 0; slot < maxRows; ++slot) {
            const std::size_t i = start + slot;
            if (i >= collections.size()) break;
            const bool on = i == selected;
            const bool member = collectionContains(collections[i], game.path);
            const int y = panelY + 145 + static_cast<int>(slot) * 47;
            roundedFill({panelX + 28, y, panelW - 56, 39}, 13, on ? rgba(accent.r,accent.g,accent.b,29) : rgba(12,12,17));
            if (on) roundedOutline({panelX + 28, y, panelW - 56, 39}, 13, accent, 1);
            circle(panelX + 52, y + 20, 10, member ? accent : rgba(62,62,74), true);
            if (member) text(fonts_.tiny, "✓", panelX + 52, y + 12, rgba(255,255,255), 0, true);
            text(fonts_.small, collections[i].name, panelX + 76, y + 8, on ? rgba(255,255,255) : rgba(211,211,220), panelW - 130);
        }
    }
    drawFooter({{"A","Toggle"},{"X","New collection"},{"B","Back"}});
    drawScreenTransition(Screen::CollectionPicker, config);
    drawSidebarOverlay(Screen::Details, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderSettings(const Config& config, std::size_t gameCount, std::size_t selectedSetting,
                              const std::string& toast) {
    drawBaseBackdrop(config);
    drawSidebar(Screen::Settings, Shelf::All, config);
    drawTopChrome("ADVANCE  /  SETTINGS", "Make Advance yours", std::to_string(gameCount) + " games indexed", config);
    const SDL_Color accent = accentColor(config);

    struct SettingRow { const char* name; std::string value; const char* description; };
    const std::array<SettingRow,17> rows = {{
        {"Theme", prettyThemeName(config.theme), "Seven signature palettes built for OLED."},
        {"Grid columns", std::to_string(config.columns), "Choose between bold covers and denser browsing."},
        {"Grid rows", std::to_string(config.rows), "Two rows for showcase, three for density."},
        {"Dynamic backdrop", config.dynamicBackdrop ? "On" : "Off", "Let the selected game's art color the whole interface."},
        {"Backdrop intensity", std::to_string(config.backdropIntensity) + "%", "Controls how strongly game art influences the background."},
        {"Adaptive game accents", config.adaptiveAccent ? "On" : "Off", "Blend each selected cover into buttons, chips, and highlight color."},
        {"Screen transitions", config.screenTransitions ? "On" : "Off", "A quick cinematic fade between major sections."},
        {"Launch transition", config.launchTransition ? "On" : "Off", "Show a polished handoff card before mGBA starts."},
        {"UI sounds", config.uiSounds ? "On" : "Off", "Tiny synthesized cues — no external assets required."},
        {"UI volume", std::to_string(config.uiVolume) + "%", "Volume for navigation, confirm, and launch sounds."},
        {"Cover labels", config.showCoverLabels ? "On" : "Off", "Overlay titles on box art when you want extra context."},
        {"Touch", config.touchEnabled ? "On" : "Off", "Tap games and navigation on the Switch touchscreen."},
        {"Motion", config.motionEnabled ? "On" : "Off", "Focus lift, shimmer, and transition polish."},
        {"System status", config.showSystemStatus ? "On" : "Off", "Clock, Wi-Fi, battery, and charging state."},
        {"Show hidden", config.showHidden ? "On" : "Off", "Include hidden games in normal library views."},
        {"Confirm launch", config.confirmLaunch ? "On" : "Off", "Require a second A press before handing off to mGBA."},
        {"Rebuild library", "Run", "Rescan ROMs, artwork, advance.json metadata, and smart collections."}
    }};

    const int listX = CONTENT_X, listY = HEADER_H + 17, listW = 700, listH = 548;
    roundedFill({listX, listY, listW, listH}, 24, rgba(6,6,10,232));
    roundedOutline({listX, listY, listW, listH}, 24, rgba(49,49,60), 1);
    const std::size_t visibleRows = 9;
    std::size_t start = selectedSetting >= visibleRows ? selectedSetting - visibleRows + 1 : 0;
    if (start + visibleRows > rows.size()) start = rows.size() - visibleRows;
    for (std::size_t slot = 0; slot < visibleRows; ++slot) {
        const std::size_t i = start + slot;
        const int y = listY + 18 + static_cast<int>(slot) * 57;
        const bool on = i == selectedSetting;
        if (on) {
            roundedFill({listX + 14, y, listW - 28, 49}, 15, rgba(accent.r,accent.g,accent.b,27));
            roundedOutline({listX + 14, y, listW - 28, 49}, 15, rgba(accent.r,accent.g,accent.b,160), 1);
            roundedFill({listX + 14, y + 8, 4, 33}, 2, accent);
        }
        text(fonts_.small, rows[i].name, listX + 31, y + 5, on ? rgba(255,255,255) : rgba(211,211,220), 340);
        textRight(fonts_.small, rows[i].value, listX + listW - 32, y + 5, on ? accent : rgba(158,158,171));
        text(fonts_.tiny, rows[i].description, listX + 31, y + 29, rgba(116,116,129), listW - 65);
    }

    const int previewX = listX + listW + 18, previewW = W - previewX - CONTENT_R;
    roundedFill({previewX, listY, previewW, 326}, 24, rgba(7,7,12,238));
    roundedOutline({previewX, listY, previewW, 326}, 24, rgba(52,52,64), 1);
    text(fonts_.tiny, "LIVE THEME PREVIEW", previewX + 24, listY + 24, accent);
    roundedFill({previewX + 24, listY + 58, previewW - 48, 152}, 21, rgba(2,2,5));
    roundedFill({previewX + 24, listY + 58, 6, 152}, 3, accent);
    int brandW = 0, brandH = 0;
    SDL_Texture* previewBrand = loadImage("sdmc:/switch/advance/assets/sidebar-mark.png", brandW, brandH);
    if (!previewBrand) previewBrand = loadImage("sdmc:/switch/advance/assets/store-icon.png", brandW, brandH);
    if (!previewBrand) previewBrand = loadImage("assets/sidebar-mark.png", brandW, brandH);
    if (!previewBrand) previewBrand = loadImage("assets/store-icon.png", brandW, brandH);
    if (previewBrand && brandW > 0 && brandH > 0) {
        SDL_Rect dst{previewX + 36, listY + 76, 108, 108};
        SDL_RenderCopy(renderer_, previewBrand, nullptr, &dst);
    } else {
        drawBrandHandheld(previewX + 42, listY + 88, 96, 66, accent);
    }
    text(fonts_.small, "ADVANCE", previewX + 154, listY + 84, rgba(255,255,255));
    text(fonts_.tiny, "THEME", previewX + 154, listY + 110, rgba(121,121,134));
    roundedFill({previewX + 154, listY + 143, 120, 34}, 14, accent);
    text(fonts_.tiny, "A  PLAY", previewX + 178, listY + 151, rgba(255,255,255));
    text(fonts_.tiny, prettyThemeName(config.theme), previewX + 24, listY + 229, rgba(166,166,179));
    const std::array<SDL_Color,7> paletteDots = {{
        rgba(255,39,65), rgba(146,96,255), rgba(158,92,255), rgba(34,218,131),
        rgba(255,184,61), rgba(67,184,255), rgba(255,75,125)
    }};
    const std::array<const char*,7> paletteKeys = {{
        "crimson", "aurora-violet", "atomic-purple", "emerald", "midnight-gold", "ice-blue", "neon-coral"
    }};
    for (std::size_t i = 0; i < paletteDots.size(); ++i) {
        const int dx = previewX + 24 + static_cast<int>(i) * 24;
        circle(dx + 7, listY + 260, config.theme == paletteKeys[i] ? 7 : 5, paletteDots[i], true);
    }
    text(fonts_.tiny, "Advance stores settings in a human-readable config.ini.", previewX + 24, listY + 282, rgba(115,115,128), previewW - 48);

    roundedFill({previewX, listY + 344, previewW, 204}, 24, rgba(7,7,12,232));
    roundedOutline({previewX, listY + 344, previewW, 204}, 24, rgba(52,52,64), 1);
    text(fonts_.tiny, "YOUR LIBRARY", previewX + 24, listY + 368, accent);
    text(fonts_.body, std::to_string(gameCount) + (gameCount == 1 ? " game ready" : " games ready"), previewX + 24, listY + 393, rgba(255,255,255), previewW - 48);
    roundedFill({previewX + 24, listY + 430, previewW - 48, 54}, 17, rgba(255,255,255,6));
    text(fonts_.tiny, "METADATA", previewX + 40, listY + 440, rgba(191,191,204));
    text(fonts_.tiny, "COLLECTIONS", previewX + 132, listY + 440, rgba(191,191,204));
    text(fonts_.tiny, "PLAY HISTORY", previewX + 236, listY + 440, rgba(191,191,204));
    text(fonts_.tiny, "advance.json", previewX + 40, listY + 460, rgba(115,115,129));
    text(fonts_.tiny, "smart + custom", previewX + 132, listY + 460, rgba(115,115,129));
    text(fonts_.tiny, "approximate", previewX + 236, listY + 460, rgba(115,115,129));
    roundedFill({previewX + 24, listY + 505, 112, 28}, 12, rgba(accent.r,accent.g,accent.b,30));
    text(fonts_.tiny, "X  RESCAN", previewX + 80, listY + 511, accent, 0, true);
    roundedFill({previewX + 146, listY + 505, 104, 28}, 12, rgba(255,255,255,8));
    text(fonts_.tiny, "R  ABOUT", previewX + 198, listY + 511, rgba(198,198,210), 0, true);

    drawFooter({{"←/→","Change"},{"A","Select"},{"X","Rescan"},{"R","About"},{"B","Back"}});
    drawScreenTransition(Screen::Settings, config);
    drawSidebarOverlay(Screen::Settings, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderAbout(const Config& config, const std::string& toast) {
    drawBaseBackdrop(config);
    drawSidebar(Screen::About, Shelf::All, config);
    drawTopChrome("ADVANCE  /  ABOUT", "Advance 0.4", "A definitive Game Boy Advance library experience", config);
    const SDL_Color accent = accentColor(config);
    const int x = CONTENT_X, y = 118, w = W - CONTENT_X - CONTENT_R;
    roundedFill({x + 7, y + 10, w, 470}, 31, rgba(0,0,0,150));
    roundedFill({x, y, w, 470}, 31, rgba(6,6,10,240));
    roundedOutline({x, y, w, 470}, 31, rgba(55,55,68), 1);
    roundedFill({x + 2, y + 2, w - 4, 2}, 1, rgba(255,255,255,18));

    int aboutW = 0, aboutH = 0;
    SDL_Texture* aboutBrand = loadImage("sdmc:/switch/advance/assets/sidebar-mark.png", aboutW, aboutH);
    if (!aboutBrand) aboutBrand = loadImage("assets/sidebar-mark.png", aboutW, aboutH);
    if (aboutBrand && aboutW > 0 && aboutH > 0) {
        SDL_Rect dst{x + 34, y + 24, 132, 132};
        SDL_RenderCopy(renderer_, aboutBrand, nullptr, &dst);
    } else {
        drawBrandHandheld(x + 36, y + 45, 130, 92, accent);
    }
    text(fonts_.mega, "ADVANCE", x + 190, y + 34, rgba(255,255,255), 620);
    text(fonts_.body, "Your GBA library. Your profile. Your shelves. Your style.", x + 194, y + 105, rgba(178,178,190), 700);

    const std::array<std::pair<const char*,const char*>,6> features = {{
        {"HOME", "Continue Playing, Recently Added, Favorites, Most Played, and smart discovery."},
        {"METADATA", "advance.json supports titles, authors, versions, base games, genres, tags, descriptions, banners, and screenshots."},
        {"COLLECTIONS", "Automatic shelves plus unlimited personal collections stored locally on your SD card."},
        {"HISTORY", "Launches, last played, completion state, favorites, hidden games, and approximate play time."},
        {"PERSONAL", "Your Switch avatar and nickname, seven OLED themes, adaptive accents, transitions, touch, motion, dynamic backgrounds, and UI sounds."},
        {"OPEN", "No ROM downloads, no bundled copyrighted art, and no modification of your existing mGBA library."}
    }};
    for (int i = 0; i < 6; ++i) {
        const int col = i % 2, row = i / 2;
        const int fx = x + 40 + col * 560, fy = y + 195 + row * 76;
        roundedFill({fx, fy, 520, 62}, 15, rgba(11,11,16));
        text(fonts_.tiny, features[i].first, fx + 16, fy + 11, accent, 120);
        text(fonts_.tiny, features[i].second, fx + 126, fy + 11, rgba(180,180,191), 375);
    }

    text(fonts_.tiny, "Advance 0.4.0 · Native Nintendo Switch homebrew · mGBA remains the emulator backend",
         x + 40, y + 436, rgba(116,116,129), w - 80);
    drawFooter({{"A/B","Back"}});
    drawScreenTransition(Screen::About, config);
    drawSidebarOverlay(Screen::About, Shelf::All, config);
    drawToast(toast);
}

void Renderer::renderLaunch(const Game& game, const Config& config) {
    drawDynamicBackdrop(&game, config, 88);
    const SDL_Color accent = accentColor(config);
    fill({0, 0, W, H}, rgba(0,0,0,120));
    const int panelW = 660, panelH = 300;
    const int px = W / 2 - panelW / 2, py = H / 2 - panelH / 2;
    roundedFill({px + 6, py + 10, panelW, panelH}, 30, rgba(0,0,0,150));
    roundedFill({px, py, panelW, panelH}, 30, rgba(6,6,11,242));
    roundedOutline({px, py, panelW, panelH}, 30, rgba(accent.r,accent.g,accent.b,155), 2);
    roundedFill({px, py, 7, panelH}, 4, accent);

    drawCoverCard(game, {px + 28, py + 28, 150, 220}, false, config, 1.0f, true);
    text(fonts_.tiny, "HANDING OFF TO mGBA", px + 208, py + 46, accent, 380);
    text(fonts_.large, game.title, px + 208, py + 74, rgba(255,255,255), 410);
    text(fonts_.small, game.launches > 1 ? "Welcome back." : "Starting your adventure.", px + 208, py + 130, rgba(182,182,194), 410);
    roundedFill({px + 208, py + 182, 338, 4}, 2, rgba(55,55,68));
    const int travel = static_cast<int>((SDL_GetTicks64() / 4) % 280);
    roundedFill({px + 208 + travel, py + 182, 58, 4}, 2, accent);
    text(fonts_.tiny, "ADVANCE keeps your library, artwork, favorites, and play history ready for your return.",
         px + 208, py + 207, rgba(125,125,139), 390);
}

void Renderer::renderBoot(const Config& config, const std::string& message) {
    drawBaseBackdrop(config);
    const SDL_Color accent = accentColor(config);
    const int cx = W / 2, cy = H / 2 - 20;
    for (int i = 0; i < 8; ++i) {
        const int d = 105 + i * 26;
        roundedOutline({cx - d, cy - d / 2, d * 2, d}, d / 2, rgba(accent.r,accent.g,accent.b,static_cast<Uint8>(38 - i * 4)), 1);
    }
    int bootW = 0, bootH = 0;
    SDL_Texture* bootBrand = loadImage("sdmc:/switch/advance/assets/sidebar-mark.png", bootW, bootH);
    if (!bootBrand) bootBrand = loadImage("assets/sidebar-mark.png", bootW, bootH);
    if (bootBrand && bootW > 0 && bootH > 0) {
        SDL_Rect dst{cx - 84, cy - 96, 168, 168};
        SDL_RenderCopy(renderer_, bootBrand, nullptr, &dst);
    } else {
        drawBrandHandheld(cx - 94, cy - 78, 188, 122, accent);
    }
    text(fonts_.hero, "ADVANCE", cx, cy + 76, rgba(255,255,255), 0, true);
    text(fonts_.small, "YOUR GAME BOY ADVANCE UNIVERSE", cx, cy + 124, rgba(161,161,174), 0, true);
    text(fonts_.tiny, message, cx, cy + 158, rgba(119,119,132), 680, true);
    const int barW = 310;
    roundedFill({cx - barW / 2, cy + 194, barW, 4}, 2, rgba(43,43,52));
    const int pulse = 70 + static_cast<int>((SDL_GetTicks64() / 7) % (barW - 70));
    roundedFill({cx - barW / 2 + pulse - 70, cy + 194, 70, 4}, 2, accent);
}

void Renderer::drawToast(const std::string& toast) {
    if (toast.empty()) return;
    int tw = 0, th = 0;
    TTF_SizeUTF8(fonts_.small, toast.c_str(), &tw, &th);
    const int w = std::min(520, tw + 72);
    const int x = W - CONTENT_R - w;
    const int y = H - FOOTER_H - 60;
    roundedFill({x + 4, y + 5, w, 44}, 18, rgba(0,0,0,150));
    roundedFill({x, y, w, 44}, 18, rgba(18,18,26,246));
    roundedOutline({x, y, w, 44}, 18, rgba(94,94,108), 1);
    roundedFill({x + 10, y + 9, 26, 26}, 13, rgba(245,245,250));
    text(fonts_.tiny, "✓", x + 23, y + 10, rgba(18,18,24), 0, true);
    text(fonts_.small, toast, x + 47, y + 10, rgba(255,255,255), w - 60);
}

std::optional<std::size_t> Renderer::hitTestLibrary(int x, int y,
                                                    std::size_t visibleCount,
                                                    std::size_t selected,
                                                    const Config& config) const {
    const GalleryLayout l = galleryLayout(config);
    if (x < l.x || y < l.y || x >= l.x + l.w || y >= l.y + l.h) return std::nullopt;
    const int unitW = l.cardW + l.gap, unitH = l.cardH + l.gap;
    const int col = (x - l.x) / unitW, row = (y - l.y) / unitH;
    if (col < 0 || col >= l.cols || row < 0 || row >= l.rows) return std::nullopt;
    const int localX = (x - l.x) % unitW, localY = (y - l.y) % unitH;
    if (localX >= l.cardW || localY >= l.cardH) return std::nullopt;
    const std::size_t page = l.perPage ? selected / l.perPage : 0;
    const std::size_t index = page * l.perPage + static_cast<std::size_t>(row * l.cols + col);
    return index < visibleCount ? std::optional<std::size_t>(index) : std::nullopt;
}

std::optional<std::pair<std::size_t, std::size_t>> Renderer::hitTestHome(int x, int y,
                                                                         const std::vector<HomeRow>& rows,
                                                                         std::size_t selectedRow,
                                                                         std::size_t selectedColumn) const {
    if (rows.empty()) return std::nullopt;
    constexpr int cardW = 148, cardH = 144, gap = 14, visibleCards = 7;
    const std::array<int,2> ys = {HEADER_H + 242, HEADER_H + 424};
    const std::size_t r0 = std::min(selectedRow, rows.size() - 1);
    const std::size_t r1 = r0 + 1 < rows.size() ? r0 + 1 : (r0 > 0 ? r0 - 1 : r0);
    const std::array<std::size_t,2> rs = {r0, r1};
    for (int lineIndex = 0; lineIndex < 2; ++lineIndex) {
        const std::size_t r = rs[lineIndex];
        if (lineIndex == 1 && r == r0) continue;
        const int cardY = ys[lineIndex] + 34;
        if (y < cardY || y >= cardY + cardH || x < CONTENT_X) continue;
        const int slot = (x - CONTENT_X) / (cardW + gap);
        if (slot < 0 || slot >= visibleCards || (x - CONTENT_X) % (cardW + gap) >= cardW) continue;
        std::size_t start = 0;
        if (r == selectedRow) {
            if (selectedColumn >= visibleCards) start = selectedColumn - visibleCards + 1;
            if (rows[r].games.size() > visibleCards && start + visibleCards > rows[r].games.size())
                start = rows[r].games.size() - visibleCards;
        }
        const std::size_t index = start + static_cast<std::size_t>(slot);
        if (index < rows[r].games.size()) return std::make_pair(r, index);
    }
    return std::nullopt;
}

int Renderer::hitTestSidebar(int x, int y, bool expanded) const {
    const int width = expanded ? SIDEBAR_EXPANDED_W : SIDEBAR_W;
    if (x < 0 || x >= width) return -1;
    const std::array<int, 7> ys = {146, 226, 306, 386, 466, 546, 642};
    for (int i = 0; i < 7; ++i) if (std::abs(y - ys[i]) <= 31) return i;
    return -1;
}

} // namespace advance
