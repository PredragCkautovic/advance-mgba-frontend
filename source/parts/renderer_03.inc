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
