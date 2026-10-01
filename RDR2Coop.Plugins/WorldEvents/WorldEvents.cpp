#define NOMINMAX
#include "script.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>

// RVector3 (matches ScriptHookRDR2 layout: float x,y,z, padding)
#pragma pack(push, 1)
struct RVector3 { float x, y, z, _pad; };
#pragma pack(pop)

// ── Config ──────────────────────────────────────────────────────────────────
static constexpr int   ANIMAL_SCAN_RADIUS  = 80;   // meters
static constexpr int   ANIMAL_MAX_DENSITY  = 10;   // max animals per player area
static constexpr int   SCAN_INTERVAL       = 300;  // frames (~5s)
static constexpr int   AMBUSH_COOLDOWN     = 1800; // frames (~30s) between ambushes
static constexpr float AMBUSH_CHANCE       = 0.15f;// 15% per scan

// ── Animal models by biome ───────────────────────────────────────────────────
static const uint32_t ANIMALS_PLAINS[] = {
    0xD86C3B0A, // a_c_deer_01
    0x18E0680A, // a_c_elk_01
    0xAA6FF2C6, // a_c_pronghorn_01
    0x4E5E8B8C, // a_c_buck_01
    0xA5C5B0F9, // a_c_rabbit_01
    0x0E0B9E6D, // a_c_raccoon_01
    0xFB8C71A1, // a_c_badger_01
    0x876D37B3, // a_c_coyote_01
    0xC02DF42D, // a_c_wolf_01
};
static const uint32_t ANIMALS_FOREST[] = {
    0x058C10B3, // a_c_blackbear_01
    0xA6BD2C2C, // a_c_grizzly_01
    0xD86C3B0A, // a_c_deer_01
    0xAA6FF2C6, // a_c_pronghorn_01
    0x0DFB13C8, // a_c_boar_01
    0xFB8C71A1, // a_c_badger_01
    0x27D600A3, // a_c_skunk_01
    0xC02DF42D, // a_c_wolf_01
};
static const uint32_t ANIMALS_SWAMP[] = {
    0x3BD14DC3, // a_c_alligator_01
    0x4BEE5640, // a_c_alligator_02
    0xDFB13C78, // a_c_snake_01
    0x6FD03F46, // a_c_turtle_01
    0x29B109D1, // a_c_heron_01
    0x0E0B9E6D, // a_c_raccoon_01
};
static const uint32_t ANIMALS_DESERT[] = {
    0xDFF3B7F0, // a_c_bighornram_01
    0x54CBB85F, // a_c_vulture_01
    0x876D37B3, // a_c_coyote_01
    0xFB8C71A1, // a_c_badger_01
    0xCE31F3DC, // a_c_eagle_01
    0xC62CD222, // a_c_armadillo_01
};
static const uint32_t ANIMALS_BIRDS[] = {
    0x54CBB85F, // a_c_vulture_01
    0xCE31F3DC, // a_c_eagle_01
    0x9CE20DB9, // a_c_hawk_01
    0x5FC007C8, // a_c_crow_01
    0x29B109D1, // a_c_heron_01
    0x0D0DFFA9, // a_c_duck_01
};
static constexpr int ANIMALS_PLAINS_COUNT = 9;
static constexpr int ANIMALS_FOREST_COUNT = 8;
static constexpr int ANIMALS_SWAMP_COUNT  = 6;
static constexpr int ANIMALS_DESERT_COUNT = 6;
static constexpr int ANIMALS_BIRDS_COUNT  = 6;

// ── Ambush NPCs ──────────────────────────────────────────────────────────────
static const uint32_t AMBUSH_NPCS[] = {
    0x39E870D3, // g_m_y_uniexconfeds_01 (Lemoyne Raiders)
    0xEA8CE68B, // g_m_o_uniexconfeds_01
    0x3B5E381D, // u_m_m_htlskinner_01   (Skinner Brothers)
    0x9BD5A8CE, // g_m_m_unimountainmen_01
    0x4300C73A, // g_m_m_unibanditos_01  (Del Lobo)
    0x62CFB213, // u_m_m_valpoliceman_01 (Law)
};
static constexpr int AMBUSH_NPCS_COUNT = 6;

