#pragma once
// ID senjata SA -> nama texture ikon (sesuai gta6hud_icons.txd). nullptr = tidak ada ikon.
static const char* WeaponIconName(int id) {
    switch (id) {
        case 0:  return "unarmedicon";   case 1:  return "brassknuckleicon"; case 2:  return "golfclubicon";
        case 3:  return "nitestickicon"; case 4:  return "knifecuricon";     case 5:  return "baticon";
        case 6:  return "shovelicon";    case 7:  return "poolcueicon";      case 8:  return "katanaicon";
        case 9:  return "chnsawicon";    case 10: return "gun_dildo1icon";   case 11: return "gun_dildo2icon";
        case 12: return "gun_vibe1icon"; case 14: return "floweraicon";      case 15: return "gun_caneicon";
        case 16: return "grenadeicon";   case 17: return "teargasicon";      case 18: return "molotovicon";
        case 22: return "colt45icon";    case 23: return "silencedicon";     case 24: return "desert_eagleicon";
        case 25: return "chromegunicon"; case 26: return "sawnofficon";      case 27: return "shotgspaicon";
        case 28: return "micro_uziicon"; case 29: return "mp5lngicon";       case 30: return "ak47icon";
        case 31: return "m4icon";        case 32: return "tec9icon";         case 33: return "cuntgunicon";
        case 34: return "snipericon";    case 35: return "rocketlaicon";     case 36: return "heatseekicon";
        case 37: return "flameicon";     case 38: return "minigunicon";      case 39: return "satchelicon";
        case 40: return "bombicon";      case 41: return "spraycanicon";     case 42: return "fire_exicon";
        case 43: return "cameraicon";    case 44: return "nvgogglesicon";    case 45: return "irgogglesicon";
        case 46: return "gun_paraicon";
        default: return nullptr;
    }
}
