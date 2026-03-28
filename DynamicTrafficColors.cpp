
// Written by:
// 
// ███╗   ██╗ ██████╗  ██████╗██╗  ██╗ █████╗ ██╗      █████╗ 
// ████╗  ██║██╔═══██╗██╔════╝██║  ██║██╔══██╗██║     ██╔══██╗
// ██╔██╗ ██║██║   ██║██║     ███████║███████║██║     ███████║
// ██║╚██╗██║██║   ██║██║     ██╔══██║██╔══██║██║     ██╔══██║
// ██║ ╚████║╚██████╔╝╚██████╗██║  ██║██║  ██║███████╗██║  ██║
// ╚═╝  ╚═══╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
//
//          ░░▒▒▓▓ https://github.com/Nochala ▓▓▒▒░░

#include "script.h"
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#include <cctype>
#include <natives.h>

static std::string GetModuleDir()
{
    char path[MAX_PATH];
    path[0] = '\0';
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string s(path);
    size_t pos = s.find_last_of("\\/");
    if (pos == std::string::npos) return std::string();
    return s.substr(0, pos);
}

static std::string IniPath() { return GetModuleDir() + "\\DynamicTrafficColors.ini"; }

static int IniInt(const std::string& ini, const char* sec, const char* key, int defVal)
{
    return (int)GetPrivateProfileIntA(sec, key, defVal, ini.c_str());
}

static float IniFloat(const std::string& ini, const char* sec, const char* key, float defVal)
{
    char buf[64], defBuf[64];
    std::snprintf(defBuf, sizeof(defBuf), "%.3f", defVal);
    GetPrivateProfileStringA(sec, key, defBuf, buf, sizeof(buf), ini.c_str());
    return (float)std::atof(buf);
}

static bool IniBool(const std::string& ini, const char* sec, const char* key, bool defVal)
{
    char buf[64];
    buf[0] = ' ';
    GetPrivateProfileStringA(sec, key, defVal ? "true" : "false", buf, sizeof(buf), ini.c_str());

    std::string s(buf);
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) ++start;
    size_t end = s.size();
    while (end > start && std::isspace((unsigned char)s[end - 1])) --end;
    s = s.substr(start, end - start);

    for (size_t i = 0; i < s.size(); ++i)
        s[i] = (char)std::tolower((unsigned char)s[i]);

    if (s == "true") return true;
    if (s == "false") return false;
    return defVal;
}

static std::string IniString(const std::string& ini, const char* sec, const char* key, const char* defVal)
{
    char buf[128];
    buf[0] = '\0';
    GetPrivateProfileStringA(sec, key, defVal, buf, sizeof(buf), ini.c_str());
    return std::string(buf);
}

static std::string LogPathFromIni(const std::string& iniPath)
{
    char buf[MAX_PATH];
    buf[0] = '\0';
    GetPrivateProfileStringA("Logging", "LogFile", "DynamicTrafficColors.log", buf, MAX_PATH, iniPath.c_str());
    std::string name(buf);
    if (name.find(':') == std::string::npos && name.find('\\') == std::string::npos && name.find('/') == std::string::npos)
        return GetModuleDir() + "\\" + name;
    return name;
}

static DWORD GameTimeMs() { return (DWORD)GAMEPLAY::GET_GAME_TIMER(); }
static int RandInt(int minIncl, int maxExcl) { return (maxExcl <= minIncl) ? minIncl : GAMEPLAY::GET_RANDOM_INT_IN_RANGE(minIncl, maxExcl); }

static bool KeyJustUp(int vk)
{
    static SHORT prev[256] = {};
    if (vk < 0 || vk > 255) return false;
    SHORT now = GetAsyncKeyState(vk);
    bool wasDown = (prev[vk] & 0x8000) != 0;
    bool isDown = (now & 0x8000) != 0;
    prev[vk] = now;
    return wasDown && !isDown;
}

class Logger
{
public:
    Logger() : enabled_(false), handle_(INVALID_HANDLE_VALUE) {}
    ~Logger() { Close(); }

    void Configure(bool enabled, const std::string& path)
    {
        enabled_ = enabled;
        path_ = path;
        Close();
        if (!enabled_) return;
        handle_ = CreateFileA(path_.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (handle_ != INVALID_HANDLE_VALUE)
            WriteLine("[DynamicTrafficColors] Logging started.");
    }

    void Close()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    bool IsEnabled() const
    {
        return enabled_ && handle_ != INVALID_HANDLE_VALUE;
    }

    void WriteLine(const std::string& s)
    {
        if (!enabled_ || handle_ == INVALID_HANDLE_VALUE) return;
        std::string out = s + "";
        DWORD written = 0;
        WriteFile(handle_, out.c_str(), (DWORD)out.size(), &written, NULL);
    }

private:
    bool enabled_;
    std::string path_;
    HANDLE handle_;
};

struct Config
{
    bool Enabled;
    bool EnableColors;
    bool EnableLiveries;
    bool EnableDecals;
    bool EnableWheelColors;
    float Radius;
    float AircraftRadius;
    int UpdateIntervalMs;
    int RecentPerModel;
    int AircraftRecentPerModel;
    int GlobalRecentColorMemory;
    bool AffectOnlyTraffic;
    bool ParkedVehicleColors;
    bool SkipEmergency;
    bool SkipMissionVehicles;
    bool ServiceVehiclesKeepColors;
    bool RequireOffscreenApply;
    bool StrictSpawnOnly;
    int NewlySeenMaxAgeMs;
    bool RealisticVehicleColors;
    bool AllowBrightColors;
    bool TwoToneColors;
    bool RainbowMode;
    int ColorVarietyMultiplier;
    int NonRealisticRecentPrimaryMemory;
    int NonRealisticRecentSecondaryMemory;
    int NonRealisticOverusedColorPenalty;
    bool EnablePlanes;
    bool EnableHelicopters;
    bool AircraftKeepLiveries;
    int LiveryChancePercent;
    int DecalChancePercent;
    int AircraftLiveryChancePercent;
    int AircraftDecalChancePercent;
    bool ShowLoadNotification;
    std::string ReloadCheatCode;

    Config()
    {
        Enabled = true;
        EnableColors = true;
        EnableLiveries = true;
        EnableDecals = true;
        EnableWheelColors = true;
        Radius = 170.0f;
        AircraftRadius = 260.0f;
        UpdateIntervalMs = 160;
        RecentPerModel = 90;
        AircraftRecentPerModel = 50;
        GlobalRecentColorMemory = 140;
        AffectOnlyTraffic = true;
        ParkedVehicleColors = true;
        SkipEmergency = true;
        SkipMissionVehicles = true;
        ServiceVehiclesKeepColors = true;
        RequireOffscreenApply = true;
        StrictSpawnOnly = true;
        NewlySeenMaxAgeMs = 900;
        RealisticVehicleColors = true;
        AllowBrightColors = true;
        TwoToneColors = true;
        RainbowMode = false;
        ColorVarietyMultiplier = 240;
        NonRealisticRecentPrimaryMemory = 120;
        NonRealisticRecentSecondaryMemory = 90;
        NonRealisticOverusedColorPenalty = 7;
        EnablePlanes = true;
        EnableHelicopters = true;
        AircraftKeepLiveries = false;
        LiveryChancePercent = 10;
        DecalChancePercent = 14;
        AircraftLiveryChancePercent = 18;
        AircraftDecalChancePercent = 10;
        ShowLoadNotification = true;
        ReloadCheatCode = "DTCRELOAD";
    }
};

static Config gCfg;
static Logger gLog;

struct VehicleRuntimeInfo
{
    Hash model;
    Ped driver;
    Vector3 coords;
    bool hasCoords;
    bool hasDriver;
    bool hasPlayerDriver;
    bool isPlayerVehicle;
    bool isMissionEntity;
    bool isEmergency;
    bool driveable;
    bool isPlane;
    bool isHelicopter;
    bool isAircraft;
    bool onScreen;
    bool onScreenKnown;

    VehicleRuntimeInfo()
        : model(0), driver(0), hasCoords(false), hasDriver(false), hasPlayerDriver(false),
        isPlayerVehicle(false), isMissionEntity(false), isEmergency(false), driveable(false), isPlane(false),
        isHelicopter(false), isAircraft(false), onScreen(false), onScreenKnown(false)
    {
        ZeroMemory(&coords, sizeof(coords));
    }
};

struct VehicleEvalContext
{
    Ped playerPed;
    Vehicle playerVehicle;
    Vector3 playerPos;
    DWORD now;

