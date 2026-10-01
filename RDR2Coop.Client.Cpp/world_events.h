#pragma once
#include "script.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <ctime>

struct WorldEvents {
    // ── Config ────────────────────────────────────────────────────────────────
    static constexpr int   SCAN_INTERVAL      = 300;   // frames between auto-spawns (~5s)
    static constexpr int   AMBUSH_CD          = 1800;  // frames between random ambushes (~30s)
    static constexpr int   ZOMBIE_WAVE_FRAMES = 1200;  // frames between zombie waves (~20s)
    static constexpr int   ZOMBIE_MAX_COUNT   = 15;    // hard cap per wave
    static constexpr float ZOMBIE_SPAWN_DIST  = 30.f;  // meters from player
    static constexpr int   PENDING_TIMEOUT    = 300;   // frames before giving up on a model load (~5s)

    // ── State ─────────────────────────────────────────────────────────────────
    int  tick = 0, ambushCd = 0;
    bool animals = true, ambushes = true;
    bool zombieMode = false;
    int  zombieWave = 0, zombieTimer = 0;
    std::vector<int> zombiePeds;

    // ── Pending spawn queue (async model loading) ─────────────────────────────
    struct PendingSpawn { uint32_t hash; float x,y,z; bool zombie; int waited; };
    std::vector<PendingSpawn> pendingSpawns;

    // ── Native helpers ────────────────────────────────────────────────────────
    // RDR2 returns Vector3 as 6 floats: x,pad,y,pad,z,pad (24 bytes total).
    // Each coordinate is followed by a 4-byte padding float — DO NOT read them
    // at consecutive 4-byte offsets or y/z will silently get garbage values.
    struct NativeVec3 { float x, _px, y, _py, z, _pz; };
    struct Vec3 { float x,y,z; };
    static Vec3 GetCoords(int e)
    {
        NativeVec3 nv = invoke<NativeVec3>(0xA86D5F069399F44DULL, e, false); // GET_ENTITY_COORDS
        return { nv.x, nv.y, nv.z };
    }

    // ── Queue a spawn (model load is async — executed in ProcessPending) ──────
    void QueueSpawn(uint32_t hash, float x, float y, float z, bool zombie)
    {
        if (!invoke<bool>(0x1283B8B89DD5D1B6ULL, hash)) // HAS_MODEL_LOADED
            invoke<void>(0xFA28FE3A6246FC30ULL, hash, false); // REQUEST_MODEL (async)
        PendingSpawn ps; ps.hash=hash; ps.x=x; ps.y=y; ps.z=z; ps.zombie=zombie; ps.waited=0;
        pendingSpawns.push_back(ps);
    }

