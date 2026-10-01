#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "packets.h"

// ─────────────────────────────────────────────────────────────────────────────
// UDP network client — pure Win32, no std::thread / std::mutex.
// No libwinpthread-1.dll dependency.
// Runs a background receive thread; packets are queued and dispatched
// on the main game thread via Poll().
// ─────────────────────────────────────────────────────────────────────────────

class NetworkClient
{
public:
    static constexpr int MAX_PACKET = 1024;

    bool  connected     = false;
    int   localId       = -1;
    int   ping          = 0;
    DWORD lastPacketMs  = 0;   // GetTickCount() of the last packet received from server

    std::function<void(const ServerInfoPacket&)>       onServerInfo;
    std::function<void(const PlayerSyncPacket&)>       onPlayerSync;
    std::function<void(const EntitySyncPacket&)>       onEntitySync;
    std::function<void(const PlayerConnectPacket&)>    onPlayerConnect;
    std::function<void(const PlayerDisconnectPacket&)> onPlayerDisconnect;
    std::function<void(const ChatMessagePacket&)>      onChatMessage;
    std::function<void(const WeatherSyncPacket&)>     onWeatherSync;
    std::function<void(const SyncEventPacket&)>       onSyncEvent;
    std::function<void(const ClothingSyncPacket&)>   onClothingSync;
    std::function<void(const PluginCommandPacket&)>   onPluginCommand;
    std::function<void()>                              onDisconnected;

    NetworkClient()
        : _sock(INVALID_SOCKET), _running(0), _recvThread(nullptr)
    {
        InitializeCriticalSection(&_queueLock);
    }

    ~NetworkClient()
    {
        Disconnect();
        DeleteCriticalSection(&_queueLock);
    }

    bool Connect(const std::string& host, int port, const std::string& username)
    {
        if (_sock != INVALID_SOCKET) return false;

        // WSAStartup is managed at process level (DllMain).
        _sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_sock == INVALID_SOCKET) return false;

        DWORD timeout = 100;
        setsockopt(_sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        memset(&_serverAddr, 0, sizeof(_serverAddr));
        _serverAddr.sin_family = AF_INET;
        _serverAddr.sin_port   = htons((u_short)port);
        inet_pton(AF_INET, host.c_str(), &_serverAddr.sin_addr);

        InterlockedExchange(&_running, 1);
        _recvThread = CreateThread(nullptr, 0, RecvThread, this, 0, nullptr);
        if (!_recvThread) return false;

        // Send PlayerConnect immediately — server responds with ServerInfo.
        SendConnect(username);
        return true;
    }

    void Disconnect()
    {
        InterlockedExchange(&_running, 0);
        if (_recvThread)
        {
            WaitForSingleObject(_recvThread, 2000);
            CloseHandle(_recvThread);
            _recvThread = nullptr;
        }
        if (_sock != INVALID_SOCKET)
        {
            closesocket(_sock);
            _sock = INVALID_SOCKET;
        }
        connected = false;
        localId   = -1;
        // WSACleanup is managed at process level (DllMain).
    }

