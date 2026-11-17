#include "config.h"
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include <map>

static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

void ParseHudConfig(const std::string& text, HudConfig& c) {
    std::map<std::string, std::string> kv;
    std::istringstream in(text);
    std::string line, sec;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') { size_t e = line.find(']'); if (e != std::string::npos) sec = Lower(Trim(line.substr(1, e - 1))); continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string val = Trim(line.substr(eq + 1));
        size_t sc = val.find(';');
        if (sc != std::string::npos) val = Trim(val.substr(0, sc));
        kv[sec + "." + Lower(Trim(line.substr(0, eq)))] = val;
    }
    auto B = [&](const char* k, bool& v) { auto it = kv.find(k); if (it != kv.end()) v = atoi(it->second.c_str()) != 0; };
    auto F = [&](const char* k, float& v) { auto it = kv.find(k); if (it != kv.end()) v = (float)atof(it->second.c_str()); };
    auto I = [&](const char* k, int& v) { auto it = kv.find(k); if (it != kv.end()) v = atoi(it->second.c_str()); };

    B("hud.statusbars", c.statusBars); B("hud.staminabar", c.staminaBar); B("hud.oxygenbar", c.oxygenBar);
    B("hud.edgeglow", c.edgeGlow); B("hud.wantedstars", c.wantedStars); B("hud.ammo", c.ammo);
    B("hud.weaponicon", c.weaponIcon); B("hud.money", c.money); B("hud.helpbox", c.helpBox); B("hud.subtitles", c.subtitles);
    F("hud.barsx", c.barsX); F("hud.barsy", c.barsY); F("hud.barsscale", c.barsScale);
    F("hud.columnx", c.columnX); F("hud.columny", c.columnY); F("hud.columnscale", c.columnScale);
    F("hud.helpx", c.helpX); F("hud.helpy", c.helpY); F("hud.helpscale", c.helpScale);

    B("minimap.show", c.minimapShow); B("minimap.gta6radartiles", c.gta6RadarTiles); B("minimap.gangareas", c.gangAreas);
    B("minimap.othermodsblips", c.otherModsBlips); B("minimap.nightcolours", c.nightColours);
    B("minimap.wantedcolours", c.wantedColours); B("minimap.locationplate", c.locationPlate); B("minimap.vehiclecard", c.vehicleCard);
    F("minimap.left", c.mmLeft); F("minimap.bottom", c.mmBottom); F("minimap.scale", c.mmScale);
    F("minimap.tilt", c.mmTilt); F("minimap.zoomout", c.mmZoomOut); F("minimap.zoomhold", c.mmZoomHold);
    I("minimap.zoomkey", c.zoomKey);

    B("weaponwheel.show", c.wheelShow); I("weaponwheel.key", c.wheelKey);
}

bool LoadHudConfig(const char* path, HudConfig& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss; ss << f.rdbuf();
    ParseHudConfig(ss.str(), out);
    return true;
}