// ── Zombie mode NPC pool ──────────────────────────────────────────────────────
// RDR2 has no undead models; we use ragged/outlaw peds with aggressive AI.
static const uint32_t ZOMBIE_NPCS[] = {
    0x3B5E381D, // u_m_m_htlskinner_01    — Skinner Brothers (scarred, dirty)
    0x9BD5A8CE, // g_m_m_unimountainmen_01 — Mountain Men (wild)
    0x39E870D3, // g_m_y_uniexconfeds_01  — Lemoyne Raiders
    0xEA8CE68B, // g_m_o_uniexconfeds_01  — older raider
    0x4300C73A, // g_m_m_unibanditos_01   — Del Lobo
    0xC9F27B1A, // g_m_y_uniodriscoll_01  — O'Driscoll Boys
    0xAE2B9773, // g_m_m_uniodriscoll_01  — O'Driscoll (older)
    0x4A557D91, // g_m_y_unioutlaws_01    — generic outlaws
};
static constexpr int ZOMBIE_NPCS_COUNT = 8;

// ── Plugin state ─────────────────────────────────────────────────────────────
static int   g_tickCounter    = 0;
static int   g_ambushCooldown = 0;
static bool  g_animalsEnabled = true;
static bool  g_ambushesEnabled= true;
static int   g_localPed       = 0;

// ── Pending spawns queue (async model loading) ────────────────────────────────
// REQUEST_MODEL is async — we queue the spawn and PluginTick executes it once
// HAS_MODEL_LOADED returns true (usually 1-3 frames later).
struct PendingSpawn { uint32_t hash; float x, y, z; int waited; bool zombie; };
static std::vector<PendingSpawn> g_pendingSpawns;

// ── Zombie mode ───────────────────────────────────────────────────────────────
static constexpr int   ZOMBIE_WAVE_FRAMES  = 1200; // frames between auto-waves (~20s @ 60fps)
static constexpr int   ZOMBIE_BASE_COUNT   = 3;    // zombies in wave 1
static constexpr int   ZOMBIE_MAX_COUNT    = 15;   // hard cap per wave
static constexpr float ZOMBIE_SPAWN_DIST   = 35.f; // meters from player

static bool              g_zombieMode  = false;
static int               g_zombieWave  = 0;
static int               g_zombieTimer = 0;
static std::vector<int>  g_zombiePeds;             // tracking active zombie peds

// ── Helpers ──────────────────────────────────────────────────────────────────

static int CountNearbyAnimals(int playerPed, float radius)
{
    std::vector<int> buf(257, 0);
    buf[0] = 256;
    int cnt = invoke<int>(0x23F8F5FC7E8C4A6BULL, playerPed, buf.data(), -1, 0); // GET_PED_NEARBY_PEDS
    if (cnt > 256) cnt = 256;
    RVector3 myPos = invoke<RVector3>(0xA86D5F069399F44DULL, playerPed, false);
    int animals = 0;
    for (int i = 0; i < cnt; i++)
    {
        int p = buf[i+1];
        if (p <= 0) continue;
        if (invoke<bool>(0xB980061DA992779DULL, p)) continue; // IS_PED_HUMAN
        RVector3 pp = invoke<RVector3>(0xA86D5F069399F44DULL, p, false);
        float dx = pp.x - myPos.x, dy = pp.y - myPos.y;
        if (dx*dx + dy*dy < radius*radius) animals++;
    }
    return animals;
}

// Queue a single animal spawn (async-safe)
static void QueueSpawn(uint32_t hash, float px, float py, float pz, bool zombie)
{
    if (!invoke<bool>(0x1283B8B89DD5D1B6ULL, hash)) // HAS_MODEL_LOADED
        invoke<void>(0xFA28FE3A6246FC30ULL, hash, false); // REQUEST_MODEL
    PendingSpawn ps;
    ps.hash = hash; ps.x = px; ps.y = py; ps.z = pz; ps.waited = 0; ps.zombie = zombie;
    g_pendingSpawns.push_back(ps);
}

// Queue a zombie wave — wave size scales with wave number
static void QueueZombieWave(float px, float py, float pz)
{
    g_zombieWave++;
    int count = ZOMBIE_BASE_COUNT + (g_zombieWave - 1);
    if (count > ZOMBIE_MAX_COUNT) count = ZOMBIE_MAX_COUNT;
    for (int i = 0; i < count; i++)
        QueueSpawn(ZOMBIE_NPCS[rand() % ZOMBIE_NPCS_COUNT], px, py, pz, true);
}

// Purge dead / removed zombie peds from tracking list
static void CleanZombiePeds()
{
    for (int i = (int)g_zombiePeds.size()-1; i >= 0; i--)
    {
        int p = g_zombiePeds[i];
        if (!invoke<bool>(0xD42BD6EB2E0F1677ULL, p) ||   // DOES_ENTITY_EXIST
             invoke<bool>(0x3317DEDB88C95038ULL, p, true)) // IS_PED_DEAD_OR_DYING
            g_zombiePeds.erase(g_zombiePeds.begin() + i);
    }
}