    VehicleEvalContext()
        : playerPed(0), playerVehicle(0), now(0)
    {
        ZeroMemory(&playerPos, sizeof(playerPos));
    }
};

static const int kScanArraySize = 384;
static const int kVehiclesExaminedPerTick = 12;
static const int kMaxAppliesPerTick = 3;
static const int kRainbowVehiclesExaminedPerTick = 18;
static const int kRainbowMaxAppliesPerTick = 6;
static const int kSnapshotBuildVehiclesPerTick = 72;
static const int kRainbowSnapshotBuildVehiclesPerTick = 108;
static const size_t kSnapshotLowWatermark = 18;
static const DWORD kSnapshotRefreshIntervalMs = 1400;
static const DWORD kRainbowSnapshotRefreshIntervalMs = 900;
static const float kSnapshotRefreshMoveDistanceSq = 18.0f * 18.0f;
static const float kRainbowSnapshotRefreshMoveDistanceSq = 8.0f * 8.0f;
static const float kNearbyVehicleProtectRadiusSq = 55.0f * 55.0f;
static const float kNearbyAircraftProtectRadiusSq = 85.0f * 85.0f;
static const float kVisibleSpawnApplyMinDistanceSq = 45.0f * 45.0f;
static const float kVisibleParkedApplyMinDistanceSq = 65.0f * 65.0f;
static const DWORD kVisibleSpawnApplyMaxAgeMs = 320;
static const float kVisibleApplyPlayerSpeedMax = 4.5f;
static const int kRerollAttempts = 16;
static const DWORD kStartupWarmupMs = 3500;
static const DWORD kCleanupIntervalMs = 2500;
static const DWORD kSeenTtlMs = 18000;
static const int kVehicleModTypeLivery = 48;

static int MaxInt(int a, int b) { return (a > b) ? a : b; }
static int MinInt(int a, int b) { return (a < b) ? a : b; }
static int ClampPct(int v) { return (v < 0) ? 0 : ((v > 100) ? 100 : v); }

static Hash HashName(const char* name) { return GAMEPLAY::GET_HASH_KEY((char*)name); }

static float DistSq(const Vector3& a, const Vector3& b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
static bool IsWithinRadiusSq(const Vector3& a, const Vector3& b, float radius)
{
    return DistSq(a, b) <= (radius * radius);
}
static bool IsPlayerMovingTooFastForVisibleApply()
{
    Ped playerPed = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(playerPed))
        return false;

    const Vehicle playerVehicle = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);
    const Entity speedEntity = (playerVehicle != 0 && ENTITY::DOES_ENTITY_EXIST(playerVehicle)) ? (Entity)playerVehicle : (Entity)playerPed;
    return ENTITY::GET_ENTITY_SPEED(speedEntity) > kVisibleApplyPlayerSpeedMax;
}
static bool ChancePass(int pct)
{
    pct = ClampPct(pct);
    if (pct <= 0) return false;
    if (pct >= 100) return true;
    return RandInt(0, 100) < pct;
}

static bool IsPlayerVehicle(Vehicle v)
{
    Ped playerPed = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(playerPed)) return false;
    Vehicle pv = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);
    return pv != 0 && pv == v;
}
static bool IsEmergencyVehicle(Vehicle v) { return VEHICLE::GET_VEHICLE_CLASS(v) == 18; }
static bool IsMissionEntityVehicle(Vehicle v) { return ENTITY::IS_ENTITY_A_MISSION_ENTITY(v) != 0; }
static bool IsVehicleDriveableSafe(Vehicle v) { return VEHICLE::IS_VEHICLE_DRIVEABLE(v, false) != 0; }
static bool IsOnScreenSafe(Entity e) { return ENTITY::IS_ENTITY_ON_SCREEN(e) != 0; }
static bool IsPlane(Vehicle v) { return VEHICLE::GET_VEHICLE_CLASS(v) == 16; }
static bool IsHelicopter(Vehicle v) { return VEHICLE::GET_VEHICLE_CLASS(v) == 15; }
static bool IsAircraft(Vehicle v) { int vc = VEHICLE::GET_VEHICLE_CLASS(v); return vc == 15 || vc == 16; }

static bool IsValidVehicleHandle(Vehicle v)
{
    return v != 0 && ENTITY::DOES_ENTITY_EXIST(v) && ENTITY::IS_ENTITY_A_VEHICLE(v) != 0;
}

static VehicleEvalContext MakeVehicleEvalContext(const Vector3& playerPos, DWORD now)
{
    VehicleEvalContext ctx;
    ctx.playerPed = PLAYER::PLAYER_PED_ID();
    ctx.playerVehicle = (ctx.playerPed != 0 && ENTITY::DOES_ENTITY_EXIST(ctx.playerPed)) ? PED::GET_VEHICLE_PED_IS_IN(ctx.playerPed, false) : 0;
    ctx.playerPos = playerPos;
    ctx.now = now;
    return ctx;
}

static bool PopulateVehicleRuntimeInfo(Vehicle veh, const VehicleEvalContext& ctx, VehicleRuntimeInfo& outInfo, bool fetchCoords, bool fetchOnScreen)
{
    if (!IsValidVehicleHandle(veh)) return false;

    outInfo.model = ENTITY::GET_ENTITY_MODEL(veh);
    outInfo.driver = VEHICLE::GET_PED_IN_VEHICLE_SEAT(veh, -1);
    outInfo.hasDriver = outInfo.driver != 0 && ENTITY::DOES_ENTITY_EXIST(outInfo.driver);
    outInfo.hasPlayerDriver = outInfo.hasDriver && PED::IS_PED_A_PLAYER(outInfo.driver) != 0;
    outInfo.isPlayerVehicle = ctx.playerVehicle != 0 && ctx.playerVehicle == veh;
    outInfo.isMissionEntity = ENTITY::IS_ENTITY_A_MISSION_ENTITY(veh) != 0;
    outInfo.isEmergency = VEHICLE::GET_VEHICLE_CLASS(veh) == 18;
    outInfo.driveable = VEHICLE::IS_VEHICLE_DRIVEABLE(veh, false) != 0;

    const int vehicleClass = VEHICLE::GET_VEHICLE_CLASS(veh);
    outInfo.isPlane = vehicleClass == 16;
    outInfo.isHelicopter = vehicleClass == 15;
    outInfo.isAircraft = outInfo.isPlane || outInfo.isHelicopter;

    if (fetchCoords)
    {
        outInfo.coords = ENTITY::GET_ENTITY_COORDS(veh, true);
        outInfo.hasCoords = true;
    }
    else
    {
        ZeroMemory(&outInfo.coords, sizeof(outInfo.coords));
        outInfo.hasCoords = false;
    }

    if (fetchOnScreen)
    {
        outInfo.onScreen = ENTITY::IS_ENTITY_ON_SCREEN(veh) != 0;
        outInfo.onScreenKnown = true;
    }
    else
    {
        outInfo.onScreen = false;
        outInfo.onScreenKnown = false;
    }

    return true;
}

static Ped GetDriverSafe(Vehicle veh)
{
    if (!IsValidVehicleHandle(veh)) return 0;
    Ped driver = VEHICLE::GET_PED_IN_VEHICLE_SEAT(veh, -1);
    if (driver == 0 || !ENTITY::DOES_ENTITY_EXIST(driver)) return 0;
    return driver;
}

static bool HasAnyDriver(Vehicle veh)
{
    return GetDriverSafe(veh) != 0;
}

static bool HasPlayerDriver(Vehicle veh)
{
    Ped driver = GetDriverSafe(veh);
    return driver != 0 && PED::IS_PED_A_PLAYER(driver);
}

static bool IsLikelyPlayerOwnedOrPersistedVehicle(Vehicle veh)
{
    if (!IsValidVehicleHandle(veh)) return false;
    if (IsPlayerVehicle(veh)) return true;
    if (ENTITY::IS_ENTITY_A_MISSION_ENTITY(veh)) return true;
    return false;
}

static bool IsServiceVehicleModel(Hash model)
{
    static std::vector<Hash> models;
    if (models.empty())
    {
        const char* names[] = {
            "taxi","airbus","brickade","bus","coach","rentalbus","tourbus","trash","trash2","mule","mule2","mule3","mule4",
            "benson","boxville","boxville2","boxville3","boxville4","boxville5","packer","pounder","pounder2","stockade",
            "stockade3","tiptruck","tiptruck2","towtruck","towtruck2","flatbed","utillitruck","utillitruck2","utillitruck3",
            "docktug","ripley","airtug","tractor","tractor2","tractor3","forklift","mixer","mixer2","rubble"
        };
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) models.push_back(HashName(names[i]));
    }
    return std::find(models.begin(), models.end(), model) != models.end();
}

