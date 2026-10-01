#pragma once
// WeatherControl — built-in weather/time lock script using PluginSystem::ScriptEvents.
// Usage: /wlock <weather>  /wunlock  /wlist
//        /tlock <h> <m>   /tunlock
#include <cstdint>
#include <map>
#include <string>
#include <cstring>
#include <cstdio>

struct WeatherControl
{
    bool     weatherLocked = false;
    uint32_t targetHash    = 0;
    bool     timeLocked    = false;
    int      timeHour      = 12;
    int      timeMinute    = 0;
    std::map<std::string, uint32_t> weatherMap;

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

    void Init()
    {
        const char* names[] = {
            "clear","extrasunny","sunny","overcast","rain","drizzle",
            "thunder","thunderstorm","fog","misty","snow","snowlight",
            "blizzard","groundblizzard","hurricane","highpressure",
            "sandstorm","smog","clouds"
        };
        for (auto n : names)
            weatherMap[n] = JoaatLower(n);
    }

    void OnChatCommand(const char* cmd, const char* args)
    {
        if (_stricmp(cmd, "wlock") == 0)
        {
            if (!args || !args[0]) return;
            auto it = weatherMap.find(args);
            if (it != weatherMap.end())
            {
                weatherLocked = true;
                targetHash = it->second;
            }
        }
        else if (_stricmp(cmd, "wunlock") == 0)
        {
            weatherLocked = false;
            targetHash = 0;
        }
        else if (_stricmp(cmd, "wlist") == 0) { /* handled by chat */ }
        else if (_stricmp(cmd, "tlock") == 0)
        {
            if (!args || !args[0]) return;
            int h = 12, m = 0;
            char buf[32]; strncpy(buf, args, 31); buf[31]=0;
            char* space = strchr(buf, ' ');
            if (space) { *space=0; m = atoi(space+1); }
            h = atoi(buf);
            if (h >= 0 && h <= 23 && m >= 0 && m <= 59)
            {
                timeLocked = true;
                timeHour = h;
                timeMinute = m;
            }
        }
        else if (_stricmp(cmd, "tunlock") == 0)
        {
            timeLocked = false;
        }
    }

    void OnTick() {}
};
