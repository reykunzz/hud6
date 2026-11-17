#pragma once
#include <string>

// Pengaturan kompatibel dengan GTA6HUD.ini versi PC (fase 1: flag tampil/sembunyi).
struct HudConfig {
    // [HUD]
    bool statusBars = true, staminaBar = false, oxygenBar = false, edgeGlow = false;
    bool wantedStars = true, ammo = true, weaponIcon = true, money = true;
    bool helpBox = true, subtitles = true;
    float barsX = 0, barsY = 0, barsScale = 1;
    float columnX = 0, columnY = 0, columnScale = 1;
    float helpX = 0, helpY = 0, helpScale = 1;
    // [Minimap]
    bool minimapShow = true, gta6RadarTiles = true, gangAreas = true, otherModsBlips = false;
    bool nightColours = true, wantedColours = true, locationPlate = true, vehicleCard = true;
    float mmLeft = 18.0f, mmBottom = 13.5f, mmScale = 1.0f, mmTilt = 0.8f, mmZoomOut = 2.5f, mmZoomHold = 2.0f;
    int zoomKey = 90;
    // [WeaponWheel]
    bool wheelShow = true;
    int wheelKey = 9;
};

void ParseHudConfig(const std::string& text, HudConfig& out);
bool LoadHudConfig(const char* path, HudConfig& out);