struct WeightedPalette
{
    std::vector<int> ultraCommon, common, uncommon, rare, bright;
    std::vector<int> neutrals, blues, reds, greens, warm, unusual;
};

static void AppendRange(std::vector<int>& out, const int* vals, size_t count) { out.insert(out.end(), vals, vals + count); }

static bool IsGloballyBlacklistedColor(int color)
{
    switch (color)
    {
    case 11:
    case 12:
    case 13:
    case 15:
    case 21:
    case 24:
    case 36:
    case 38:
    case 41:
    case 42:
    case 47:
    case 48:
    case 58:
    case 59:
    case 71:
    case 72:
    case 81:
    case 86:
    case 87:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 126:
    case 127:
    case 141:
    case 142:
    case 143:
    case 145:
    case 146:
    case 148:
    case 149:
    case 150:
    case 152:
    case 153:
    case 154:
    case 155:
    case 156:
    case 157:
    case 158:
    case 159:
    case 160:
        return true;
    default:
        return false;
    }
}

static bool VectorContainsColor(const std::vector<int>& values, int color)
{
    return std::find(values.begin(), values.end(), color) != values.end();
}

static bool IsRealisticRestrictedColor(int color)
{
    switch (color)
    {
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 40:
    case 43:
    case 44:
    case 46:
    case 52:
    case 53:
    case 56:
    case 57:
    case 63:
    case 64:
    case 67:
    case 68:
    case 69:
    case 70:
    case 73:
    case 74:
    case 75:
    case 76:
    case 77:
    case 78:
    case 79:
    case 80:
    case 82:
    case 83:
    case 84:
    case 85:
    case 88:
    case 89:
    case 90:
    case 91:
    case 92:
    case 94:
    case 97:
    case 98:
    case 99:
    case 100:
    case 101:
    case 102:
    case 103:
    case 104:
    case 105:
    case 107:
    case 108:
    case 109:
    case 110:
    case 112:
    case 117:
    case 125:
    case 128:
    case 129:
    case 130:
    case 135:
    case 147:
    case 151:
        return true;
    default:
        return false;
    }
}

static void RemoveBlacklistedColors(std::vector<int>& values)
{
    values.erase(std::remove_if(values.begin(), values.end(),
        [](int color) { return IsGloballyBlacklistedColor(color); }),
        values.end());
}

static void RemoveRealisticRestrictedColors(std::vector<int>& values)
{
    values.erase(std::remove_if(values.begin(), values.end(),
        [](int color) { return IsGloballyBlacklistedColor(color) || IsRealisticRestrictedColor(color); }),
        values.end());
}

static int PickFromVector(const std::vector<int>& values, int fallback)
{
    if (values.empty()) return fallback;
    return values[RandInt(0, (int)values.size())];
}

static WeightedPalette BuildRealisticPalette()
{
    WeightedPalette p;

    // Extremely conservative factory-style palette for realistic traffic.
    // Keep realistic mode focused on the colors that dominate normal road traffic:
    // black, gray, silver, white, off-white, a very small amount of deep blue,
    // and an even smaller amount of muted beige / tan.
    const int ultraCommonNeutrals[] = {
        0,1,2,3,4,5,6,7,8,9,10,17,18,19,22,23,25,111,112,121,122,131,132,134,156
    };
    const int commonNeutrals[] = {
        0,1,2,3,4,5,6,7,8,9,10,16,17,18,19,20,22,23,25,26,111,112,121,122,131,132,134,141,146,156
    };
    const int uncommonBlues[] = {
        61,62,66,69,75,76,82,84,141,146
    };
    const int rareEarthTones[] = {
        90,93,95,99,105,106,113,116,129,144,153,154
    };

    AppendRange(p.neutrals, ultraCommonNeutrals, sizeof(ultraCommonNeutrals) / sizeof(ultraCommonNeutrals[0]));
    AppendRange(p.neutrals, commonNeutrals, sizeof(commonNeutrals) / sizeof(commonNeutrals[0]));
    AppendRange(p.blues, uncommonBlues, sizeof(uncommonBlues) / sizeof(uncommonBlues[0]));
    AppendRange(p.warm, rareEarthTones, sizeof(rareEarthTones) / sizeof(rareEarthTones[0]));

    RemoveRealisticRestrictedColors(p.neutrals);
    RemoveRealisticRestrictedColors(p.blues);
    RemoveRealisticRestrictedColors(p.warm);

    for (int v : ultraCommonNeutrals)
    {
        if (IsGloballyBlacklistedColor(v) || IsRealisticRestrictedColor(v)) continue;
        for (int i = 0; i < 18; ++i) p.ultraCommon.push_back(v);
        for (int i = 0; i < 12; ++i) p.common.push_back(v);
        for (int i = 0; i < 3; ++i) p.uncommon.push_back(v);
    }

    for (int v : commonNeutrals)
    {
        if (IsGloballyBlacklistedColor(v) || IsRealisticRestrictedColor(v)) continue;
        for (int i = 0; i < 6; ++i) p.common.push_back(v);
        for (int i = 0; i < 2; ++i) p.uncommon.push_back(v);
    }

    for (int v : uncommonBlues)
    {
        if (IsGloballyBlacklistedColor(v) || IsRealisticRestrictedColor(v)) continue;
        for (int i = 0; i < 2; ++i) p.uncommon.push_back(v);
        p.rare.push_back(v);
    }

    for (int v : rareEarthTones)
    {
        if (IsGloballyBlacklistedColor(v) || IsRealisticRestrictedColor(v)) continue;
        p.rare.push_back(v);
    }

    RemoveRealisticRestrictedColors(p.ultraCommon);
    RemoveRealisticRestrictedColors(p.common);
    RemoveRealisticRestrictedColors(p.uncommon);
    RemoveRealisticRestrictedColors(p.rare);

    p.reds.clear();
    p.greens.clear();
    p.unusual.clear();
    p.bright.clear();
    return p;
}

static WeightedPalette BuildArcadePalette()
{
    WeightedPalette p;
    const int all[] = {
        0,1,2,3,4,5,11,12,13,15,21,24,27,28,29,30,31,32,33,34,35,36,38,39,40,41,42,43,44,46,47,
        49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,
        79,80,81,82,83,84,85,86,87,88,89,90,91,92,94,95,96,97,98,99,100,101,102,103,104,105,107,108,
        109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,
        132,133,134,135,136,137,138,139,140,141,142,143,145,146,147,148,149,150,151,152,153,154,155,
        156,157,158,159,160
    };
    const int bright[] = { 11,12,13,15,21,24,36,38,47,53,58,64,72,73,81,88,89,112,117,135,145,146,150,151,152,157,160 };
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
    {
        if (!IsGloballyBlacklistedColor(all[i]))
        {
            p.common.push_back(all[i]);
            p.uncommon.push_back(all[i]);
        }
    }
    for (size_t i = 0; i < sizeof(bright) / sizeof(bright[0]); ++i)
    {
        if (!IsGloballyBlacklistedColor(bright[i]))
        {
            p.rare.push_back(bright[i]);
            p.bright.push_back(bright[i]);
        }
    }
    return p;
}

static WeightedPalette gRealisticPalette = BuildRealisticPalette();
static WeightedPalette gArcadePalette = BuildArcadePalette();

struct BucketLookup
{
    int values[256];
    BucketLookup()
    {
        for (int i = 0; i < 256; ++i) values[i] = 6;
    }
};

static void FillBucketLookup(BucketLookup& lookup, const std::vector<int>& colors, int bucket)
{
    for (size_t i = 0; i < colors.size(); ++i)
    {
        const int color = colors[i];
        if (color >= 0 && color < 256)
            lookup.values[color] = bucket;
    }
}

static BucketLookup BuildBucketLookup(const WeightedPalette& p)
{
    BucketLookup lookup;
    FillBucketLookup(lookup, p.neutrals, 0);
    FillBucketLookup(lookup, p.blues, 1);
    FillBucketLookup(lookup, p.reds, 2);
    FillBucketLookup(lookup, p.greens, 3);
    FillBucketLookup(lookup, p.warm, 4);
    FillBucketLookup(lookup, p.unusual, 5);
    return lookup;
}

