#include "../src/config.h"
#include <cstdio>
#include <cmath>
#define CHECK(x) do{ if(!(x)){ printf("GAGAL: %s (baris %d)\n", #x, __LINE__); return 1; } }while(0)
int main() {
    HudConfig c;
    CHECK(LoadHudConfig("test/GTA6HUD.ini", c));
    CHECK(c.statusBars && !c.staminaBar && c.wantedStars && c.helpBox && c.subtitles);
    CHECK(c.minimapShow && std::fabs(c.mmLeft - 18.0f) < 1e-4 && std::fabs(c.mmBottom - 13.5f) < 1e-4);
    CHECK(std::fabs(c.mmTilt - 0.8f) < 1e-4 && std::fabs(c.mmZoomOut - 2.5f) < 1e-4 && c.zoomKey == 90);
    CHECK(c.wheelShow && c.wheelKey == 9 && !c.otherModsBlips);
    HudConfig d; ParseHudConfig("[Minimap]\nShow=0\n[HUD]\nWantedStars = 0 ; x\n", d);
    CHECK(!d.minimapShow && !d.wantedStars && d.statusBars);
    printf("Semua tes config lulus\n");
    return 0;
}
