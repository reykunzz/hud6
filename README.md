# GTA6 HUD - GTA SA Android (arm64) - Fase 2

Plugin AML untuk GTA SA 2.10 64-bit.

Fase 2: ikon senjata + ammo dari TXD game, bar health/armor kustom, toggle elemen HUD sesuai `GTA6HUD.ini`.
Belum ada: radar GTA6, weapon wheel, GPS, race HUD, uang kustom (uang & wanted tetap bawaan game).

## Build (GitHub)
Dari Termux, di folder proyek:

    git init && git add . && git commit -m "gta6hud fase 2"
    git branch -M main
    git remote add origin https://github.com/USERNAME/REPO.git
    git push -u origin main

Lalu buka tab Actions > Build GTA6HUD.so > unduh artifact `GTA6HUD`.
Jika build gagal, salin log langkah yang merah dan kirim ke Claude.

## Pasang
1. `libGTA6HUD.so` -> folder mod AML.
2. `GTA6HUD.ini` -> `Android/data/<package game>/files/GTA6HUD/`.
3. Texture ikon: lihat bagian TXD.

## TXD
Plugin memuat ikon dengan `CSprite2d::SetTexture(nama)` dari txd slot **"hud"** (MODELS/HUD.TXD di IMG).
Nama texture harus persis seperti di `gta6hud_icons.txd` (mis. `ak47icon`, `colt45icon`, `unarmedicon`).
- Ekspor/lihat isi TXD PC: `python3 tools/txdtool.py list gta6hud_icons.txd`
- TXD PC (D3D9) TIDAK bisa dipakai langsung di Android: konversi ke format Android (mis. dengan Magic.TXD,
  format DXT/ETC/PVR sesuai GPU), lalu gabungkan ke HUD.TXD dan impor ke IMG.
- Jika sebuah texture tidak ada, plugin otomatis memakai ikon asli game (tidak crash). Cek di log baris
  `ikon <nama> ditemukan` / `TIDAK ada di HUD.TXD`.

## Pengaman
- `Android/data/<package>/files/GTA6HUD/.initlock` dibuat saat game mulai dan dihapus setelah 300 frame aman.
  Jika game crash sebelum itu, peluncuran berikutnya masuk mode aman (hanya toggle HUD, tanpa gambar kustom).
  Hapus file itu untuk mencoba lagi.
- Semua nilai yang dibaca dari memori game (health, armor, senjata, ammo) dicek kewajarannya; kalau aneh -> HUD asli.

## Log
    adb logcat | grep GTA6HUD
Atau lihat log AML. Kirim log jika ada masalah. Yang perlu dicek: semua "hook OK", ukuran layar benar,
nilai armor dan ammo terbaca wajar (offset armor dan ammo masih dugaan).