static BucketLookup gRealisticBucketLookup = BuildBucketLookup(gRealisticPalette);
static BucketLookup gArcadeBucketLookup = BuildBucketLookup(gArcadePalette);

static const std::vector<int>& PickColorPool(const WeightedPalette& p, bool allowBright)
{
    const int roll = RandInt(0, 100);
    if (allowBright && roll >= 97 && !p.bright.empty()) return p.bright;
    if (roll < 28 && !p.ultraCommon.empty()) return p.ultraCommon;
    if (roll < 66 && !p.common.empty()) return p.common;
    if (roll < 93 && !p.uncommon.empty()) return p.uncommon;
    if (!p.rare.empty()) return p.rare;
    if (!p.common.empty()) return p.common;
    return p.uncommon;
}

static int CountInRecent(const std::deque<int>& recent, int color)
{
    int c = 0;
    for (size_t i = 0; i < recent.size(); ++i) if (recent[i] == color) ++c;
    return c;
}

static bool RecentContains(const std::deque<int>& recent, int color)
{
    return std::find(recent.begin(), recent.end(), color) != recent.end();
}

static int BucketId(const WeightedPalette& p, int color)
{
    if (color < 0 || color >= 256) return 6;
    const BucketLookup& lookup = (&p == &gRealisticPalette) ? gRealisticBucketLookup : gArcadeBucketLookup;
    return lookup.values[color];
}

static int CountBucketInRecent(const WeightedPalette& p, const std::deque<int>& recent, int bucket)
{
    int c = 0;
    for (size_t i = 0; i < recent.size(); ++i) if (BucketId(p, recent[i]) == bucket) ++c;
    return c;
}

static int PickBucketedColor(const WeightedPalette& p, bool allowBright, int varietyMultiplier, const std::deque<int>& recentBuckets)
{
    const std::vector<int>& pool = PickColorPool(p, allowBright);
    if (pool.empty()) return 0;

    const bool realisticPalette = (&p == &gRealisticPalette);
    int bestColor = 0;
    int bestScore = -999999;
    bool found = false;

    for (int i = 0; i < MaxInt(kRerollAttempts, 30); ++i)
    {
        int color = pool[RandInt(0, (int)pool.size())];
        if (IsGloballyBlacklistedColor(color))
            continue;
        if (realisticPalette && IsRealisticRestrictedColor(color))
            continue;

        int bucket = BucketId(p, color);
        int bucketHits = CountBucketInRecent(p, recentBuckets, bucket);
        int score = RandInt(0, 1000) - bucketHits * varietyMultiplier;

        if (realisticPalette)
        {
            if (bucket == 0) score += 420;
            else if (bucket == 1) score -= 20;
            else if (bucket == 2) score -= 260;
            else if (bucket == 3) score -= 320;
            else if (bucket == 4) score -= 360;
            else score -= 420;
        }

        if (!found || score > bestScore)
        {
            bestScore = score;
            bestColor = color;
            found = true;
        }
    }

    if (found)
        return bestColor;

    for (size_t i = 0; i < pool.size(); ++i)
    {
        if (IsGloballyBlacklistedColor(pool[i]))
            continue;
        if (realisticPalette && IsRealisticRestrictedColor(pool[i]))
            continue;
        return pool[i];
    }

    return 0;
}

struct SeenInfo
{
    DWORD firstSeen;
    DWORD lastSeen;
    Hash model;
    bool applied;
    bool protectedNearby;
    SeenInfo() : firstSeen(0), lastSeen(0), model(0), applied(false), protectedNearby(false) {}
    SeenInfo(DWORD t, Hash m, bool protectNearby) : firstSeen(t), lastSeen(t), model(m), applied(false), protectedNearby(protectNearby) {}
};

static std::unordered_map<int, SeenInfo> gSeen;
static std::unordered_map<unsigned int, std::deque<int> > gRecentPrimaryByModel;
static std::unordered_map<unsigned int, std::deque<int> > gRecentSecondaryByModel;
static std::deque<int> gRecentGlobal;
static std::deque<int> gRecentBuckets;
static std::deque<int> gRecentCombos;
static std::unordered_map<int, DWORD> gCharacterSwitchProtectedVehicles;
static std::vector<int> gPoolSnapshot;
static std::vector<int> gSnapshotBuildInput;
static std::vector<int> gSnapshotBuildPriority;
static std::vector<int> gSnapshotBuildUrgentVisible;
static std::vector<int> gSnapshotBuildFallback;
static size_t gPoolCursor = 0;
static size_t gSnapshotBuildCursor = 0;
static Vector3 gLastSnapshotPlayerPos = { 0 };
static Vector3 gSnapshotBuildPlayerPos = { 0 };
static bool gHasLastSnapshotPlayerPos = false;
static bool gSnapshotBuildInProgress = false;
static DWORD gStartMs = 0;
static DWORD gLastCleanupMs = 0;
static DWORD gLastSnapshotMs = 0;
static Ped gLastObservedPlayerPed = 0;
static Vehicle gLastObservedPlayerVehicle = 0;
static std::string gCheatBuffer;
static DWORD gLastCheatKeyMs = 0;

static bool IsBlacklistedVehicleModel(Hash model)
{
    static const Hash kPoliceMaverickModel = HashName("polmav");
    return model == kPoliceMaverickModel;
}


static void ResetVehicleSnapshotState()
{
    gPoolSnapshot.clear();
    gSnapshotBuildInput.clear();
    gSnapshotBuildPriority.clear();
    gSnapshotBuildUrgentVisible.clear();
    gSnapshotBuildFallback.clear();
    gPoolCursor = 0;
    gSnapshotBuildCursor = 0;
    gSnapshotBuildInProgress = false;
    gHasLastSnapshotPlayerPos = false;
    gLastSnapshotMs = 0;
}

static void ProtectVehicleFromCharacterSwitch(Vehicle veh, DWORD now)
{
    if (!IsValidVehicleHandle(veh)) return;

    gCharacterSwitchProtectedVehicles[(int)veh] = now;

    SeenInfo& si = gSeen[(int)veh];
    const Hash model = ENTITY::GET_ENTITY_MODEL(veh);
    if (si.firstSeen == 0 || si.model != model)
        si.firstSeen = now;

    si.lastSeen = now;
    si.model = model;
    si.applied = true;
    si.protectedNearby = true;
}

static bool IsCharacterSwitchProtectedVehicle(Vehicle veh)
{
    return veh != 0 && gCharacterSwitchProtectedVehicles.find((int)veh) != gCharacterSwitchProtectedVehicles.end();
}

static void UpdateCharacterSwitchProtection()
{
    Ped playerPed = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(playerPed))
        return;

    const DWORD now = GameTimeMs();
    const Vehicle currentVehicle = PED::GET_VEHICLE_PED_IS_IN(playerPed, false);
    const bool switchedCharacter = gLastObservedPlayerPed != 0 && gLastObservedPlayerPed != playerPed;

    if (switchedCharacter)
    {
        ProtectVehicleFromCharacterSwitch(gLastObservedPlayerVehicle, now);
        ProtectVehicleFromCharacterSwitch(currentVehicle, now);
        ResetVehicleSnapshotState();

        if (gLog.IsEnabled())
        {
            std::ostringstream oss;
            oss << "Character switch detected; protecting previousVehicle=" << (int)gLastObservedPlayerVehicle
                << " currentVehicle=" << (int)currentVehicle;
            gLog.WriteLine(oss.str());
        }
    }

    if (currentVehicle != 0)
    {
        gLastObservedPlayerVehicle = currentVehicle;

        std::unordered_map<int, DWORD>::iterator it = gCharacterSwitchProtectedVehicles.find((int)currentVehicle);
        if (it != gCharacterSwitchProtectedVehicles.end())
            it->second = now;
    }
    else if (gLastObservedPlayerVehicle != 0 && !IsValidVehicleHandle(gLastObservedPlayerVehicle))
    {
        gLastObservedPlayerVehicle = 0;
    }

    gLastObservedPlayerPed = playerPed;
}

static bool ShouldProtectNearbyVehicle(const VehicleRuntimeInfo& info, const Vector3& playerPos)
{
    if (info.hasDriver || !info.hasCoords) return false;

    const float protectRadiusSq = info.isAircraft ? kNearbyAircraftProtectRadiusSq : kNearbyVehicleProtectRadiusSq;
    return DistSq(info.coords, playerPos) <= protectRadiusSq;
}