    void SendConnect(const std::string& username)
    {
        PlayerConnectPacket p; p.username = username;
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    void SendSync(const PlayerSyncPacket& p)
    {
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    void SendEntitySync(const EntitySyncPacket& p)
    {
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    void SendClothing(const ClothingSyncPacket& p)
    {
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    void SendEvent(const SyncEventPacket& p)
    {
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    void SendPluginInfo(int playerId, const char* names)
    {
        PluginInfoPacket p;
        p.playerId = playerId;
        strncpy(p.names, names, 199);
        p.names[199] = 0;
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    void SendChat(const std::string& username, const std::string& msg)
    {
        ChatMessagePacket p; p.username = username; p.message = msg;
        uint8_t buf[MAX_PACKET];
        int len = p.Serialize(buf, MAX_PACKET);
        RawSend(buf, len);
    }

    // Call once per game frame to dispatch received packets on the game thread.
    void Poll()
    {
        std::vector<std::vector<uint8_t>> pending;
        EnterCriticalSection(&_queueLock);
        pending.swap(_queue);
        LeaveCriticalSection(&_queueLock);

        for (auto& data : pending)
            Dispatch(data.data(), (int)data.size());
    }

private:
    SOCKET           _sock;
    sockaddr_in      _serverAddr;
    HANDLE           _recvThread;
    volatile LONG    _running;
    CRITICAL_SECTION _queueLock;
    std::vector<std::vector<uint8_t>> _queue;

    void RawSend(const uint8_t* buf, int len)
    {
        if (_sock == INVALID_SOCKET) return;
        sendto(_sock, (const char*)buf, len, 0,
               (sockaddr*)&_serverAddr, sizeof(_serverAddr));
    }

    // Static trampoline for CreateThread
    static DWORD WINAPI RecvThread(LPVOID param)
    {
        static_cast<NetworkClient*>(param)->RecvLoop();
        return 0;
    }

    void RecvLoop()
    {
        uint8_t buf[MAX_PACKET];
        sockaddr_in from;
        int fromLen = sizeof(from);

        while (InterlockedCompareExchange(&_running, 1, 1) == 1)
        {
            int n = recvfrom(_sock, (char*)buf, MAX_PACKET, 0,
                             (sockaddr*)&from, &fromLen);
            if (n <= 0) continue;

            std::vector<uint8_t> data(buf, buf + n);
            EnterCriticalSection(&_queueLock);
            _queue.push_back(std::move(data));
            LeaveCriticalSection(&_queueLock);
        }
    }

    void Dispatch(const uint8_t* buf, int len)
    {
        if (len < 1) return;
        lastPacketMs = GetTickCount();   // any packet from server = server is alive
        auto type = (PacketType)buf[0];

        switch (type)
        {
        case PacketType::ServerInfo:
        {
            auto p = ServerInfoPacket::Deserialize(buf, len);
            localId   = p.assignedId;
            connected = true;
            if (onServerInfo) onServerInfo(p);
            break;
        }
        case PacketType::PlayerSync:
        {
            auto p = PlayerSyncPacket::Deserialize(buf, len);
            if (onPlayerSync) onPlayerSync(p);
            break;
        }
        case PacketType::EntitySync:
        {
            auto p = EntitySyncPacket::Deserialize(buf, len);
            if (onEntitySync) onEntitySync(p);
            break;
        }
        case PacketType::PlayerConnect:
        {
            auto p = PlayerConnectPacket::Deserialize(buf, len);
            if (onPlayerConnect) onPlayerConnect(p);
            break;
        }
        case PacketType::PlayerDisconnect:
        {
            auto p = PlayerDisconnectPacket::Deserialize(buf, len);
            if (p.playerId == localId) { connected = false; if (onDisconnected) onDisconnected(); }
            else if (onPlayerDisconnect) onPlayerDisconnect(p);
            break;
        }
        case PacketType::ChatMessage:
        {
            auto p = ChatMessagePacket::Deserialize(buf, len);
            if (onChatMessage) onChatMessage(p);
            break;
        }
        case PacketType::WeatherSync:
        {
            auto p = WeatherSyncPacket::Deserialize(buf, len);
            if (onWeatherSync) onWeatherSync(p);
            break;
        }
        case PacketType::SyncEvent:
        {
            auto p = SyncEventPacket::Deserialize(buf, len);
            if (onSyncEvent) onSyncEvent(p);
            break;
        }
        case PacketType::ClothingSync:
        {
            auto p = ClothingSyncPacket::Deserialize(buf, len);
            if (onClothingSync) onClothingSync(p);
            break;
        }
        case PacketType::PluginCommand:
        {
            auto p = PluginCommandPacket::Deserialize(buf, len);
            if (onPluginCommand) onPluginCommand(p);
            break;
        }
        }
    }
};
