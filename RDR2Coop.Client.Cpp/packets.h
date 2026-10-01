#pragma once
#include <cstdint>
#include <string>
#include <cstring>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Wire protocol — same layout used by both C++ client and C# server.
// All multi-byte integers are little-endian (x86/x64 native).
//
// Every packet starts with 1 byte: PacketType.
// ─────────────────────────────────────────────────────────────────────────────

enum class PacketType : uint8_t
{
    PlayerConnect    = 1,
    PlayerDisconnect = 2,
    PlayerSync       = 3,
    ChatMessage      = 4,
    ServerInfo       = 5,
    ServerAdvertise  = 6,   // Server → client: LAN discovery response
    DiscoverRequest  = 7,   // Client → broadcast: "any servers out there?"
    EntitySync       = 8,   // Client → server → all: batch world-entity positions
WeatherSync      = 9,   // Server → client: weather and time broadcast
    SyncEvent        = 10,  // Client → server → all: combat events (shot, hit, etc)
    ClothingSync     = 11,  // Client → server → all: clothing component hashes
    PluginInfo       = 12,  // Client → server: loaded plugin names
    PluginCommand    = 13,  // Server → client: relay plugin command
};

// ── Helpers ──────────────────────────────────────────────────────────────────

// Writes a length-prefixed string (uint8 len + bytes, max 255 chars).
inline int WriteString(uint8_t* buf, int offset, const std::string& s)
{
    uint8_t len = (uint8_t)std::min((int)s.size(), 255);
    buf[offset++] = len;
    memcpy(buf + offset, s.c_str(), len);
    return offset + len;
}

// Reads a length-prefixed string. Returns new offset.
inline int ReadString(const uint8_t* buf, int offset, int bufLen, std::string& out)
{
    if (offset >= bufLen) return offset;
    uint8_t len = buf[offset++];
    if (offset + len > bufLen) len = (uint8_t)(bufLen - offset);
    out.assign((const char*)(buf + offset), len);
    return offset + len;
}

template<typename T>
inline int Write(uint8_t* buf, int offset, T val)
{
    memcpy(buf + offset, &val, sizeof(T));
    return offset + (int)sizeof(T);
}

template<typename T>
inline int Read(const uint8_t* buf, int offset, T& val)
{
    memcpy(&val, buf + offset, sizeof(T));
    return offset + (int)sizeof(T);
}

// ── Packet: PlayerConnect ─────────────────────────────────────────────────────
// Client → Server when joining.
// [type:1][username:str]
struct PlayerConnectPacket
{
    std::string username;

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::PlayerConnect;
        o = WriteString(buf, o, username);
        return o;
    }

    static PlayerConnectPacket Deserialize(const uint8_t* buf, int len)
    {
        PlayerConnectPacket p;
        ReadString(buf, 1, len, p.username);
        return p;
    }
};

// ── Packet: PlayerDisconnect ──────────────────────────────────────────────────
// Server → all clients when someone leaves.
// [type:1][player_id:4][username:str]
struct PlayerDisconnectPacket
{
    int32_t     playerId;
    std::string username;

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::PlayerDisconnect;
        o = Write(buf, o, playerId);
        o = WriteString(buf, o, username);
        return o;
    }

    static PlayerDisconnectPacket Deserialize(const uint8_t* buf, int len)
    {
        PlayerDisconnectPacket p;
        int o = 1;
        o = Read(buf, o, p.playerId);
        ReadString(buf, o, len, p.username);
        return p;
    }
};

// ── Packet: PlayerSync ────────────────────────────────────────────────────────
// Client → Server ~30x/sec. Server re-broadcasts to all other clients.
// [type:1][player_id:4][x:4][y:4][z:4][heading:4][health:4][speed:1][flags:1]
// [weaponHash:4][modelHash:4][horseModelHash:4][outfitHash:4][horseOutfitHash:4][username:str]
// flags: bit0=onHorse, bit1=aiming, bit2=shooting, bit3=dead
struct PlayerSyncPacket
{
    int32_t     playerId    = 0;
    float       x = 0, y = 0, z = 0;
    float       heading     = 0;
    float       health      = 1.f;
    uint8_t     speed       = 0;       // 0=idle 1=walk 2=run 3=sprint
    uint8_t     flags       = 0;
    uint32_t    weaponHash      = 0;   // current weapon (0 = unarmed)
    uint32_t    modelHash       = 0;   // ped model hash (GET_ENTITY_MODEL)
    uint32_t    horseModelHash  = 0;   // current mount model (0 = not on horse)
    uint32_t    outfitHash      = 0;   // _GET_PED_META_OUTFIT_HASH — player clothing preset
uint32_t    horseOutfitHash = 0;   // _GET_PED_META_OUTFIT_HASH on the mount — saddle/blanket
    int32_t     wantedLevel     = 0;   // police wanted level (0-5)
    uint8_t     passengerOfId   = 0;   // player ID whose horse we're a passenger on (0=none)
    std::string username;