static void RegisterVehicleSeen(Vehicle veh, const VehicleRuntimeInfo& info, const Vector3& playerPos, DWORD now)
{
    SeenInfo& si = gSeen[(int)veh];
    if (si.firstSeen == 0 || si.model != info.model)
    {
        si.firstSeen = now;
        si.lastSeen = now;
        si.model = info.model;
        si.applied = false;
        si.protectedNearby = ShouldProtectNearbyVehicle(info, playerPos);
        return;
    }

    si.lastSeen = now;

    if (!si.applied && !si.protectedNearby && ShouldProtectNearbyVehicle(info, playerPos))
        si.protectedNearby = true;
}

static bool IsFreshVisibleSpawnCandidate(const VehicleRuntimeInfo& info, const SeenInfo& si, const Vector3& playerPos, DWORD now)
{
    if (info.hasPlayerDriver || !info.onScreenKnown || !info.onScreen || !info.hasCoords) return false;
    if (si.applied || si.protectedNearby) return false;
    if (IsPlayerMovingTooFastForVisibleApply()) return false;

    const DWORD visibleAgeLimit = (DWORD)MaxInt(120, MinInt(gCfg.NewlySeenMaxAgeMs, (int)kVisibleSpawnApplyMaxAgeMs));
    if ((now - si.firstSeen) > visibleAgeLimit)
        return false;

    const float minDistanceSq = info.hasDriver ? kVisibleSpawnApplyMinDistanceSq : kVisibleParkedApplyMinDistanceSq;
    return DistSq(info.coords, playerPos) >= minDistanceSq;
}

static bool ShouldQueueVehicleForProcessing(const VehicleRuntimeInfo& info, const SeenInfo& si, const Vector3& playerPos, DWORD now)
{
    if (gCfg.RainbowMode)
        return true;

    if (si.applied || si.protectedNearby)
        return false;

    if (gCfg.StrictSpawnOnly && (now - si.firstSeen) > (DWORD)MaxInt(0, gCfg.NewlySeenMaxAgeMs))
        return false;

    if (gCfg.RequireOffscreenApply && info.onScreenKnown && info.onScreen)
        return IsFreshVisibleSpawnCandidate(info, si, playerPos, now);

    return true;
}

static void PushLimited(std::deque<int>& dq, int value, size_t cap)
{
    dq.push_back(value);
    while (dq.size() > cap) dq.pop_front();
}

static int PackCombo(int primary, int secondary)
{
    return ((primary & 0xFF) << 8) | (secondary & 0xFF);
}

static bool RecentContainsCombo(const std::deque<int>& recent, int combo)
{
    return std::find(recent.begin(), recent.end(), combo) != recent.end();
}

static int CountComboInRecent(const std::deque<int>& recent, int combo)
{
    int c = 0;
    for (size_t i = 0; i < recent.size(); ++i)
        if (recent[i] == combo) ++c;
    return c;
}

static void ReserveWorkingSet()
{
    gSeen.reserve(4096);
    gRecentPrimaryByModel.reserve(512);
    gRecentSecondaryByModel.reserve(512);
    gPoolSnapshot.reserve(kScanArraySize);
    gSnapshotBuildInput.reserve(kScanArraySize);
    gSnapshotBuildPriority.reserve(kScanArraySize);
    gSnapshotBuildUrgentVisible.reserve(kScanArraySize);
    gSnapshotBuildFallback.reserve(kScanArraySize);
}

static void MarkUsed(Hash model, int primary, int secondary, bool aircraft)
{
    PushLimited(gRecentPrimaryByModel[(unsigned int)model], primary, aircraft ? (size_t)gCfg.AircraftRecentPerModel : (size_t)gCfg.RecentPerModel);
    PushLimited(gRecentSecondaryByModel[(unsigned int)model], secondary, aircraft ? (size_t)gCfg.AircraftRecentPerModel : (size_t)gCfg.RecentPerModel);
    PushLimited(gRecentGlobal, primary, (size_t)gCfg.GlobalRecentColorMemory);
    PushLimited(gRecentGlobal, secondary, (size_t)gCfg.GlobalRecentColorMemory);
    PushLimited(gRecentBuckets, primary, 140);
    PushLimited(gRecentBuckets, secondary, 140);
    PushLimited(gRecentCombos, PackCombo(primary, secondary), 180);
}

static void CleanupSeen()
{
    DWORD now = GameTimeMs();
    if (now - gLastCleanupMs < kCleanupIntervalMs) return;
    gLastCleanupMs = now;

    std::vector<int> dead;
    dead.reserve(gSeen.size());
    std::vector<int> deadProtected;
    deadProtected.reserve(gCharacterSwitchProtectedVehicles.size());
    for (std::unordered_map<int, DWORD>::iterator it = gCharacterSwitchProtectedVehicles.begin(); it != gCharacterSwitchProtectedVehicles.end(); ++it)
    {
        const int handle = it->first;
        if (!ENTITY::DOES_ENTITY_EXIST(handle) || ENTITY::IS_ENTITY_A_VEHICLE(handle) == 0)
            deadProtected.push_back(handle);
    }
    for (size_t i = 0; i < deadProtected.size(); ++i)
        gCharacterSwitchProtectedVehicles.erase(deadProtected[i]);

    if (gLastObservedPlayerVehicle != 0 && !IsValidVehicleHandle(gLastObservedPlayerVehicle))
        gLastObservedPlayerVehicle = 0;

    for (std::unordered_map<int, SeenInfo>::iterator it = gSeen.begin(); it != gSeen.end(); ++it)
    {
        int handle = it->first;
        const SeenInfo& si = it->second;
        if (!ENTITY::DOES_ENTITY_EXIST(handle) || ENTITY::IS_ENTITY_A_VEHICLE(handle) == 0)
        {
            dead.push_back(handle);
            continue;
        }

        if (!si.applied && (now - si.lastSeen) > kSeenTtlMs)
            dead.push_back(handle);
    }
    for (size_t i = 0; i < dead.size(); ++i) gSeen.erase(dead[i]);
}

static bool IsLikelyPlayerOwnedOrPersistedVehicle(const VehicleRuntimeInfo& info)
{
    return info.isPlayerVehicle || info.isMissionEntity;
}

static bool IsTrafficVehicle(const VehicleRuntimeInfo& info)
{
    if (info.hasDriver)
        return !info.hasPlayerDriver;

    if (!gCfg.ParkedVehicleColors) return false;
    if (IsLikelyPlayerOwnedOrPersistedVehicle(info)) return false;

    return true;
}

static bool AllowVehicleByFilters(Vehicle veh, const VehicleRuntimeInfo& info)
{
    if (!info.driveable) return false;
    if (IsCharacterSwitchProtectedVehicle(veh)) return false;
    if (IsLikelyPlayerOwnedOrPersistedVehicle(info)) return false;
    if (!IsTrafficVehicle(info)) return false;
    if (gCfg.SkipEmergency && info.isEmergency) return false;
    if (gCfg.SkipMissionVehicles && info.isMissionEntity) return false;
    if (IsBlacklistedVehicleModel(info.model)) return false;
    if (gCfg.ServiceVehiclesKeepColors && IsServiceVehicleModel(info.model)) return false;
    return true;
}

static const std::vector<int>& ColorsForBucket(const WeightedPalette& p, int bucket)
{
    switch (bucket)
    {
    case 0: return p.neutrals;
    case 1: return p.blues;
    case 2: return p.reds;
    case 3: return p.greens;
    case 4: return p.warm;
    case 5: return p.unusual;
    default: return p.neutrals;
    }
}

static int ScoreRealisticTwoTonePair(int primaryBucket, int secondaryBucket, bool sameColor)
{
    if (sameColor)
        return 900;

    if (secondaryBucket == 0)
        return 40;

    return -500;
}

