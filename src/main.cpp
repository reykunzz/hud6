// GTA6 HUD untuk GTA SA Android (arm64) - FASE 2 (HUD dasar)
// - Ikon senjata & ammo: texture diambil dari TXD game (HUD.TXD di dalam IMG), tanpa file PNG.
// - Bar health/armor kustom. Semua pembacaan memori divalidasi; kalau tidak masuk akal -> pakai HUD asli.
// - Pengaman crash: file .initlock (lihat README).
#include <mod/amlmod.h>
#include <mod/logger.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <string>
#include "config.h"
#include "weapons.h"

MYMODCFG(net.user.gta6hud, GTA6 HUD, 0.2.0, user)

// ---- tipe game (layout sama dengan game; ukuran/HFA dicocokkan) -------------------------
struct CRect { float x1, y1, x2, y2; };            // (left, bottom, right, top) di game; dipakai sebagai x1,y1,x2,y2 layar
struct CRGBA { uint8_t r, g, b, a; };
struct CSprite2d { void* tex = nullptr; };         // m_pTexture di offset 0

// ---- offset struktur (hasil baca kode libGTASA 2.10 arm64) -----------------------------
// Semua dicek kewajarannya saat runtime. Ubah di sini kalau versi game berbeda.
static const int OFF_PED_HEALTH       = 1708;   // float, dipakai CWidgetPlayerInfo::RenderHealthBar
static const int OFF_PED_ARMOUR       = 1716;   // float, DUGAAN (health + 8, seperti layout PC) -> divalidasi
static const int OFF_PED_WEAPONS      = 1840;   // CWeapon[13], tiap elemen 32 byte (terbaca di CWidgetPlayerInfo::Draw)
static const int WEAPON_STRIDE        = 32;
static const int OFF_PED_ACTIVE_SLOT  = 2268;   // int8, terbaca di DrawWeaponIcon -> divalidasi 0..12
static const int OFF_WEAPON_TYPE      = 0;
static const int OFF_WEAPON_CLIP      = 8;      // DUGAAN (layout PC) -> divalidasi
static const int OFF_WEAPON_TOTAL     = 12;     // DUGAAN (layout PC) -> divalidasi

static HudConfig g_cfg;
static bool g_enabled = true;      // false bila crash terdeteksi pada peluncuran sebelumnya
static int  g_frames = 0;
static std::string g_dir;          // .../files/GTA6HUD/

// ---- fungsi game (diambil lewat dlsym) --------------------------------------------------
static void* g_hGTASA = nullptr;
static void* (*FindPlayerPed)(int) = nullptr;
static int   (*CTxdStore_FindTxdSlot)(const char*) = nullptr;
static void  (*CTxdStore_PushCurrentTxd)() = nullptr;
static void  (*CTxdStore_PopCurrentTxd)() = nullptr;
static void  (*CTxdStore_SetCurrentTxd)(int, const char*) = nullptr;
static void  (*CSprite2d_SetTexture)(CSprite2d*, char*) = nullptr;
static void  (*CSprite2d_Draw)(CSprite2d*, const CRect&, const CRGBA&) = nullptr;
static void  (*CSprite2d_DrawRect)(const CRect&, const CRGBA&) = nullptr;
static void  (*CFont_SetFontStyle)(uint8_t) = nullptr;
static void  (*CFont_SetScale)(float) = nullptr;
static void  (*CFont_SetColor)(CRGBA) = nullptr;
static void  (*CFont_SetProportional)(uint8_t) = nullptr;
static void  (*CFont_SetBackground)(uint8_t, uint8_t) = nullptr;
static void  (*CFont_SetJustify)(uint8_t) = nullptr;
static void  (*CFont_PrintString)(float, float, uint16_t*) = nullptr;
static void  (*CFont_RenderFontBuffer)() = nullptr;

template <typename T> static bool Sym(const char* n, T& out) {
    out = (T)dlsym(g_hGTASA, n);
    if (!out) logger->Error("GTA6HUD: simbol hilang: %s", n);
    return out != nullptr;
}
template <typename Fn> static bool HookSym(const char* sym, Fn rep, Fn* orig) {
    void* a = dlsym(g_hGTASA, sym);
    if (!a) { logger->Error("GTA6HUD: hook gagal, simbol hilang: %s", sym); return false; }
    aml->Hook(a, (void*)rep, (void**)orig);
    logger->Info("GTA6HUD: hook OK: %s", sym);
    return true;
}

