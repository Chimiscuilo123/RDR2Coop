#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

// Plugin function signatures
typedef void (*PluginInitFn)(int localPed, const char* username, bool* weatherLocked, uint32_t* weatherTarget, uint32_t* lastWeather);
typedef void (*PluginTickFn)(int localPed);
typedef void (*PluginChatCmdFn)(const char* cmd, const char* args);
typedef const char* (*PluginCommandsFn)();  // returns comma-separated command list
typedef void (*PluginShutdownFn)();

struct LoadedPlugin
{
    HMODULE handle = NULL;
    PluginInitFn     init     = nullptr;
    PluginTickFn     tick     = nullptr;
    PluginChatCmdFn  chatCmd  = nullptr;
    PluginCommandsFn commands = nullptr;
    PluginShutdownFn shutdown = nullptr;
    std::string name;
    std::string cmdList;  // cached command list
};

class PluginSystem
{
public:
    std::vector<LoadedPlugin> plugins;

    // Script callbacks (event-based scripting without Lua)
    struct ScriptEvents
    {
        std::function<void(int playerId, const char* username)> onPlayerConnect;
        std::function<void(int playerId, const char* username)> onPlayerDisconnect;
        std::function<void(int attackerId, int victimId, const char* type)> onSyncEvent;
        std::function<void()> onTick;
        std::function<void(const char* cmd, const char* args)> onChatCommand;
    };
    ScriptEvents events;

    // Scan plugins/ folder and load all .dll files
    void LoadAll(const std::string& pluginDir, int localPed, const char* username,
                 bool* weatherLocked, uint32_t* weatherTarget, uint32_t* lastWeather)
    {
        std::string search = pluginDir + "*.dll";
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search.c_str(), &fd);
        if (hFind == INVALID_HANDLE_VALUE) return;

        do {
            std::string path = pluginDir + fd.cFileName;
            LoadSingle(path, localPed, username, weatherLocked, weatherTarget, lastWeather);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }

    // Tick all loaded plugins
    void TickAll(int localPed)
    {
        if (events.onTick) events.onTick();
        for (auto& p : plugins)
            if (p.tick) p.tick(localPed);
    }

    // Forward chat commands to all plugins
    void DispatchChat(const char* cmd, const char* args)
    {
        for (auto& p : plugins)
            if (p.chatCmd) p.chatCmd(cmd, args);
    }

    // Build comma-separated list of all plugin commands for the server
    std::string GetAllCommands()
    {
        std::string all;
        for (auto& p : plugins)
        {
            if (!p.cmdList.empty())
            {
                if (!all.empty()) all += ",";
                all += p.cmdList;
            }
        }
        return all;
    }

    // Unload all plugins
    void UnloadAll()
    {
        for (auto& p : plugins)
        {
            if (p.shutdown) p.shutdown();
            if (p.handle) FreeLibrary(p.handle);
        }
        plugins.clear();
    }

private:
    void LoadSingle(const std::string& path, int localPed, const char* username,
                    bool* wl, uint32_t* wt, uint32_t* lw)
    {
        HMODULE h = LoadLibraryA(path.c_str());
        if (!h) return;

        auto init = (PluginInitFn)GetProcAddress(h, "PluginInit");
        auto tick = (PluginTickFn)GetProcAddress(h, "PluginTick");
        auto chat = (PluginChatCmdFn)GetProcAddress(h, "PluginChatCommand");
        auto cmds = (PluginCommandsFn)GetProcAddress(h, "PluginCommands");
        auto sd   = (PluginShutdownFn)GetProcAddress(h, "PluginShutdown");

        if (!init && !tick && !chat && !cmds && !sd) { FreeLibrary(h); return; }

        LoadedPlugin p;
        p.handle   = h;
        p.init     = init;
        p.tick     = tick;
        p.chatCmd  = chat;
        p.commands = cmds;
        p.shutdown = sd;
        p.name     = path.substr(path.rfind('\\') + 1);
        p.cmdList  = cmds ? cmds() : "";

        if (p.init) p.init(localPed, username, wl, wt, lw);
        plugins.push_back(p);
    }
};