static int PickRealisticSecondaryColor(const WeightedPalette& palette, int primary, const std::deque<int>& recentSecondary)
{
    const int primaryBucket = BucketId(palette, primary);

    // In realistic mode, almost every vehicle should remain single-color.
    if (RandInt(0, 100) < 98)
        return primary;

    int bestColor = primary;
    int bestScore = -999999;

    for (size_t i = 0; i < palette.neutrals.size(); ++i)
    {
        const int candidate = palette.neutrals[i];
        if (candidate == primary) continue;
        if (IsGloballyBlacklistedColor(candidate) || IsRealisticRestrictedColor(candidate)) continue;

        const int bucket = BucketId(palette, candidate);
        const int combo = PackCombo(primary, candidate);

        int score = RandInt(0, 40);
        score += ScoreRealisticTwoTonePair(primaryBucket, bucket, candidate == primary);
        score -= CountInRecent(recentSecondary, candidate) * MaxInt(6, gCfg.NonRealisticOverusedColorPenalty * 2);
        score -= CountInRecent(gRecentGlobal, candidate) * 5;
        score -= CountComboInRecent(gRecentCombos, combo) * 140;

        if (RecentContains(recentSecondary, candidate)) score -= 60;
        if (RecentContains(gRecentGlobal, candidate)) score -= 25;
        if (RecentContainsCombo(gRecentCombos, combo)) score -= 200;

        if (score > bestScore)
        {
            bestScore = score;
            bestColor = candidate;
        }
    }

    return bestColor;
}

static bool PickPrimarySecondary(Hash model, bool aircraft, int& outPrimary, int& outSecondary)
{
    const WeightedPalette& palette = (gCfg.RealisticVehicleColors && !gCfg.RainbowMode) ? gRealisticPalette : gArcadePalette;
    std::deque<int>& recentPrimary = gRecentPrimaryByModel[(unsigned int)model];
    std::deque<int>& recentSecondary = gRecentSecondaryByModel[(unsigned int)model];

    int bestP = 0, bestS = 0, bestScore = -999999;
    const int tries = gCfg.RainbowMode ? 32 : MaxInt(kRerollAttempts, 22);

    for (int i = 0; i < tries; ++i)
    {
        const bool allowBrightSelection = gCfg.RainbowMode || (!gCfg.RealisticVehicleColors && gCfg.AllowBrightColors);
        const int p = PickBucketedColor(palette, allowBrightSelection, gCfg.RainbowMode ? 420 : gCfg.ColorVarietyMultiplier, gRecentBuckets);

        int s = p;
        if (gCfg.TwoToneColors)
        {
            if (gCfg.RealisticVehicleColors && !gCfg.RainbowMode)
                s = PickRealisticSecondaryColor(palette, p, recentSecondary);
            else
                s = PickBucketedColor(palette, allowBrightSelection, gCfg.RainbowMode ? 420 : gCfg.ColorVarietyMultiplier, gRecentBuckets);
        }

        const int primaryBucket = BucketId(palette, p);
        const int secondaryBucket = BucketId(palette, s);
        const int combo = PackCombo(p, s);
        const bool sameColor = p == s;
        const bool preferContrast =
            gCfg.RainbowMode ||
            (gCfg.TwoToneColors && (!gCfg.RealisticVehicleColors ? (RandInt(0, 100) < 68) : false));

        int score = RandInt(0, 700);

        if (!gCfg.TwoToneColors)
        {
            if (sameColor) score += 120;
        }
        else if (gCfg.RealisticVehicleColors && !gCfg.RainbowMode)
        {
            if (sameColor) score += 520;
            else score -= 180;
        }
        else
        {
            if (sameColor) score -= 75;
            else score += 95;
        }

        if (preferContrast)
        {
            if (primaryBucket != secondaryBucket) score += (gCfg.RealisticVehicleColors && !gCfg.RainbowMode) ? 25 : 125;
            else score -= (gCfg.RealisticVehicleColors && !gCfg.RainbowMode) ? 20 : 70;
        }
        else
        {
            if (primaryBucket == secondaryBucket) score += 20;
        }

        if (gCfg.RealisticVehicleColors && !gCfg.RainbowMode)
        {
            if (primaryBucket == 0) score += 260;
            if (secondaryBucket == 0) score += 320;
            if (primaryBucket == 1) score += 6;
            if (secondaryBucket == 1) score -= 8;
            if (primaryBucket == 2) score -= 180;
            if (secondaryBucket == 2) score -= 240;
            if (primaryBucket == 3) score -= 260;
            if (secondaryBucket == 3) score -= 320;
            if (primaryBucket == 4) score -= 320;
            if (secondaryBucket == 4) score -= 380;
            if (primaryBucket >= 5) score -= 420;
            if (secondaryBucket >= 5) score -= 460;

            if (primaryBucket == 0 && secondaryBucket == 0) score += 180;
            if ((primaryBucket == 0 && secondaryBucket != 0) || (primaryBucket != 0 && secondaryBucket == 0)) score += 20;
            if (primaryBucket != 0 && secondaryBucket != 0 && primaryBucket != secondaryBucket) score -= 260;

            if (gCfg.TwoToneColors)
                score += ScoreRealisticTwoTonePair(primaryBucket, secondaryBucket, sameColor);
            else if (sameColor)
                score += 220;
        }

        score -= CountInRecent(recentPrimary, p) * MaxInt(8, gCfg.NonRealisticOverusedColorPenalty * 3);
        score -= CountInRecent(recentSecondary, s) * MaxInt(7, gCfg.NonRealisticOverusedColorPenalty * 2);
        score -= CountInRecent(gRecentGlobal, p) * 5;
        score -= CountInRecent(gRecentGlobal, s) * 4;
        score -= CountBucketInRecent(palette, gRecentBuckets, primaryBucket) * MaxInt(10, gCfg.ColorVarietyMultiplier / 5);
        score -= CountBucketInRecent(palette, gRecentBuckets, secondaryBucket) * MaxInt(8, gCfg.ColorVarietyMultiplier / 6);
        score -= CountComboInRecent(gRecentCombos, combo) * 95;

        if (RecentContainsCombo(gRecentCombos, combo)) score -= 180;
        if (RecentContains(recentPrimary, p)) score -= 65;
        if (RecentContains(recentSecondary, s)) score -= 45;
        if (RecentContains(gRecentGlobal, p)) score -= 25;
        if (RecentContains(gRecentGlobal, s)) score -= 18;

        if (!gCfg.RealisticVehicleColors || gCfg.RainbowMode)
        {
            if (RecentContains(recentPrimary, p)) score -= gCfg.NonRealisticRecentPrimaryMemory;
            if (RecentContains(recentSecondary, s)) score -= gCfg.NonRealisticRecentSecondaryMemory;
            if (gCfg.RainbowMode)
            {
                if (p != s) score += 70;
                if (primaryBucket != secondaryBucket) score += 100;
            }
        }

        if (score > bestScore)
        {
            bestScore = score;
            bestP = p;
            bestS = s;
        }
    }

    outPrimary = bestP;
    outSecondary = bestS;
    return true;
}


static bool TryApplyLivery(Vehicle veh, bool aircraft)
{
    if (!gCfg.EnableLiveries) return false;
    if (aircraft && gCfg.AircraftKeepLiveries) return false;

    int chance = aircraft ? gCfg.AircraftLiveryChancePercent : gCfg.LiveryChancePercent;
    if (!ChancePass(chance)) return false;

    VEHICLE::SET_VEHICLE_MOD_KIT(veh, 0);
    int count = VEHICLE::GET_NUM_VEHICLE_MODS(veh, kVehicleModTypeLivery);
    if (count > 0)
    {
        int idx = RandInt(0, count);
        VEHICLE::SET_VEHICLE_MOD(veh, kVehicleModTypeLivery, idx, false);
        return true;
    }

    int legacyCount = VEHICLE::GET_VEHICLE_LIVERY_COUNT(veh);
    if (legacyCount > 0)
    {
        int idx = RandInt(0, legacyCount);
        VEHICLE::SET_VEHICLE_LIVERY(veh, idx);
        return true;
    }
    return false;
}

static bool TryApplyDecal(Vehicle veh, bool aircraft)
{
    if (!gCfg.EnableDecals) return false;
    int chance = aircraft ? gCfg.AircraftDecalChancePercent : gCfg.DecalChancePercent;
    if (!ChancePass(chance)) return false;

    VEHICLE::SET_VEHICLE_MOD_KIT(veh, 0);
    const int modTypeRoofLivery = 10;
    int count = VEHICLE::GET_NUM_VEHICLE_MODS(veh, modTypeRoofLivery);
    if (count > 0)
    {
        int idx = RandInt(0, count);
        VEHICLE::SET_VEHICLE_MOD(veh, modTypeRoofLivery, idx, false);
        return true;
    }
    return false;
}

