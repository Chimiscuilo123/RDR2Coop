// plugins/example/example.cpp
// Example plugin for RDR2Coop

#include <windows.h>
#include <cstdio>

static FILE* g_log = nullptr;
static int g_localPed = -1;

extern "C" __declspec(dllexport) void PluginInit(int localPed, const char* username)
{
    g_localPed = localPed;
    g_log = fopen("RDR2Coop/logs/plugin_example.log", "w");
    fprintf(g_log, "Example plugin loaded by %s\n", username);
    fflush(g_log);
}

extern "C" __declspec(dllexport) void PluginTick(int localPed)
{
    // Called every frame - can access RDR2 natives
    // via ScriptHookRDR2 invoke()
    g_localPed = localPed;
}

extern "C" __declspec(dllexport) void PluginShutdown()
{
    if (g_log) { fprintf(g_log, "Plugin shutdown\n"); fclose(g_log); g_log = nullptr; }
}

// Compile: g++ -std=c++17 -shared -o example.dll example.cpp -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread -Wl,-Bdynamic -Wl,--subsystem,windows -O2