    bool IsOnHorse()   const { return (flags & 0x01) != 0; }
    bool IsAiming()    const { return (flags & 0x02) != 0; }
    bool IsShooting()  const { return (flags & 0x04) != 0; }
    bool IsDead()      const { return (flags & 0x08) != 0; }
    bool IsPassenger() const { return (flags & 0x10) != 0; }
    void SetOnHorse(bool v)   { if (v) flags |= 0x01; else flags &= ~0x01; }
    void SetAiming(bool v)    { if (v) flags |= 0x02; else flags &= ~0x02; }
    void SetShooting(bool v)  { if (v) flags |= 0x04; else flags &= ~0x04; }
    void SetDead(bool v)      { if (v) flags |= 0x08; else flags &= ~0x08; }
    void SetPassenger(bool v) { if (v) flags |= 0x10; else flags &= ~0x10; }

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::PlayerSync;
        o = Write(buf, o, playerId);
        o = Write(buf, o, x); o = Write(buf, o, y); o = Write(buf, o, z);
        o = Write(buf, o, heading);
        o = Write(buf, o, health);
        buf[o++] = speed;
        buf[o++] = flags;
        o = Write(buf, o, weaponHash);
        o = Write(buf, o, modelHash);
        o = Write(buf, o, horseModelHash);
        o = Write(buf, o, outfitHash);
        o = Write(buf, o, horseOutfitHash);
        o = Write(buf, o, wantedLevel);
        buf[o++] = passengerOfId;
        o = WriteString(buf, o, username);
        return o;
    }

    static PlayerSyncPacket Deserialize(const uint8_t* buf, int len)
    {
        PlayerSyncPacket p;
        int o = 1;
        o = Read(buf, o, p.playerId);
        o = Read(buf, o, p.x);  o = Read(buf, o, p.y);  o = Read(buf, o, p.z);
        o = Read(buf, o, p.heading);
        o = Read(buf, o, p.health);
        p.speed = buf[o++];
        p.flags = buf[o++];
        o = Read(buf, o, p.weaponHash);
        o = Read(buf, o, p.modelHash);
        o = Read(buf, o, p.horseModelHash);
if (o + 4 <= len) o = Read(buf, o, p.outfitHash);      // backwards-compat guard
        if (o + 4 <= len) o = Read(buf, o, p.horseOutfitHash); // backwards-compat guard
        o = Read(buf, o, p.wantedLevel);
        if (o < len) p.passengerOfId = buf[o++];
        ReadString(buf, o, len, p.username);
        return p;
    }
};

// ── Packet: EntitySync ───────────────────────────────────────────────────────
// Client → Server → all other clients (BroadcastExcept sender).
// Sent every ~10 frames carrying the state of up to 12 world entities the local
// player owns (mission-entity NPCs / animals spawned by that player).
// Remote clients spawn or update each entity by netId, and delete any entity
// that has been absent for > 500 ms (handles the entity being deleted in-game).
//
// Wire layout per entry (35 bytes):
//   [netId:2][modelHash:4][entityType:1][x:4][y:4][z:4][heading:4][vx:4][vy:4][vz:4]
// Total max packet size: 1 + 4 + 1 + 12 * 35 = 426 bytes — under MAX_PACKET (1024).

constexpr int ENTITY_SYNC_MAX = 24;

struct EntitySyncEntry
{
    uint16_t netId      = 0;   // sender-assigned stable ID for this entity
    uint32_t modelHash  = 0;   // model hash (for CREATE_PED on remote)
    uint8_t  entityType = 0;   // GET_ENTITY_TYPE: 1=ped/animal, 3=object
    uint8_t  flags      = 0;   // bit 0 = isMounted (ped is on a horse)
    float    x=0, y=0, z=0;   // world position
    float    heading    = 0;   // yaw degrees
    float    velX=0, velY=0, velZ=0;   // physics velocity (m/s) for dead-reckoning
};