static void ApplyToVehicle(Vehicle veh, const VehicleRuntimeInfo& info)
{
    if (!AllowVehicleByFilters(veh, info)) return;
    const bool aircraft = info.isAircraft;

    int primary = 0, secondary = 0;
    const Hash model = info.model;
    bool changedColors = false;
    if (gCfg.EnableColors)
    {
        PickPrimarySecondary(model, aircraft, primary, secondary);
        VEHICLE::SET_VEHICLE_COLOURS(veh, primary, secondary);
        if (gCfg.EnableWheelColors)
        {
            int pearlescent = 0, wheelColor = 0;
            VEHICLE::GET_VEHICLE_EXTRA_COLOURS(veh, &pearlescent, &wheelColor);
            wheelColor = secondary;
            VEHICLE::SET_VEHICLE_EXTRA_COLOURS(veh, pearlescent, wheelColor);
        }
        MarkUsed(model, primary, secondary, aircraft);
        changedColors = true;
    }

    const bool appliedLivery = TryApplyLivery(veh, aircraft);
    const bool appliedDecal = TryApplyDecal(veh, aircraft);

    if (gLog.IsEnabled() && (changedColors || appliedLivery || appliedDecal))
    {
        std::ostringstream oss;
        oss << "Applied vehicle=" << (int)veh
            << " model=0x" << std::hex << (unsigned int)model << std::dec
            << " primary=" << primary
            << " secondary=" << secondary
            << " aircraft=" << (aircraft ? 1 : 0)
            << " livery=" << (appliedLivery ? 1 : 0)
            << " decal=" << (appliedDecal ? 1 : 0);
        gLog.WriteLine(oss.str());
    }

    SeenInfo& si = gSeen[(int)veh];
    si.applied = true;
    si.lastSeen = GameTimeMs();
    si.model = model;
}

static void ApplyToVehicle(Vehicle veh)
{
    Ped player = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(player)) return;

    const DWORD now = GameTimeMs();
    const Vector3 playerPos = ENTITY::GET_ENTITY_COORDS(player, true);
    const VehicleEvalContext ctx = MakeVehicleEvalContext(playerPos, now);

    VehicleRuntimeInfo info;
    if (!PopulateVehicleRuntimeInfo(veh, ctx, info, false, false)) return;
    ApplyToVehicle(veh, info);
}

static bool ShouldHandleCategory(const VehicleRuntimeInfo& info)
{
    if (info.isAircraft)
    {
        if (info.isPlane && !gCfg.EnablePlanes) return false;
        if (info.isHelicopter && !gCfg.EnableHelicopters) return false;
    }
    return true;
}

static size_t RemainingSnapshotVehicles()
{
    return (gPoolCursor < gPoolSnapshot.size()) ? (gPoolSnapshot.size() - gPoolCursor) : 0;
}

static void StartVehicleSnapshotBuild(const Vector3& playerPos, DWORD now)
{
    Vehicle pool[kScanArraySize];
    const int count = worldGetAllVehicles(pool, kScanArraySize);

    gSnapshotBuildInput.clear();
    gSnapshotBuildPriority.clear();
    gSnapshotBuildUrgentVisible.clear();
    gSnapshotBuildFallback.clear();
    gSnapshotBuildCursor = 0;

    if (count > 0)
        gSnapshotBuildInput.insert(gSnapshotBuildInput.end(), pool, pool + count);

    gSnapshotBuildPlayerPos = playerPos;
    gSnapshotBuildInProgress = true;
    gLastSnapshotMs = now;
    gLastSnapshotPlayerPos = playerPos;
    gHasLastSnapshotPlayerPos = true;
}

static void BuildVehicleSnapshotChunk()
{
    if (!gSnapshotBuildInProgress) return;

    const DWORD now = GameTimeMs();
    const VehicleEvalContext ctx = MakeVehicleEvalContext(gSnapshotBuildPlayerPos, now);
    const int maxScan = gCfg.RainbowMode ? kRainbowSnapshotBuildVehiclesPerTick : kSnapshotBuildVehiclesPerTick;
    int scanned = 0;

    while (gSnapshotBuildCursor < gSnapshotBuildInput.size() && scanned < maxScan)
    {
        Vehicle veh = (Vehicle)gSnapshotBuildInput[gSnapshotBuildCursor++];
        ++scanned;

        VehicleRuntimeInfo info;
        if (!PopulateVehicleRuntimeInfo(veh, ctx, info, true, true)) continue;
        if (!ShouldHandleCategory(info)) continue;
        if (!AllowVehicleByFilters(veh, info)) continue;

        const float radius = info.isAircraft ? gCfg.AircraftRadius : gCfg.Radius;
        if (!IsWithinRadiusSq(info.coords, gSnapshotBuildPlayerPos, radius)) continue;

        RegisterVehicleSeen(veh, info, gSnapshotBuildPlayerPos, now);

        SeenInfo& si = gSeen[(int)veh];
        if (!ShouldQueueVehicleForProcessing(info, si, gSnapshotBuildPlayerPos, now))
            continue;

        if (IsFreshVisibleSpawnCandidate(info, si, gSnapshotBuildPlayerPos, now))
            gSnapshotBuildUrgentVisible.push_back((int)veh);
        else
            gSnapshotBuildPriority.push_back((int)veh);
    }

    if (gSnapshotBuildCursor < gSnapshotBuildInput.size())
        return;

    gPoolSnapshot.clear();
    gPoolCursor = 0;
    gPoolSnapshot.insert(gPoolSnapshot.end(), gSnapshotBuildUrgentVisible.begin(), gSnapshotBuildUrgentVisible.end());
    gPoolSnapshot.insert(gPoolSnapshot.end(), gSnapshotBuildPriority.begin(), gSnapshotBuildPriority.end());

    gSnapshotBuildInput.clear();
    gSnapshotBuildPriority.clear();
    gSnapshotBuildUrgentVisible.clear();
    gSnapshotBuildFallback.clear();
    gSnapshotBuildCursor = 0;
    gSnapshotBuildInProgress = false;
    CleanupSeen();
}

static void RefreshVehicleSnapshot()
{
    const DWORD now = GameTimeMs();
    if (!gCfg.Enabled) return;
    if (now - gStartMs < kStartupWarmupMs) return;
    if (gSnapshotBuildInProgress) return;

    Ped player = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(player)) return;
    Vector3 ppos = ENTITY::GET_ENTITY_COORDS(player, true);

    const float moveDistanceSq = gCfg.RainbowMode ? kRainbowSnapshotRefreshMoveDistanceSq : kSnapshotRefreshMoveDistanceSq;
    const DWORD refreshIntervalMs = gCfg.RainbowMode ? kRainbowSnapshotRefreshIntervalMs : kSnapshotRefreshIntervalMs;
    const bool movedEnough = !gHasLastSnapshotPlayerPos || DistSq(ppos, gLastSnapshotPlayerPos) >= moveDistanceSq;
    const bool waitedLongEnough = (now - gLastSnapshotMs) >= refreshIntervalMs;
    const bool queueLow = RemainingSnapshotVehicles() <= kSnapshotLowWatermark;

    if (!movedEnough && !waitedLongEnough && !queueLow)
        return;

    StartVehicleSnapshotBuild(ppos, now);
}

static void ProcessVehicleSnapshot()
{
    if (!gCfg.Enabled) return;
    if (gPoolCursor >= gPoolSnapshot.size()) return;

    Ped player = PLAYER::PLAYER_PED_ID();
    if (!ENTITY::DOES_ENTITY_EXIST(player)) return;
    Vector3 ppos = ENTITY::GET_ENTITY_COORDS(player, true);
    const DWORD now = GameTimeMs();
    const VehicleEvalContext ctx = MakeVehicleEvalContext(ppos, now);
    const int maxExamined = gCfg.RainbowMode ? kRainbowVehiclesExaminedPerTick : kVehiclesExaminedPerTick;
    const int maxApplied = gCfg.RainbowMode ? kRainbowMaxAppliesPerTick : kMaxAppliesPerTick;

    int examined = 0;
    int applied = 0;
    while (gPoolCursor < gPoolSnapshot.size() && examined < maxExamined && applied < maxApplied)
    {
        Vehicle veh = (Vehicle)gPoolSnapshot[gPoolCursor++];
        ++examined;

        VehicleRuntimeInfo info;
        if (!PopulateVehicleRuntimeInfo(veh, ctx, info, true, true)) continue;
        if (!ShouldHandleCategory(info)) continue;
        if (!AllowVehicleByFilters(veh, info)) continue;

        SeenInfo& si = gSeen[(int)veh];
        if (!gCfg.RainbowMode)
        {
            if (si.applied || si.protectedNearby) continue;
            if (gCfg.StrictSpawnOnly && (now - si.firstSeen) > (DWORD)MaxInt(0, gCfg.NewlySeenMaxAgeMs)) continue;
        }

        const float radius = info.isAircraft ? gCfg.AircraftRadius : gCfg.Radius;
        if (!IsWithinRadiusSq(info.coords, ppos, radius)) continue;

        RegisterVehicleSeen(veh, info, ppos, now);

        if (!ShouldQueueVehicleForProcessing(info, si, ppos, now))
            continue;

        ApplyToVehicle(veh, info);
        ++applied;
    }

    if (gPoolCursor >= gPoolSnapshot.size())
    {
        gPoolSnapshot.clear();
        gPoolCursor = 0;
    }
}