    // ── Execute pending spawns once their model is ready ──────────────────────
    void ProcessPending()
    {
        int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
        for (int i = (int)pendingSpawns.size()-1; i >= 0; i--)
        {
            auto& ps = pendingSpawns[i];
            if (invoke<bool>(0x1283B8B89DD5D1B6ULL, ps.hash)) // HAS_MODEL_LOADED
            {
                float dist = ps.zombie ? ZOMBIE_SPAWN_DIST : 25.f;
                float ang  = (rand()%360) * 3.14159265f / 180.f;
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
                    invoke<void>(0xCC8CA3E88256E58FULL, ped, false, true, true, true, true);
                    invoke<void>(0xDC19C288082E586EULL, ped, true, true); // SET_ENTITY_AS_MISSION_ENTITY
                    invoke<void>(0x1794B4FCC84D812FULL, ped, true);       // SET_ENTITY_VISIBLE
                    invoke<void>(0x9F8AA94D6D97DBF4ULL, ped, true);       // SET_BLOCKING_OF_NON_TEMP_EVENTS
                    if (ps.zombie && pp > 0)
                    {
                        invoke<void>(0xF166E48407BAC484ULL, ped, pp, 0, 16); // TASK_COMBAT_PED
                        zombiePeds.push_back(ped);
                    }
                }
                pendingSpawns.erase(pendingSpawns.begin() + i);
            }
            else if (++ps.waited > PENDING_TIMEOUT)
            {
                pendingSpawns.erase(pendingSpawns.begin() + i); // give up
            }
        }
    }

    // ── Purge dead zombie peds from tracking list ─────────────────────────────
    void CleanZombies()
    {
        for (int i = (int)zombiePeds.size()-1; i >= 0; i--)
        {
            int p = zombiePeds[i];
            if (!invoke<bool>(0xD42BD6EB2E0F1677ULL, p) ||   // DOES_ENTITY_EXIST
                 invoke<bool>(0x3317DEDB88C95038ULL, p, true)) // IS_PED_DEAD_OR_DYING
                zombiePeds.erase(zombiePeds.begin() + i);
        }
    }

    // ── Count nearby non-human peds (animals) ─────────────────────────────────
    int CountAnimals(int pp, float r)
    {
        std::vector<int> b(257,0); b[0]=256;
        int c = invoke<int>(0x23F8F5FC7E8C4A6BULL, pp, b.data(), -1, 0);
        if (c > 256) c = 256;
        Vec3 mp = GetCoords(pp);
        int a = 0;
        for (int i = 0; i < c; i++)
        {
            int p = b[i+1]; if (p <= 0) continue;
            if (invoke<bool>(0xB980061DA992779DULL, p)) continue; // IS_PED_HUMAN
            Vec3 pp2 = GetCoords(p);
            float dx = pp2.x-mp.x, dy = pp2.y-mp.y;
            if (dx*dx + dy*dy < r*r) a++;
        }
        return a;
    }

    // ── Called every game frame from g_plugins.events.onTick ─────────────────
    void OnTick()
    {
        // Pending queue runs EVERY frame (model streaming can take a few frames)
        ProcessPending();

        // Zombie wave timer runs every frame too
        if (zombieMode)
        {
            CleanZombies();
            bool waveCleared = zombieWave > 0 && zombiePeds.empty() && pendingSpawns.empty();
            if (++zombieTimer >= ZOMBIE_WAVE_FRAMES || waveCleared)
            {
                zombieTimer = 0;
                int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
                if (pp > 0)
                {
                    Vec3 pos = GetCoords(pp);
                    SpawnZombieWave(pos.x, pos.y, pos.z);
                }
            }
        }

        // Animal / ambush scan runs every SCAN_INTERVAL frames only
        if (++tick < SCAN_INTERVAL) return;
        tick = 0;
        if (ambushCd > 0) ambushCd -= SCAN_INTERVAL;

        int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
        if (pp <= 0) return;
        Vec3 pos = GetCoords(pp);

        // Animal spawning (fill up to max density)
        if (animals)
        {
            static const uint32_t PLAINS[]={0xD86C3B0A,0x18E0680A,0xAA6FF2C6,0x4E5E8B8C,0xA5C5B0F9,0x0E0B9E6D,0xFB8C71A1,0x876D37B3,0xC02DF42D};
            static const uint32_t FOREST[]={0x058C10B3,0xA6BD2C2C,0xD86C3B0A,0xAA6FF2C6,0x0DFB13C8,0xFB8C71A1,0x27D600A3,0xC02DF42D};
            static const uint32_t SWAMP[] ={0x3BD14DC3,0x4BEE5640,0xDFB13C78,0x6FD03F46,0x29B109D1,0x0E0B9E6D};
            static const uint32_t DESERT[]={0xDFF3B7F0,0x54CBB85F,0x876D37B3,0xFB8C71A1,0xCE31F3DC,0xC62CD222};
            static const uint32_t BIRDS[] ={0x54CBB85F,0xCE31F3DC,0x9CE20DB9,0x5FC007C8,0x29B109D1,0x0D0DFFA9};

            int nearby = CountAnimals(pp, 80);
            int toSpawn = 10 - nearby; if (toSpawn > 3) toSpawn = 3;
            int biome = rand() % 4;
            const uint32_t* list = nullptr; int lc = 0;
            switch(biome){case 0:list=PLAINS;lc=9;break;case 1:list=FOREST;lc=8;break;case 2:list=SWAMP;lc=6;break;case 3:list=DESERT;lc=6;break;}

            float ang = (rand()%360)*3.14159265f/180.f;
            float d   = 30.f + rand()%40;
            for (int i = 0; i < toSpawn && list; i++)
                QueueSpawn(list[rand()%lc], pos.x+sinf(ang)*d, pos.y+cosf(ang)*d, pos.z, false);
            for (int i = 0; i < 2; i++)
                QueueSpawn(BIRDS[rand()%6], pos.x, pos.y, pos.z+15.f, false);
        }

        // Random ambush
        if (ambushes && ambushCd <= 0 && (rand()%100) < 15)
        {
            static const uint32_t A[]={0x39E870D3,0xEA8CE68B,0x3B5E381D,0x9BD5A8CE,0x4300C73A,0x62CFB213};
            uint32_t m = A[rand()%6];
            int count = 2 + rand()%4;
            for (int i = 0; i < count; i++)
            {
                float a = (rand()%360)*3.14159265f/180.f;
                float d = 20.f + rand()%20;
                QueueSpawn(m, pos.x+sinf(a)*d, pos.y+cosf(a)*d, pos.z, false);
            }
            ambushCd = AMBUSH_CD;
        }
    }

    // ── Queue a full zombie wave ──────────────────────────────────────────────
    void SpawnZombieWave(float px, float py, float pz)
    {
        zombieWave++;
        static const uint32_t Z[]={0x39E870D3,0xEA8CE68B,0x3B5E381D,0x9BD5A8CE,0x4300C73A,0xC9F27B1A,0xAE2B9773,0x4A557D91};
        int count = 3 + (zombieWave - 1);
        if (count > ZOMBIE_MAX_COUNT) count = ZOMBIE_MAX_COUNT;
        for (int i = 0; i < count; i++)
            QueueSpawn(Z[rand()%8], px, py, pz, true);
    }

    // ── Chat / plugin command handler ─────────────────────────────────────────
    void OnCommand(const char* cmd, const char* args)
    {
        int pp = invoke<int>(0x275F255ED201B937ULL, invoke<int>(0x47E385B0D957C8D4ULL));
        Vec3 pos = {0,0,0};
        if (pp > 0) pos = GetCoords(pp);

        // wevents on/off/animals/ambush
        if (_stricmp(cmd, "wevents") == 0)
        {
            if (!args || !args[0]) return;
            if      (_stricmp(args,"on")     ==0) { animals=true;  ambushes=true;  }
            else if (_stricmp(args,"off")    ==0) { animals=false; ambushes=false; }
            else if (_stricmp(args,"animals")==0)   animals  = !animals;
            else if (_stricmp(args,"ambush") ==0)   ambushes = !ambushes;
        }

        // ambush (manual trigger)
        else if (_stricmp(cmd, "ambush") == 0 && pp > 0)
        {
            static const uint32_t A[]={0x39E870D3,0xEA8CE68B,0x3B5E381D,0x9BD5A8CE,0x4300C73A,0x62CFB213};
            uint32_t m = A[rand()%6];
            int count = 2 + rand()%4;
            for (int i = 0; i < count; i++)
            {
                float a = (rand()%360)*3.14159265f/180.f;
                float d = 20.f + rand()%20;
                QueueSpawn(m, pos.x+sinf(a)*d, pos.y+cosf(a)*d, pos.z, false);
            }
        }

        // spawnanimal <name>
        else if (_stricmp(cmd, "spawnanimal") == 0 && pp > 0 && args && args[0])
        {
            static struct { const char* n; uint32_t h; } lk[] = {
                {"deer",0xD86C3B0A},{"elk",0x18E0680A},{"buck",0x4E5E8B8C},
                {"rabbit",0xA5C5B0F9},{"raccoon",0x0E0B9E6D},{"badger",0xFB8C71A1},
                {"coyote",0x876D37B3},{"wolf",0xC02DF42D},{"bear",0xA6BD2C2C},
                {"grizzly",0xA6BD2C2C},{"blackbear",0x058C10B3},{"boar",0x0DFB13C8},
                {"skunk",0x27D600A3},{"alligator",0x3BD14DC3},{"snake",0xDFB13C78},
                {"turtle",0x6FD03F46},{"heron",0x29B109D1},{"bighorn",0xDFF3B7F0},
                {"vulture",0x54CBB85F},{"eagle",0xCE31F3DC},{"hawk",0x9CE20DB9},
                {"crow",0x5FC007C8},{"duck",0x0D0DFFA9},{"armadillo",0xC62CD222},
                {"cougar",0xDCE22F0C},{"puma",0xDCE22F0C},{"pronghorn",0xAA6FF2C6},
            };
            uint32_t h = 0;
            for (auto& e : lk) if (_stricmp(args, e.n)==0) { h=e.h; break; }
            if (h) QueueSpawn(h, pos.x, pos.y, pos.z, false);
        }

        // zombie on / off / wave
        else if (_stricmp(cmd, "zombie") == 0)
        {
            if (!args || !args[0]) return;
            if (_stricmp(args, "on") == 0)
            {
                zombieMode  = true;
                zombieWave  = 0;
                zombieTimer = ZOMBIE_WAVE_FRAMES; // trigger first wave immediately next tick
                zombiePeds.clear();
            }
            else if (_stricmp(args, "off") == 0)
            {
                zombieMode = false;
                zombieWave = 0;
                zombieTimer = 0;
                for (int p : zombiePeds)
                    if (invoke<bool>(0xD42BD6EB2E0F1677ULL, p))
                        invoke<int>(0x4CD38C78BD19A497ULL, p); // DELETE_ENTITY
                zombiePeds.clear();
                // Cancel pending zombie spawns
                pendingSpawns.erase(
                    std::remove_if(pendingSpawns.begin(), pendingSpawns.end(),
                        [](const PendingSpawn& ps){ return ps.zombie; }),
                    pendingSpawns.end());
            }
            else if (_stricmp(args, "wave") == 0)
            {
                // Force next wave immediately (works even outside auto-mode)
                if (pp > 0) SpawnZombieWave(pos.x, pos.y, pos.z);
                zombieTimer = 0;
            }
        }
    }
};