struct EntitySyncPacket
{
    int32_t         senderId = 0;
    uint8_t         count    = 0;
    EntitySyncEntry entries[ENTITY_SYNC_MAX] = {};

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::EntitySync;
        o = Write(buf, o, senderId);
        buf[o++] = count;
        for (int i = 0; i < count; i++)
        {
            const auto& e = entries[i];
            o = Write(buf, o, e.netId);
            o = Write(buf, o, e.modelHash);
            buf[o++] = e.entityType;
            buf[o++] = e.flags;
            o = Write(buf, o, e.x);  o = Write(buf, o, e.y);  o = Write(buf, o, e.z);
            o = Write(buf, o, e.heading);
            o = Write(buf, o, e.velX); o = Write(buf, o, e.velY); o = Write(buf, o, e.velZ);
        }
        return o;
    }

    static EntitySyncPacket Deserialize(const uint8_t* buf, int len)
    {
        EntitySyncPacket p;
        int o = 1;
        o = Read(buf, o, p.senderId);
        p.count = (o < len) ? buf[o++] : 0;
        if (p.count > ENTITY_SYNC_MAX) p.count = ENTITY_SYNC_MAX;
        for (int i = 0; i < p.count; i++)
        {
            auto& e = p.entries[i];
            if (o + 2  > len) break; o = Read(buf, o, e.netId);
            if (o + 4  > len) break; o = Read(buf, o, e.modelHash);
            if (o + 2  > len) break; e.entityType = buf[o++]; e.flags = buf[o++];
            if (o + 12 > len) break;
            o = Read(buf, o, e.x); o = Read(buf, o, e.y); o = Read(buf, o, e.z);
            if (o + 4  > len) break; o = Read(buf, o, e.heading);
            if (o + 12 > len) break;
            o = Read(buf, o, e.velX); o = Read(buf, o, e.velY); o = Read(buf, o, e.velZ);
        }
        return p;
    }
};

// ── Packet: ChatMessage ───────────────────────────────────────────────────────
// [type:1][username:str][message:str]
struct ChatMessagePacket
{
    std::string username;
    std::string message;

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::ChatMessage;
        o = WriteString(buf, o, username);
        o = WriteString(buf, o, message);
        return o;
    }

    static ChatMessagePacket Deserialize(const uint8_t* buf, int len)
    {
        ChatMessagePacket p;
        int o = 1;
        o = ReadString(buf, o, len, p.username);
        ReadString(buf, o, len, p.message);
        return p;
    }
};

// ── Packet: ServerInfo ────────────────────────────────────────────────────────
// Server → client immediately after PlayerConnect is received.
// [type:1][assigned_id:4][player_count:2][max_players:2][server_name:str]
struct ServerInfoPacket
{
    int32_t     assignedId  = 0;
    uint16_t    playerCount = 0;
    uint16_t    maxPlayers  = 0;
    std::string serverName;

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::ServerInfo;
        o = Write(buf, o, assignedId);
        o = Write(buf, o, playerCount);
        o = Write(buf, o, maxPlayers);
        o = WriteString(buf, o, serverName);
        return o;
    }

    static ServerInfoPacket Deserialize(const uint8_t* buf, int len)
    {
        ServerInfoPacket p;
        int o = 1;
        o = Read(buf, o, p.assignedId);
        o = Read(buf, o, p.playerCount);
        o = Read(buf, o, p.maxPlayers);
        ReadString(buf, o, len, p.serverName);
        return p;
    }
};

// ── Packet: ServerAdvertise ───────────────────────────────────────────────────
// Server → broadcast (255.255.255.255) on UDP port 7778 every 2s for LAN discovery.
// [type:1][port:2][players:1][maxPlayers:1][name:str]
struct ServerAdvertisePacket
{
    uint16_t    port       = 7777;
    uint8_t     players    = 0;
    uint8_t     maxPlayers = 16;
    std::string name;

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o = 0;
        buf[o++] = (uint8_t)PacketType::ServerAdvertise;
        o = Write(buf, o, port);
        buf[o++] = players;
        buf[o++] = maxPlayers;
        o = WriteString(buf, o, name);
        return o;
    }

    static ServerAdvertisePacket Deserialize(const uint8_t* buf, int len)
    {
        ServerAdvertisePacket p;
        int o = 1;
        o = Read(buf, o, p.port);
        if (o < len) p.players    = buf[o++];
        if (o < len) p.maxPlayers = buf[o++];
        ReadString(buf, o, len, p.name);
        return p;
    }
};