static void ScanAndApply()
{
    UpdateCharacterSwitchProtection();
    RefreshVehicleSnapshot();
    BuildVehicleSnapshotChunk();
    ProcessVehicleSnapshot();
}

static void NotifyFeed(const std::string& msg)
{
    UI::_SET_NOTIFICATION_TEXT_ENTRY("STRING");
    UI::_ADD_TEXT_COMPONENT_STRING((char*)msg.c_str());
    UI::_DRAW_NOTIFICATION(false, false);
}

static char VkToCharSimple(int vk)
{
    if (vk >= 'A' && vk <= 'Z') return (char)vk;
    if (vk >= '0' && vk <= '9') return (char)vk;
    return 0;
}

static std::string ToUpperAscii(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = (char)std::toupper((unsigned char)out[i]);
    return out;
}

static bool UpdateReloadCheatCode()
{
    const DWORD now = GameTimeMs();
    if (now - gLastCheatKeyMs > 2500)
        gCheatBuffer.clear();

    for (int vk = '0'; vk <= 'Z'; ++vk)
    {
        if (!KeyJustUp(vk))
            continue;

        char c = VkToCharSimple(vk);
        if (!c)
            continue;

        gLastCheatKeyMs = now;
        if ((int)gCheatBuffer.size() >= 32)
            gCheatBuffer.erase(gCheatBuffer.begin());
        gCheatBuffer.push_back((char)std::toupper((unsigned char)c));

        const std::string target = ToUpperAscii(gCfg.ReloadCheatCode);
        if (!target.empty() && gCheatBuffer.size() >= target.size())
        {
            if (gCheatBuffer.compare(gCheatBuffer.size() - target.size(), target.size(), target) == 0)
            {
                gCheatBuffer.clear();
                return true;
            }
        }
    }

    return false;
}

static void LoadConfig()
{
    const std::string ini = IniPath();
    gCfg.Enabled = IniBool(ini, "General", "Enabled", true);
    gCfg.EnableColors = IniBool(ini, "General", "EnableColors", true);
    gCfg.EnableLiveries = IniBool(ini, "General", "EnableLiveries", true);
    gCfg.EnableDecals = IniBool(ini, "General", "EnableDecals", true);
    gCfg.EnableWheelColors = IniBool(ini, "General", "EnableWheelColors", true);
    gCfg.Radius = IniFloat(ini, "General", "Radius", 170.0f);
    gCfg.AircraftRadius = IniFloat(ini, "General", "AircraftRadius", 260.0f);
    gCfg.UpdateIntervalMs = IniInt(ini, "General", "UpdateIntervalMs", 160);

    gCfg.RecentPerModel = IniInt(ini, "Variety", "RecentPerModel", 90);
    gCfg.AircraftRecentPerModel = IniInt(ini, "Variety", "AircraftRecentPerModel", 50);
    gCfg.GlobalRecentColorMemory = IniInt(ini, "Variety", "GlobalRecentColorMemory", 140);
    gCfg.ColorVarietyMultiplier = IniInt(ini, "Variety", "ColorVarietyMultiplier", 240);

    gCfg.AffectOnlyTraffic = IniBool(ini, "Filters", "AffectOnlyTraffic", true);
    gCfg.ParkedVehicleColors = IniBool(ini, "Filters", "ParkedVehicleColors", true);
    gCfg.SkipEmergency = IniBool(ini, "Filters", "SkipEmergency", true);
    gCfg.SkipMissionVehicles = IniBool(ini, "Filters", "SkipMissionVehicles", true);
    gCfg.ServiceVehiclesKeepColors = IniBool(ini, "Filters", "ServiceVehiclesKeepColors", true);
    gCfg.RequireOffscreenApply = IniBool(ini, "Filters", "RequireOffscreenApply", true);
    gCfg.StrictSpawnOnly = IniBool(ini, "Filters", "StrictSpawnOnly", true);
    gCfg.NewlySeenMaxAgeMs = IniInt(ini, "Filters", "NewlySeenMaxAgeMs", 900);

    gCfg.RealisticVehicleColors = IniBool(ini, "Colors", "RealisticVehicleColors", true);
    gCfg.AllowBrightColors = IniBool(ini, "Colors", "AllowBrightColors", true);
    gCfg.TwoToneColors = IniBool(ini, "Colors", "TwoToneColors", true);
    gCfg.RainbowMode = IniBool(ini, "Colors", "RainbowMode", false);
    gCfg.NonRealisticOverusedColorPenalty = IniInt(ini, "Colors", "NonRealisticOverusedColorPenalty", 7);
    gCfg.NonRealisticRecentPrimaryMemory = IniInt(ini, "Colors", "NonRealisticRecentPrimaryMemory", 120);
    gCfg.NonRealisticRecentSecondaryMemory = IniInt(ini, "Colors", "NonRealisticRecentSecondaryMemory", 90);

    gCfg.EnablePlanes = IniBool(ini, "Aircraft", "EnablePlanes", true);
    gCfg.EnableHelicopters = IniBool(ini, "Aircraft", "EnableHelicopters", true);
    gCfg.AircraftKeepLiveries = IniBool(ini, "Aircraft", "AircraftKeepLiveries", false);

    gCfg.LiveryChancePercent = IniInt(ini, "Liveries", "LiveryChancePercent", 10);
    gCfg.DecalChancePercent = IniInt(ini, "Liveries", "DecalChancePercent", 14);
    gCfg.AircraftLiveryChancePercent = IniInt(ini, "Liveries", "AircraftLiveryChancePercent", 18);
    gCfg.AircraftDecalChancePercent = IniInt(ini, "Liveries", "AircraftDecalChancePercent", 10);

    gCfg.ShowLoadNotification = IniBool(ini, "UI", "ShowLoadNotification", true);
    gCfg.ReloadCheatCode = IniString(ini, "UI", "ReloadCheatCode", "DTCRELOAD");

    bool logEnabled = IniBool(ini, "Logging", "EnableLog", false);
    const std::string logPath = LogPathFromIni(ini);
    gLog.Configure(logEnabled, logPath);

    if (gLog.IsEnabled())
    {
        std::ostringstream oss;
        oss << "Config loaded: Enabled=" << (gCfg.Enabled ? 1 : 0)
            << " RealisticVehicleColors=" << (gCfg.RealisticVehicleColors ? 1 : 0)
            << " TwoToneColors=" << (gCfg.TwoToneColors ? 1 : 0)
            << " RainbowMode=" << (gCfg.RainbowMode ? 1 : 0)
            << " Radius=" << gCfg.Radius
            << " AircraftRadius=" << gCfg.AircraftRadius
            << " UpdateIntervalMs=" << gCfg.UpdateIntervalMs
            << " ParkedVehicleColors=" << (gCfg.ParkedVehicleColors ? 1 : 0)
            << " LogFile=" << logPath;
        gLog.WriteLine(oss.str());
    }
}

void ScriptMain()
{
    gStartMs = GameTimeMs();
    ReserveWorkingSet();
    LoadConfig();
    if (gCfg.ShowLoadNotification)
        NotifyFeed("~b~~h~Dynamic Traffic Colors~s~\nLoaded successfully.");

    DWORD lastUpdate = 0;
    while (true)
    {
        const DWORD now = GameTimeMs();

        if (UpdateReloadCheatCode())
        {
            LoadConfig();
            if (gCfg.ShowLoadNotification)
                NotifyFeed("~b~~h~Dynamic Traffic Colors~s~\nINI reloaded.");
        }

        if (now - lastUpdate >= (DWORD)MaxInt(20, gCfg.UpdateIntervalMs))
        {
            lastUpdate = now;
            ScanAndApply();
        }

        WAIT(0);
    }
}

