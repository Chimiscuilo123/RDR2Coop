#pragma once
#include <cstdint>

// Weather lock globals — writable by plugins to override server weather.
// When g_weatherLocked is true, the core forces g_weatherTarget instead of
// the server's weather. Plugins set these via GetProcAddress from the ASI.

#ifdef WEATHER_EXPORTS
#define WEATHER_API __declspec(dllexport)
#else
#define WEATHER_API __declspec(dllimport)
#endif

extern "C" {
    WEATHER_API extern bool     g_weatherLocked;
    WEATHER_API extern uint32_t g_weatherTarget;
    WEATHER_API extern uint32_t g_lastWeather;
}