// ── Plugin exports ───────────────────────────────────────────────────────────

extern "C" __declspec(dllexport)
void PluginInit(int localPed, const char* /*username*/, bool*, uint32_t*, uint32_t*)
{
    g_localPed       = localPed;
    g_tickCounter    = 0;
    g_ambushCooldown = 0;
    g_zombieMode     = false;
    g_zombieWave     = 0;
    g_zombieTimer    = 0;
    g_pendingSpawns.clear();
    g_zombiePeds.clear();
    srand((unsigned)time(nullptr) + (unsigned)localPed);
}

extern "C" __declspec(dllexport)
void PluginTick(int localPed)
{
    g_localPed = localPed;

    // ── Process pending spawns (async model load) ────────────────────────────
    {
        int playerPedNow = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
        for (int i = (int)g_pendingSpawns.size()-1; i >= 0; i--)
        {
            auto& ps = g_pendingSpawns[i];
            if (invoke<bool>(0x1283B8B89DD5D1B6ULL, ps.hash)) // HAS_MODEL_LOADED
            {
                float ang  = (rand()%360) * 3.14159265f / 180.f;
                float dist = ps.zombie ? ZOMBIE_SPAWN_DIST : 5.f;
                int ped = invoke<int>(0xD49F9B0955C367DEULL, ps.hash,
                    ps.x + sinf(ang)*dist,
                    ps.y + cosf(ang)*dist,
                    ps.z,
                    ang * 180.f / 3.14159265f,
                    false, false, false, false); // CREATE_PED
                invoke<void>(0x4AD96EF928BD4F9AULL, ps.hash); // SET_MODEL_AS_NO_LONGER_NEEDED
                if (ped > 0)
                {
                    invoke<void>(0x283978A15512B2FEULL, ped, true); // SET_PED_DEFAULT_OUTFIT
                    invoke<void>(0xC8A9481A01E63C28ULL, ped, 0);    // SET_PED_RANDOM_COMPONENT_VARIATION
                    invoke<void>(0xCC8CA3E88256E58FULL, ped, false, true, true, true, true); // _UPDATE_PED_VARIATION
                    invoke<void>(0xDC19C288082E586EULL, ped, true, true); // SET_ENTITY_AS_MISSION_ENTITY
                    invoke<void>(0x1794B4FCC84D812FULL, ped, true);       // SET_ENTITY_VISIBLE
                    invoke<void>(0x9F8AA94D6D97DBF4ULL, ped, true);       // SET_BLOCKING_OF_NON_TEMP_EVENTS
                    if (ps.zombie && playerPedNow > 0)
                    {
                        // Charge directly at the local player
                        invoke<void>(0xF166E48407BAC484ULL, ped, playerPedNow, 0, 16); // TASK_COMBAT_PED
                        g_zombiePeds.push_back(ped);
                    }
                }
                g_pendingSpawns.erase(g_pendingSpawns.begin() + i);
            }
            else if (++ps.waited > 300) // ~5 second timeout
            {
                g_pendingSpawns.erase(g_pendingSpawns.begin() + i);
            }
        }
    }

    // ── Zombie mode: auto-wave ────────────────────────────────────────────────
    if (g_zombieMode)
    {
        CleanZombiePeds();
        g_zombieTimer++;

        // Trigger next wave when timer expires OR all zombies of current wave are dead
        bool waveCleared = (g_zombieWave > 0 && g_zombiePeds.empty() &&
                            g_pendingSpawns.empty());
        if (g_zombieTimer >= ZOMBIE_WAVE_FRAMES || waveCleared)
        {
            g_zombieTimer = 0;
            int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
            if (pp > 0)
            {
                RVector3 pos = invoke<RVector3>(0xA86D5F069399F44DULL, pp, false);
                QueueZombieWave(pos.x, pos.y, pos.z);
            }
        }
    }

    // ── Periodic animal / ambush scan ────────────────────────────────────────
    if (++g_tickCounter < SCAN_INTERVAL) return;
    g_tickCounter = 0;
    if (g_ambushCooldown > 0) g_ambushCooldown -= SCAN_INTERVAL;

    int playerPed = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
    if (playerPed <= 0) return;
    RVector3 pos = invoke<RVector3>(0xA86D5F069399F44DULL, playerPed, false);

    // Animal spawning
    if (g_animalsEnabled)
    {
        int nearby = CountNearbyAnimals(playerPed, ANIMAL_SCAN_RADIUS);
        int biome  = rand() % 4;
        const uint32_t* list = nullptr; int listCount = 0;
        switch (biome) {
            case 0: list = ANIMALS_PLAINS; listCount = ANIMALS_PLAINS_COUNT; break;
            case 1: list = ANIMALS_FOREST; listCount = ANIMALS_FOREST_COUNT; break;
            case 2: list = ANIMALS_SWAMP;  listCount = ANIMALS_SWAMP_COUNT;  break;
            case 3: list = ANIMALS_DESERT; listCount = ANIMALS_DESERT_COUNT; break;
        }
        int toSpawn = ANIMAL_MAX_DENSITY - nearby;
        if (toSpawn > 3) toSpawn = 3;
        float a = (rand()%360)*3.14159265f/180.f;
        float d = 30.f+(rand()%40);
        for (int i = 0; i < toSpawn && list; i++)
            QueueSpawn(list[rand()%listCount], pos.x+sinf(a)*d, pos.y+cosf(a)*d, pos.z, false);
        for (int i = 0; i < 2; i++)
            QueueSpawn(ANIMALS_BIRDS[rand()%ANIMALS_BIRDS_COUNT], pos.x, pos.y, pos.z+15.f, false);
    }

    // Random ambush
    if (g_ambushesEnabled && g_ambushCooldown <= 0 &&
        (rand()%100) < (int)(AMBUSH_CHANCE*100))
    {
        uint32_t m = AMBUSH_NPCS[rand() % AMBUSH_NPCS_COUNT];
        int count  = 2 + rand() % 4;
        for (int i = 0; i < count; i++)
        {
            float ang  = (rand()%360) * 3.14159265f / 180.f;
            float dist = 20.f + rand()%20;
            // Ambush NPCs use direct spawn (they are in memory already from common use)
            if (!invoke<bool>(0x1283B8B89DD5D1B6ULL, m))
                invoke<void>(0xFA28FE3A6246FC30ULL, m, false);
            if (!invoke<bool>(0x1283B8B89DD5D1B6ULL, m)) continue;
            int ped = invoke<int>(0xD49F9B0955C367DEULL, m,
                pos.x+sinf(ang)*dist, pos.y+cosf(ang)*dist, pos.z,
                0.f, false, false, false, false);
            if (ped <= 0) continue;
            invoke<void>(0x283978A15512B2FEULL, ped, true);
            invoke<void>(0xC8A9481A01E63C28ULL, ped, 0);
            invoke<void>(0xDC19C288082E586EULL, ped, true, true);
            invoke<void>(0x1794B4FCC84D812FULL, ped, true);
            invoke<void>(0xF166E48407BAC484ULL, ped, playerPed, 0, 16); // TASK_COMBAT_PED
        }
        invoke<void>(0x4AD96EF928BD4F9AULL, m);
        g_ambushCooldown = AMBUSH_COOLDOWN;
    }
}

