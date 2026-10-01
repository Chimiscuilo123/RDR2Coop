#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include "../packets.h"

// ─────────────────────────────────────────────────────────────────────────────
// LAN server discovery — single-threaded, non-blocking.
//
// Call Tick() once per game frame from the main loop.
// Every 2 seconds it sends a DiscoverRequest (1 byte, type=7) to:
//   • 255.255.255.255:<gamePort>  — reaches all servers on the same LAN
//   • 127.0.0.1:<gamePort>        — reaches a server on the same machine
// Server responds with a ServerAdvertise unicast packet.
// The socket is non-blocking so Tick() never stalls the game.
//
// No background thread, no CRITICAL_SECTION, no WSAStartup conflicts.
// ─────────────────────────────────────────────────────────────────────────────

class LanScanner
{
public:
    struct ServerInfo
    {
        std::string ip;
        int         port       = 7777;
        std::string name;
        int         players    = 0;
        int         maxPlayers = 16;
        DWORD       lastSeen   = 0;
    };

    LanScanner() {}

    ~LanScanner() { Stop(); }

    // Call once after WSAStartup (NetworkClient calls it on Connect, but we
    // call it ourselves here so discovery works before any connection).
    void Start(int gamePort = 7777)
    {
        if (_sock != INVALID_SOCKET) return;

        _gamePort = gamePort;
        _lastProbe = 0;   // probe immediately on first Tick

        // WSAStartup is managed at process level (DllMain) — don't call it here.
        _sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_sock == INVALID_SOCKET)
        {
            _debug = "ERR socket=" + std::to_string(WSAGetLastError());
            return;
        }

        BOOL yes = TRUE;
        setsockopt(_sock, SOL_SOCKET, SO_BROADCAST, (char*)&yes, sizeof(yes));
        setsockopt(_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

        // Non-blocking — recvfrom returns immediately if nothing available
        u_long nb = 1;
        ioctlsocket(_sock, FIONBIO, &nb);

        sockaddr_in local{};
        local.sin_family      = AF_INET;
        local.sin_port        = htons(7778); // receive server beacons sent to :7778
        local.sin_addr.s_addr = INADDR_ANY;

        if (bind(_sock, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR)
        {
            _debug = "ERR bind=" + std::to_string(WSAGetLastError());
            closesocket(_sock);
            _sock = INVALID_SOCKET;
            return;
        }

        _debug = "OK";
    }

    void Stop()
    {
        if (_sock != INVALID_SOCKET)
        {
            closesocket(_sock);
            _sock = INVALID_SOCKET;
        }
        // WSACleanup is managed at process level — don't call it here.
    }

    // Call once per game frame from the main loop.
    void Tick()
    {
        if (_sock == INVALID_SOCKET) return;

        // ── Send probes every 2 seconds ──────────────────────────────────────
        DWORD now = GetTickCount();
        if (now - _lastProbe >= 2000)
        {
            _lastProbe = now;
            SendProbe();
        }

        // ── Receive all waiting responses (non-blocking) ─────────────────────
        for (int i = 0; i < 16; i++)   // drain up to 16 packets per frame
        {
            uint8_t buf[512];
            sockaddr_in from{};
            int fromLen = sizeof(from);

            int n = recvfrom(_sock, (char*)buf, (int)sizeof(buf), 0,
                             (sockaddr*)&from, &fromLen);
            if (n <= 0) break;  // nothing left (WSAEWOULDBLOCK or error)

            if (n < 1 || (PacketType)buf[0] != PacketType::ServerAdvertise) continue;

            auto pkt = ServerAdvertisePacket::Deserialize(buf, n);

            char ipStr[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));

            StoreServer(ipStr, pkt);
            _responsesRecv++;
        }

        // ── Prune stale servers ──────────────────────────────────────────────
        for (int i = (int)_servers.size() - 1; i >= 0; i--)
            if (now - _servers[i].lastSeen > 10000)
                _servers.erase(_servers.begin() + i);
    }

    const std::vector<ServerInfo>& GetServers() const { return _servers; }

    // Short status line shown in the LAN tab of the menu.
    std::string GetDebugStr() const
    {
        if (_sock == INVALID_SOCKET) return _debug;
        char buf[64];
        snprintf(buf, sizeof(buf), "p=%d r=%d svr=%d",
                 _probesSent, _responsesRecv, (int)_servers.size());
        return buf;
    }

private:
    SOCKET                  _sock      = INVALID_SOCKET;
    int                     _gamePort  = 7777;
    DWORD                   _lastProbe = 0;
    std::vector<ServerInfo> _servers;
    std::string             _debug     = "not started";
    int                     _probesSent    = 0;
    int                     _responsesRecv = 0;

    void SendProbe()
    {
        uint8_t buf[1] = { (uint8_t)PacketType::DiscoverRequest };

        sockaddr_in dst{};
        dst.sin_family = AF_INET;
        dst.sin_port   = htons((u_short)_gamePort);

        // 1. Broadcast — reaches all servers on the LAN segment
        dst.sin_addr.s_addr = INADDR_BROADCAST;
        sendto(_sock, (char*)buf, 1, 0, (sockaddr*)&dst, sizeof(dst));

        // 2. Loopback — Windows doesn't loop broadcast back to localhost
        dst.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sendto(_sock, (char*)buf, 1, 0, (sockaddr*)&dst, sizeof(dst));

        _probesSent++;
    }

    void StoreServer(const char* ip, const ServerAdvertisePacket& pkt)
    {
        DWORD now = GetTickCount();
        for (auto& s : _servers)
        {
            if (s.ip == ip && s.port == (int)pkt.port)
            {
                s.name = pkt.name; s.players = pkt.players;
                s.maxPlayers = pkt.maxPlayers; s.lastSeen = now;
                return;
            }
        }
        ServerInfo si;
        si.ip = ip; si.port = (int)pkt.port; si.name = pkt.name;
        si.players = (int)pkt.players; si.maxPlayers = (int)pkt.maxPlayers;
        si.lastSeen = now;
        _servers.push_back(si);
    }
};