// ---- akses memori yang aman --------------------------------------------------------------
static bool PtrOK(const void* p) { return p && ((uintptr_t)p & 3) == 0 && (uintptr_t)p > 0x10000; }
template <typename T> static T Rd(const void* base, int off) { T v; memcpy(&v, (const char*)base + off, sizeof(T)); return v; }
static bool Sane(float v, float lo, float hi) { return v == v && v >= lo && v <= hi; }

struct PlayerView { float health, armour; bool armourOK; int weapon; int clip, total; bool weaponOK, ammoOK; };
static bool ReadPlayer(PlayerView& pv) {
    void* ped = FindPlayerPed ? FindPlayerPed(0) : nullptr;
    if (!PtrOK(ped)) return false;
    pv.health = Rd<float>(ped, OFF_PED_HEALTH);
    if (!Sane(pv.health, 0.0f, 2000.0f)) return false;
    pv.armour = Rd<float>(ped, OFF_PED_ARMOUR);
    pv.armourOK = Sane(pv.armour, 0.0f, 300.0f);
    int slot = Rd<int8_t>(ped, OFF_PED_ACTIVE_SLOT);
    pv.weaponOK = pv.ammoOK = false; pv.weapon = 0; pv.clip = pv.total = 0;
    if (slot >= 0 && slot <= 12) {
        const char* w = (const char*)ped + OFF_PED_WEAPONS + slot * WEAPON_STRIDE;
        int t = Rd<int>(w, OFF_WEAPON_TYPE);
        if (t >= 0 && t <= 46) {
            pv.weapon = t; pv.weaponOK = true;
            pv.clip = Rd<int>(w, OFF_WEAPON_CLIP); pv.total = Rd<int>(w, OFF_WEAPON_TOTAL);
            pv.ammoOK = pv.clip >= 0 && pv.clip < 100000 && pv.total >= 0 && pv.total < 1000000;
        }
    }
    return true;
}

// ---- ukuran layar -------------------------------------------------------------------------
static float g_W = 0, g_H = 0;
static void UpdateScreen() {
    if (g_W > 0) return;
    void* rs = dlsym(g_hGTASA, "RsGlobal");
    if (rs) {
        const int cand[][2] = { {8, 12}, {4, 8}, {16, 20} };
        for (auto& c : cand) {
            int w = Rd<int>(rs, c[0]), h = Rd<int>(rs, c[1]);
            if (w >= 320 && w <= 8000 && h >= 200 && h <= 5000 && w > h) { g_W = (float)w; g_H = (float)h; break; }
        }
    }
    if (g_W <= 0) { int w = 0, h = 0; aml->GetDisplaySize(&w, &h); if (w > h && h > 0) { g_W = (float)w; g_H = (float)h; } }
    logger->Info("GTA6HUD: layar %.0fx%.0f", g_W, g_H);
}

// ---- texture ikon dari TXD game ------------------------------------------------------------
static CSprite2d g_icons[47];
static uint8_t   g_iconTried[47];
static CSprite2d* GetIcon(int id) {
    if (id < 0 || id > 46) return nullptr;
    if (!g_iconTried[id]) {
        g_iconTried[id] = 1;
        const char* name = WeaponIconName(id);
        if (name && CTxdStore_FindTxdSlot) {
            int slot = CTxdStore_FindTxdSlot("hud");
            if (slot >= 0) {
                CTxdStore_PushCurrentTxd(); CTxdStore_SetCurrentTxd(slot, "hud");
                CSprite2d_SetTexture(&g_icons[id], (char*)name);
                CTxdStore_PopCurrentTxd();
            }
            logger->Info("GTA6HUD: ikon %s %s", name, g_icons[id].tex ? "ditemukan" : "TIDAK ada di HUD.TXD");
        }
    }
    return g_icons[id].tex ? &g_icons[id] : nullptr;
}

// ---- hook -------------------------------------------------------------------------------------
static void (*CHud_Draw_o)();
static void (*CHud_DrawRadar_o)();
static void (*CHud_DrawHelpText_o)();
static void (*CHud_DrawSubtitles_o)();
static void (*Widget_DrawWanted_o)(void*);
static void (*Widget_RenderHealthBar_o)(void*, int, float);
static void (*Widget_RenderBreathBar_o)(void*, int, float);
static void (*Widget_DrawWeaponIcon_o)(void*, void*, CRect, float);

static bool g_customBars = false;   // true bila pembacaan health kemarin masuk akal -> bar asli diganti

