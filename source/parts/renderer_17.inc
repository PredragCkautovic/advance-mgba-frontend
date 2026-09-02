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
        {"Theme", config.theme, "Six signature palettes built for OLED."},
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
    drawBrandHandheld(previewX + 42, listY + 88, 96, 66, accent);
    text(fonts_.small, "ADVANCE", previewX + 154, listY + 84, rgba(255,255,255));
    text(fonts_.tiny, "THEME", previewX + 154, listY + 110, rgba(121,121,134));
    roundedFill({previewX + 154, listY + 143, 120, 34}, 14, accent);
    text(fonts_.tiny, "A  PLAY", previewX + 178, listY + 151, rgba(255,255,255));
    text(fonts_.tiny, config.theme, previewX + 24, listY + 229, rgba(166,166,179));
