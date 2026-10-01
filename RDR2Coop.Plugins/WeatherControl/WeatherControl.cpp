#define NOMINMAX
#include "script.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

// Weather globals (passed by core via PluginInit)
static bool*     g_weatherLocked  = nullptr;
static uint32_t* g_weatherTarget = nullptr;
static uint32_t* g_lastWeather   = nullptr;

// Weather name → JOAAT hash (lowercase, matches CustomizableTrainer)
static std::map<std::string, uint32_t> g_weatherMap;

static uint32_t JoaatLower(const char* s)
{
    uint32_t hash = 0;
    for (; *s; s++) {
        char c = *s;
        if (c >= 'A' && c <= 'Z') c += 32;
        hash += (uint8_t)c;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static void InitWeatherMap()
{
    const char* names[] = {
        "clear","extrasunny","sunny","overcast","rain","drizzle",
        "thunder","thunderstorm","fog","misty","snow","snowlight",
        "blizzard","groundblizzard","hurricane","highpressure",
        "sandstorm","smog","clouds"
    };
    for (auto n : names)
        g_weatherMap[n] = JoaatLower(n);
}

extern "C" __declspec(dllexport) void PluginInit(
    int localPed, const char* username,
    bool* weatherLocked, uint32_t* weatherTarget, uint32_t* lastWeather)
{
    g_weatherLocked  = weatherLocked;
    g_weatherTarget = weatherTarget;
    g_lastWeather   = lastWeather;
    InitWeatherMap();
}

extern "C" __declspec(dllexport) void PluginTick(int localPed)
{
    // Weather is handled by the core's smart persistence system
    // (weather_control.h + main loop _SET_WEATHER_TYPE_TRANSITION).
    // This plugin just provides the lock variables and user commands.
}

extern "C" __declspec(dllexport) void PluginShutdown()
{
    if (g_weatherLocked) *g_weatherLocked = false;
}