// Packet: WeatherSync - Server -> client: periodic weather/time broadcast
struct WeatherSyncPacket
{
    uint32_t weatherType = 0;
    float    transitionTime = 5.0f;
    int      hour = 12, minute = 0, second = 0;

    int Serialize(uint8_t* buf, int maxLen) const { return 0; }
    static WeatherSyncPacket Deserialize(const uint8_t* buf, int len)
    {
        WeatherSyncPacket p; int o = 1;
        o = Read(buf,o,p.weatherType); o = Read(buf,o,p.transitionTime);
        if (o < len) p.hour   = buf[o++];
        if (o < len) p.minute = buf[o++];
        if (o < len) p.second = buf[o++];
        return p;
    }
};

// Packet: SyncEvent - Client -> server -> all: combat event
enum SyncEventType : uint8_t { PedKilled=0, PedHit=1, HorseStolen=2, Shot=3, EntityKilled=4 };
struct SyncEventPacket
{
    uint8_t  eventType = 0;
    int32_t  attackerId = 0, victimId = 0;
    uint32_t extraData = 0;

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o=0; buf[o++]=(uint8_t)PacketType::SyncEvent;
        buf[o++]=eventType; o=Write(buf,o,attackerId); o=Write(buf,o,victimId); o=Write(buf,o,extraData);
        return o;
    }
    static SyncEventPacket Deserialize(const uint8_t* buf, int len)
    {
        SyncEventPacket p; int o=1;
        p.eventType=buf[o++]; o=Read(buf,o,p.attackerId); o=Read(buf,o,p.victimId); o=Read(buf,o,p.extraData);
        return p;
    }
};

// PluginCommandPacket — server → client: console command forwarded to plugins
struct PluginCommandPacket
{
    char cmd[64]  = {};
    char args[128]= {};

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o=0; buf[o++]=(uint8_t)PacketType::PluginCommand;
        uint8_t clen=(uint8_t)std::min((int)strlen(cmd),63);
        buf[o++]=clen; memcpy(buf+o,cmd,clen); o+=clen;
        uint8_t alen=(uint8_t)std::min((int)strlen(args),127);
        buf[o++]=alen; memcpy(buf+o,args,alen); o+=alen;
        return o;
    }
    static PluginCommandPacket Deserialize(const uint8_t* buf, int len)
    {
        PluginCommandPacket p; int o=1;
        if(o<len){uint8_t cl=buf[o++];if(cl>63)cl=63;memcpy(p.cmd,buf+o,cl);p.cmd[cl]=0;o+=cl;}
        if(o<len){uint8_t al=buf[o++];if(al>127)al=127;memcpy(p.args,buf+o,al);p.args[al]=0;}
        return p;
    }
};

// Packet: ClothingSync - Client -> server -> all: clothing component hashes
struct ClothingSyncPacket
{
    int32_t  senderId = 0;
    uint8_t  count = 0;
    uint32_t hashes[25] = {};

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o=0; buf[o++]=(uint8_t)PacketType::ClothingSync;
        o=Write(buf,o,senderId); buf[o++]=count;
        for(int i=0;i<count&&i<25;i++) o=Write(buf,o,hashes[i]);
        return o;
    }
    static ClothingSyncPacket Deserialize(const uint8_t* buf, int len)
    {
        ClothingSyncPacket p; int o=1;
        o=Read(buf,o,p.senderId); p.count=buf[o++];
        if(p.count>25)p.count=25;
        for(int i=0;i<p.count&&o+4<=len;i++) o=Read(buf,o,p.hashes[i]);
        return p;
    }
};

// PluginInfoPacket — client → server on connect
struct PluginInfoPacket
{
    int32_t  playerId = 0;
    char     names[200] = {};  // comma-separated plugin names

    int Serialize(uint8_t* buf, int maxLen) const
    {
        int o=0; buf[o++]=(uint8_t)PacketType::PluginInfo;
        o=Write(buf,o,playerId);
        uint8_t len = (uint8_t)std::min((int)strlen(names), 199);
        buf[o++] = len;
        memcpy(buf+o, names, len); o+=len;
        return o;
    }
    static PluginInfoPacket Deserialize(const uint8_t* buf, int len)
    {
        PluginInfoPacket p; int o=1;
        o=Read(buf,o,p.playerId);
        if (o<len) { uint8_t slen = buf[o++]; if(slen>199)slen=199;
            memcpy(p.names, buf+o, slen); p.names[slen]=0; }
        return p;
    }
};