static void DrawBar(float x, float y, float w, float h, float frac, CRGBA fill) {
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    CSprite2d_DrawRect({ x - 2, y - 2, x + w + 2, y + h + 2 }, { 0, 0, 0, 160 });
    CSprite2d_DrawRect({ x, y, x + w, y + h }, { 40, 40, 40, 200 });
    if (frac > 0) CSprite2d_DrawRect({ x, y, x + w * frac, y + h }, fill);
}

static void CHud_Draw_h() {
    CHud_Draw_o();
    if (!g_enabled) return;
    g_frames++;
    if (g_frames == 300) { unlink((g_dir + ".initlock").c_str()); logger->Info("GTA6HUD: 300 frame aman, initlock dihapus"); }

    UpdateScreen();
    PlayerView pv;
    if (g_W <= 0 || !CSprite2d_DrawRect || !ReadPlayer(pv)) { g_customBars = false; return; }
    g_customBars = g_cfg.statusBars;
    if (!g_cfg.statusBars) return;

    float s = g_H / 1080.0f;                          // satuan config = piksel pada 1080p
    float w = 330 * s * g_cfg.barsScale, h = 16 * s * g_cfg.barsScale;
    float x = g_W * 0.5f - w * 0.5f + g_cfg.barsX * s;
    float y = g_H - 70 * s + g_cfg.barsY * s;
    float maxHp = 100.0f;
    DrawBar(x, y, w, h, pv.health / maxHp, { 120, 220, 90, 255 });
    if (pv.armourOK && pv.armour > 0.5f) DrawBar(x, y - h - 10 * s, w, h * 0.8f, pv.armour / 100.0f, { 90, 170, 240, 255 });

    if (g_cfg.ammo && pv.ammoOK && pv.weaponOK && pv.weapon >= 16 && CFont_PrintString && pv.total > 0) {
        int loaded = pv.clip, spare = pv.total - pv.clip; if (spare < 0) spare = 0;
        char tmp[32]; snprintf(tmp, sizeof tmp, "%d-%d", loaded, spare);
        uint16_t wide[32]; int n = 0; for (; tmp[n] && n < 31; n++) wide[n] = (uint16_t)tmp[n]; wide[n] = 0;
        CFont_SetFontStyle(1); CFont_SetProportional(1); CFont_SetBackground(0, 0); CFont_SetJustify(0);
        CFont_SetScale(0.5f * s * g_cfg.columnScale); CFont_SetColor({ 255, 255, 255, 255 });
        CFont_PrintString(x + w + 14 * s, y - 6 * s, wide);
        CFont_RenderFontBuffer();
    }
}

static void CHud_DrawRadar_h()     { if (g_cfg.minimapShow) CHud_DrawRadar_o(); }
static void CHud_DrawHelpText_h()  { if (g_cfg.helpBox)    CHud_DrawHelpText_o(); }
static void CHud_DrawSubtitles_h() { if (g_cfg.subtitles)  CHud_DrawSubtitles_o(); }
static void Widget_DrawWanted_h(void* self) { if (g_cfg.wantedStars) Widget_DrawWanted_o(self); }
static void Widget_RenderHealthBar_h(void* self, int a, float b) {
    if (!g_cfg.statusBars) return;
    if (g_enabled && g_customBars) return;          // bar kustom menggantikan
    Widget_RenderHealthBar_o(self, a, b);
}
static void Widget_RenderBreathBar_h(void* self, int a, float b) { if (g_cfg.oxygenBar) Widget_RenderBreathBar_o(self, a, b); }

static void Widget_DrawWeaponIcon_h(void* self, void* ped, CRect r, float f) {
    if (!g_cfg.weaponIcon) return;
    if (g_enabled && CSprite2d_Draw && PtrOK(ped)) {
        PlayerView pv;
        if (ReadPlayer(pv) && pv.weaponOK) {
            if (CSprite2d* ic = GetIcon(pv.weapon)) { CSprite2d_Draw(ic, r, { 255, 255, 255, 255 }); return; }
        }
    }
    Widget_DrawWeaponIcon_o(self, ped, r, f);        // tidak ada texture / data aneh -> ikon asli
}

