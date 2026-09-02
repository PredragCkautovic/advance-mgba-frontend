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
    if (clear) clear << "Advance 0.1 diagnostics\n";
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
