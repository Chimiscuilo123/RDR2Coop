#pragma once
#include <vector>
#include <algorithm>
#include <windows.h>

struct PoolEntity
{
    int      handle    = -1;
    int      ownerId   = -1;   // g_net.localId of the player who spawned this
    uint32_t modelHash = 0;
    uint8_t  type      = 0;    // 1=ped, 3=object
    float    x=0, y=0, z=0, heading=0;
    DWORD    lastUpdate = 0;
    bool     active     = false;
};

class EntityPool
{
    std::vector<PoolEntity> _pool;
    int _maxPerPlayer = 50;
    DWORD _timeout = 5000; // 5s without update → delete

public:
    EntityPool(int maxPerPlayer = 50, DWORD timeout = 5000)
        : _maxPerPlayer(maxPerPlayer), _timeout(timeout) {}

    int CountForOwner(int owner) const
    {
        int n = 0;
        for (auto& e : _pool)
            if (e.ownerId == owner && e.active) n++;
        return n;
    }

    bool CanSpawn(int owner) const { return CountForOwner(owner) < _maxPerPlayer; }

    int Register(int handle, int owner, uint32_t model, uint8_t type)
    {
        if (!CanSpawn(owner)) return -1;
        PoolEntity e;
        e.handle = handle;
        e.ownerId = owner;
        e.modelHash = model;
        e.type = type;
        e.lastUpdate = GetTickCount();
        e.active = true;
        _pool.push_back(e);
        return (int)_pool.size() - 1; // index
    }

    void Update(int handle, float x, float y, float z, float heading)
    {
        for (auto& e : _pool)
        {
            if (e.handle == handle && e.active)
            {
                e.x = x; e.y = y; e.z = z; e.heading = heading;
                e.lastUpdate = GetTickCount();
                return;
            }
        }
    }

    void Remove(int handle)
    {
        for (auto& e : _pool)
        {
            if (e.handle == handle && e.active)
            {
                e.active = false;
                e.handle = -1;
                return;
            }
        }
    }

    int FindByModel(uint32_t model, float nearX = -1e9f, float nearY = -1e9f, float nearZ = -1e9f, float maxDist = 5.0f)
    {
        for (auto& e : _pool)
        {
            if (!e.active) continue;
            if (e.modelHash != model) continue;
            if (e.type != 1) continue;
            bool anyPos = (nearX > -1e8f && nearY > -1e8f && nearZ > -1e8f);
            if (anyPos)
            {
                float dx = e.x - nearX, dy = e.y - nearY, dz = e.z - nearZ;
                if (dx*dx + dy*dy + dz*dz > maxDist*maxDist) continue;
            }
            return e.handle;
        }
        return -1;
    }

    void TransferOwnership(int handle, int newOwner)
    {
        for (auto& e : _pool)
        {
            if (e.handle == handle && e.active)
            {
                e.ownerId = newOwner;
                e.lastUpdate = GetTickCount();
                return;
            }
        }
    }

    void RemoveOwner(int ownerId)
    {
        for (auto& e : _pool)
            if (e.ownerId == ownerId && e.active)
                e.active = false;
    }

    void CleanupStale()
    {
        DWORD now = GetTickCount();
        for (auto& e : _pool)
        {
            if (e.active && (now - e.lastUpdate) > _timeout)
            {
                e.active = false;
                e.handle = -1;
            }
        }
    }

    std::vector<PoolEntity> GetActiveForOwner(int owner) const
    {
        std::vector<PoolEntity> result;
        for (auto& e : _pool)
            if (e.ownerId == owner && e.active)
                result.push_back(e);
        return result;
    }
};