// ---- config & init ------------------------------------------------------------------------------
static void LoadConfigAndPaths() {
    char pkg[128] = {0};
    if (FILE* f = fopen("/proc/self/cmdline", "r")) { fread(pkg, 1, sizeof(pkg) - 1, f); fclose(f); }
    std::string base = std::string("/storage/emulated/0/Android/data/") + pkg + "/files/";
    g_dir = base + "GTA6HUD/";
    mkdir(g_dir.c_str(), 0777);
    const char* names[] = { "GTA6HUD/GTA6HUD.ini", "GTA6HUD.ini", "mods/GTA6HUD.ini" };
    for (auto n : names) if (LoadHudConfig((base + n).c_str(), g_cfg)) { logger->Info("GTA6HUD: config: %s%s", base.c_str(), n); return; }
    logger->Info("GTA6HUD: GTA6HUD.ini tidak ada, pakai default");
}

extern "C" void OnModLoad() {
    logger->SetTag("GTA6HUD");
    g_hGTASA = dlopen("libGTASA.so", RTLD_NOW | RTLD_NOLOAD);
    if (!g_hGTASA) { logger->Error("GTA6HUD: libGTASA.so tidak ditemukan"); return; }
    LoadConfigAndPaths();

    std::string lock = g_dir + ".initlock";
    if (access(lock.c_str(), F_OK) == 0) {
        g_enabled = false;
        logger->Error("GTA6HUD: .initlock ada (game crash sebelum 300 frame?). Mode aman: hanya toggle HUD. Hapus file itu untuk mencoba lagi.");
    } else if (FILE* f = fopen(lock.c_str(), "w")) { fputs("1", f); fclose(f); }

    bool ok = true;
    ok &= Sym("_Z13FindPlayerPedi", FindPlayerPed);
    ok &= Sym("_ZN9CTxdStore11FindTxdSlotEPKc", CTxdStore_FindTxdSlot);
    ok &= Sym("_ZN9CTxdStore14PushCurrentTxdEv", CTxdStore_PushCurrentTxd);
    ok &= Sym("_ZN9CTxdStore13PopCurrentTxdEv", CTxdStore_PopCurrentTxd);
    ok &= Sym("_ZN9CTxdStore13SetCurrentTxdEiPKc", CTxdStore_SetCurrentTxd);
    ok &= Sym("_ZN9CSprite2d10SetTextureEPc", CSprite2d_SetTexture);
    ok &= Sym("_ZN9CSprite2d4DrawERK5CRectRK5CRGBA", CSprite2d_Draw);
    ok &= Sym("_ZN9CSprite2d8DrawRectERK5CRectRK5CRGBA", CSprite2d_DrawRect);
    Sym("_ZN5CFont12SetFontStyleEh", CFont_SetFontStyle); Sym("_ZN5CFont8SetScaleEf", CFont_SetScale);
    Sym("_ZN5CFont8SetColorE5CRGBA", CFont_SetColor);     Sym("_ZN5CFont15SetProportionalEh", CFont_SetProportional);
    Sym("_ZN5CFont13SetBackgroundEhh", CFont_SetBackground); Sym("_ZN5CFont10SetJustifyEh", CFont_SetJustify);
    Sym("_ZN5CFont11PrintStringEffPt", CFont_PrintString);   Sym("_ZN5CFont16RenderFontBufferEv", CFont_RenderFontBuffer);
    if (!ok) { g_enabled = false; logger->Error("GTA6HUD: simbol penting hilang -> mode aman"); }

    HookSym("_ZN4CHud4DrawEv",            CHud_Draw_h,           &CHud_Draw_o);
    HookSym("_ZN4CHud9DrawRadarEv",       CHud_DrawRadar_h,      &CHud_DrawRadar_o);
    HookSym("_ZN4CHud12DrawHelpTextEv",   CHud_DrawHelpText_h,   &CHud_DrawHelpText_o);
    HookSym("_ZN4CHud13DrawSubtitlesEv",  CHud_DrawSubtitles_h,  &CHud_DrawSubtitles_o);
    HookSym("_ZN17CWidgetPlayerInfo10DrawWantedEv",       Widget_DrawWanted_h,      &Widget_DrawWanted_o);
    HookSym("_ZN17CWidgetPlayerInfo15RenderHealthBarEif", Widget_RenderHealthBar_h, &Widget_RenderHealthBar_o);
    HookSym("_ZN17CWidgetPlayerInfo15RenderBreathBarEif", Widget_RenderBreathBar_h, &Widget_RenderBreathBar_o);
    HookSym("_ZN17CWidgetPlayerInfo14DrawWeaponIconEP4CPed5CRectf", Widget_DrawWeaponIcon_h, &Widget_DrawWeaponIcon_o);
}