extern "C" __declspec(dllexport)
void PluginChatCommand(const char* cmd, const char* args)
{
    // ── wevents on/off/animals/ambush ────────────────────────────────────────
    if (_stricmp(cmd, "wevents") == 0)
    {
        if (!args || !args[0]) return;
        if      (_stricmp(args, "on")     == 0) { g_animalsEnabled = true;  g_ambushesEnabled = true;  }
        else if (_stricmp(args, "off")    == 0) { g_animalsEnabled = false; g_ambushesEnabled = false; }
        else if (_stricmp(args, "animals")== 0)   g_animalsEnabled  = !g_animalsEnabled;
        else if (_stricmp(args, "ambush") == 0)   g_ambushesEnabled = !g_ambushesEnabled;
    }

    // ── ambush (manual trigger) ───────────────────────────────────────────────
    else if (_stricmp(cmd, "ambush") == 0)
    {
        int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
        if (pp <= 0) return;
        RVector3 pos = invoke<RVector3>(0xA86D5F069399F44DULL, pp, false);
        uint32_t m = AMBUSH_NPCS[rand() % AMBUSH_NPCS_COUNT];
        int count = 2 + rand() % 4;
        for (int i = 0; i < count; i++)
        {
            float ang  = (rand()%360) * 3.14159265f / 180.f;
            float dist = 20.f + rand()%20;
            if (!invoke<bool>(0x1283B8B89DD5D1B6ULL, m))
                invoke<void>(0xFA28FE3A6246FC30ULL, m, false);
            if (!invoke<bool>(0x1283B8B89DD5D1B6ULL, m)) continue;
            int ped = invoke<int>(0xD49F9B0955C367DEULL, m,
                pos.x+sinf(ang)*dist, pos.y+cosf(ang)*dist, pos.z,
                0.f, false, false, false, false);
            if (ped <= 0) continue;
            invoke<void>(0x283978A15512B2FEULL, ped, true);
            invoke<void>(0xC8A9481A01E63C28ULL, ped, 0);
            invoke<void>(0xDC19C288082E586EULL, ped, true, true);
            invoke<void>(0x1794B4FCC84D812FULL, ped, true);
            invoke<void>(0xF166E48407BAC484ULL, ped, pp, 0, 16);
        }
        invoke<void>(0x4AD96EF928BD4F9AULL, m);
    }

    // ── spawnanimal <name> ────────────────────────────────────────────────────
    else if (_stricmp(cmd, "spawnanimal") == 0)
    {
        int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
        if (pp <= 0 || !args || !args[0]) return;
        RVector3 pos = invoke<RVector3>(0xA86D5F069399F44DULL, pp, false);

        static struct { const char* name; uint32_t hash; } lookup[] = {
            {"deer",      0xD86C3B0A}, {"elk",       0x18E0680A}, {"buck",      0x4E5E8B8C},
            {"rabbit",    0xA5C5B0F9}, {"raccoon",   0x0E0B9E6D}, {"badger",    0xFB8C71A1},
            {"coyote",    0x876D37B3}, {"wolf",      0xC02DF42D}, {"bear",      0xA6BD2C2C},
            {"grizzly",   0xA6BD2C2C}, {"blackbear", 0x058C10B3}, {"boar",      0x0DFB13C8},
            {"skunk",     0x27D600A3}, {"alligator", 0x3BD14DC3}, {"snake",     0xDFB13C78},
            {"turtle",    0x6FD03F46}, {"heron",     0x29B109D1}, {"bighorn",   0xDFF3B7F0},
            {"vulture",   0x54CBB85F}, {"eagle",     0xCE31F3DC}, {"hawk",      0x9CE20DB9},
            {"crow",      0x5FC007C8}, {"duck",      0x0D0DFFA9}, {"armadillo", 0xC62CD222},
            {"cougar",    0xDCE22F0C}, {"puma",      0xDCE22F0C}, {"pronghorn", 0xAA6FF2C6},
        };
        uint32_t hash = 0;
        for (auto& e : lookup)
            if (_stricmp(args, e.name) == 0) { hash = e.hash; break; }
        if (hash == 0) return;

        // Queue async — PluginTick will spawn it once the model is streamed in
        QueueSpawn(hash, pos.x, pos.y, pos.z, false);
    }

    // ── zombie on / off / wave ────────────────────────────────────────────────
    else if (_stricmp(cmd, "zombie") == 0)
    {
        if (!args || !args[0]) return;

        if (_stricmp(args, "on") == 0)
        {
            if (!g_zombieMode)
            {
                g_zombieMode  = true;
                g_zombieWave  = 0;
                g_zombieTimer = ZOMBIE_WAVE_FRAMES; // trigger first wave immediately
                g_zombiePeds.clear();
            }
        }
        else if (_stricmp(args, "off") == 0)
        {
            g_zombieMode  = false;
            g_zombieWave  = 0;
            g_zombieTimer = 0;

            // Delete all active zombie peds
            for (int p : g_zombiePeds)
            {
                if (invoke<bool>(0xD42BD6EB2E0F1677ULL, p)) // DOES_ENTITY_EXIST
                    invoke<int>(0x4CD38C78BD19A497ULL, p);  // DELETE_ENTITY
            }
            g_zombiePeds.clear();

            // Cancel any pending zombie spawns
            g_pendingSpawns.erase(
                std::remove_if(g_pendingSpawns.begin(), g_pendingSpawns.end(),
                    [](const PendingSpawn& ps){ return ps.zombie; }),
                g_pendingSpawns.end());
        }
        else if (_stricmp(args, "wave") == 0)
        {
            // Manual wave trigger (works even if zombie mode is off)
            int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
            if (pp > 0)
            {
                RVector3 pos = invoke<RVector3>(0xA86D5F069399F44DULL, pp, false);
                QueueZombieWave(pos.x, pos.y, pos.z);
                g_zombieTimer = 0;
            }
        }
    }
}

extern "C" __declspec(dllexport)
void PluginShutdown()
{
    g_animalsEnabled  = false;
    g_ambushesEnabled = false;
    g_zombieMode      = false;
    g_pendingSpawns.clear();
    g_zombiePeds.clear();
}

extern "C" __declspec(dllexport)
const char* PluginCommands()
{
    return "wevents,ambush,spawnanimal,zombie";
}
