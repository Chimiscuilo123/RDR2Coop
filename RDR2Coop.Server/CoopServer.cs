using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Threading;
using RDR2Coop.Core;
using RDR2Coop.Core.Packets;

// Fixed UDP port used for LAN server discovery broadcasts
// (must match LAN_DISCOVERY_PORT in lanscanner.h)
// ReSharper disable once CheckNamespace

namespace RDR2Coop.Server
{
    public class CoopServer
    {
        public event Action<string>? OnLog;

        private void Log(string msg) => OnLog?.Invoke(msg);
        private const int LanDiscoveryPort = 7778;

        private UdpClient                               _udp      = null!;
        private Timer?                                  _lanTimer = null;
        private Timer?                                  _weatherTimer = null;
        // ConcurrentDictionary: receive thread and cleanup timer access _players concurrently
        private readonly ConcurrentDictionary<string, ConnectedPlayer> _players = new();
        private int  _nextId     = 1;
        private bool _running    = false;
        private int  _debugSyncCount = 0;

        // Weather: default to CLEAR (JOAAT lowercase of "clear"), matches CustomizableTrainer
        private uint _currentWeather = 0x36A83D84;  // Joaat("clear") = 0x36A83D84
        // Time: default to 12:00:00 (noon)
        private int _currentHour = 12;
        private int _currentMinute = 0;
        private int _currentSecond = 0;

        public string Name           { get; }
        public int    Port           { get; }
        public int    MaxPlayers     { get; }
        public string WelcomeMessage { get; }
        public bool   LocalHostIsAdmin { get; set; } = false;

        public CoopServer(string name, int port, int maxPlayers,
                          string welcomeMessage = "Welcome!")
        {
            Name           = name;
            Port           = port;
            MaxPlayers     = maxPlayers;
            WelcomeMessage = welcomeMessage;
        }

        private Thread?                 _webThread;
        private HttpListener?            _webListener;
        public int WebPort => Port + 1;

        public void Start()
        {
            _udp = new UdpClient(Port);
            _udp.EnableBroadcast = true;
            _running = true;

            Log($"[RDR2Coop] Server '{Name}' running on UDP port {Port}");
            Log($"[RDR2Coop] Max players: {MaxPlayers}");
            Log($"[RDR2Coop] Web admin → http://localhost:{WebPort}");

            _lanTimer = new Timer(_ => SendLanBeacon(),
                null, TimeSpan.Zero, TimeSpan.FromSeconds(2));

            _weatherTimer = new Timer(_ => BroadcastWeather(),
                null, TimeSpan.Zero, TimeSpan.FromSeconds(5));

            var t = new Thread(ReceiveLoop) { IsBackground = true, Name = "RecvLoop" };
            t.Start();

            StartWebAdmin();
        }

        public void Stop()
        {
            _running = false;
            _webListener?.Stop();
            _webListener?.Close();
            _lanTimer?.Dispose();
            _weatherTimer?.Dispose();

            // Notify every connected client so they disconnect immediately
            // instead of waiting for their local timeout to fire.
            foreach (var p in _players.Values)
            {
                try
                {
                    var disc = new PlayerDisconnectPacket
                    {
                        PlayerId = p.Id,
                        Username = p.Username,
                    };
                    byte[] data = disc.Serialize();
                    _udp.Send(data, data.Length, p.Endpoint);
                }
                catch { /* socket may already be closing */ }
            }

            _udp?.Close();
        }

        // ── Web Admin ──────────────────────────────────────────────────────────

        private void StartWebAdmin()
        {
            _webThread = new Thread(() =>
            {
                try
                {
                    _webListener = new HttpListener();
                    _webListener.Prefixes.Add($"http://localhost:{WebPort}/");
                    _webListener.Start();
                    Log($"Web admin ready: http://localhost:{WebPort}");
                    while (_running)
                    {
                        var ctx = _webListener.GetContext();
                        ThreadPool.QueueUserWorkItem(_ => HandleWebRequest(ctx));
                    }
                }
                catch (HttpListenerException ex)
                {
                    Log($"Web admin: Access Denied. Run as admin or: netsh http add urlacl url=http://+:{WebPort}/ user=TODOS");
                    Console.Error.WriteLine($"Run: netsh http add urlacl url=http://+:{WebPort}/ user=%USERNAME%");
                }
                catch (Exception ex) { Log($"Web admin error: {ex.Message}"); }
            })
            { IsBackground = true, Name = "WebAdmin" };
            _webThread.Start();
        }

        void HandleWebRequest(HttpListenerContext ctx)
        {
            try
            {
                string path = ctx.Request.Url!.AbsolutePath;
                if (path == "/api/status")
                {
                    var players = GetPlayers();
                    var json = "[";
                    for (int i = 0; i < players.Count; i++)
                    {
                        var p = players[i];
                        if (i > 0) json += ",";
                        var pluginsJson = "[" + string.Join(",", p.Plugins.Select(pl => "\"" + EscapeJson(pl) + "\"")) + "]";
                        json += "{\"id\":" + p.Id + ",\"name\":\"" + EscapeJson(p.Username) + "\",\"admin\":" + (p.IsAdmin ? "true" : "false") + ",\"plugins\":" + pluginsJson + "}";
                    }
                    json += "]";
                    RespondJson(ctx, "{\"name\":\"" + Name.Replace("\\", "\\\\").Replace("\"", "\\\"") + "\",\"players\":" + json + ",\"count\":" + players.Count + ",\"max\":" + MaxPlayers + ",\"port\":" + Port + "}");
                }
                else if (path == "/api/cmd")
                {
                    string? c = ctx.Request.QueryString["c"];
                    if (!string.IsNullOrEmpty(c)) ProcessWebCmd(c);
                    RespondJson(ctx, "{\"ok\":true}");
                }
                else RespondHtml(ctx, GetAdminPage());
            }
            catch { try { ctx.Response.StatusCode = 500; ctx.Response.Close(); } catch { } }
        }

        static string EscapeJson(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"");

        static void RespondJson(HttpListenerContext ctx, string json)
        {
            ctx.Response.ContentType = "application/json";
            var buf = System.Text.Encoding.UTF8.GetBytes(json);
            ctx.Response.OutputStream.Write(buf, 0, buf.Length);
            ctx.Response.OutputStream.Close();
        }

        static void RespondHtml(HttpListenerContext ctx, string html)
        {
            ctx.Response.ContentType = "text/html; charset=utf-8";
            var buf = System.Text.Encoding.UTF8.GetBytes(html);
            ctx.Response.OutputStream.Write(buf, 0, buf.Length);
            ctx.Response.OutputStream.Close();
        }

        void ProcessWebCmd(string cmd)
        {
            var parts = cmd.Split(' ', StringSplitOptions.RemoveEmptyEntries);
            if (parts.Length == 0) return;
            switch (parts[0].ToLower())
            {
                case "/weather": if (parts.Length > 1) SetWeatherByName(parts[1]); break;
                case "/time": if (parts.Length > 2 && int.TryParse(parts[1], out int h) && int.TryParse(parts[2], out int m)) SetTime(h, m); break;
                case "/say": if (parts.Length > 1) BroadcastMessage(string.Join(" ", parts.Skip(1))); break;
                case "/kick": if (parts.Length > 1) KickPlayer(parts[1]); break;
                case "/op": if (parts.Length > 1) SetAdmin(parts[1], true); break;
                case "/deop": if (parts.Length > 1) SetAdmin(parts[1], false); break;
                case "/tp": if (parts.Length > 1) TeleportPlayer(parts[1], parts.Length > 2 ? parts[2] : ""); break;
                case "/pcmd": if (parts.Length > 2) { if (parts[1] == "all") BroadcastPluginCommand(parts[2], parts.Length > 3 ? string.Join(" ", parts.Skip(3)) : ""); else { var pl = GetPlayers().FirstOrDefault(x => x.Username.StartsWith(parts[1], StringComparison.OrdinalIgnoreCase)); if (pl != null) SendPluginCommand(pl, parts[2], parts.Length > 3 ? string.Join(" ", parts.Skip(3)) : ""); } } break;
            }
        }

        string GetAdminPage()
        {
            var path = System.IO.Path.Combine(AppContext.BaseDirectory, "admin.html");
            if (System.IO.File.Exists(path))
                return System.IO.File.ReadAllText(path);
            return "<html><body><h1>RDR2Coop Admin</h1><p>admin.html not found</p></body></html>";
        }

        // ── LAN Discovery ──────────────────────────────────────────────────────

        private void SendLanBeacon()
        {
            try
            {
                byte[] data = MakeAdvertise();
                _udp.Send(data, data.Length,
                    new IPEndPoint(IPAddress.Broadcast, LanDiscoveryPort));
                _udp.Send(data, data.Length,
                    new IPEndPoint(IPAddress.Loopback,  LanDiscoveryPort));
            }
            catch { }
        }

        private byte[] MakeAdvertise() =>
            new ServerAdvertisePacket
            {
                Port       = (ushort)Port,
                Players    = (byte)_players.Count,
                MaxPlayers = (byte)MaxPlayers,
                Name       = Name,
            }.Serialize();

        // ── Receive loop ────────────────────────────────────────────────────

        private void ReceiveLoop()
        {
            while (_running)
            {
                try
                {
                    var remote = new IPEndPoint(IPAddress.Any, 0);
                    byte[] data = _udp.Receive(ref remote);
                    if (data.Length == 0) continue;

                    string key = remote.ToString();
                    HandlePacket(key, remote, data);
                }
                catch (SocketException) when (!_running) { break; }
                catch (SocketException se) when (se.SocketErrorCode == SocketError.ConnectionReset)
                {
                    // ICMP "port unreachable" reply from a beacon sent to a host
                    // with no client running yet — harmless, just keep looping.
                }
                catch (Exception ex)
                {
                    Log($"[RDR2Coop] Recv error: {ex.Message}");
                }
            }
        }

        // ── Packet dispatch ─────────────────────────────────────────────────

        private void HandlePacket(string key, IPEndPoint ep, byte[] data)
        {
            var type = (PacketType)data[0];

            switch (type)
            {
                case PacketType.PlayerConnect:
                    HandleConnect(key, ep, data);
                    break;
                case PacketType.PlayerSync:
                    HandleSync(key, ep, data);
                    break;
                case PacketType.ChatMessage:
                    HandleChat(key, data);
                    break;
                case PacketType.DiscoverRequest:
                    HandleDiscoverRequest(ep);
                    break;
                case PacketType.EntitySync:
                    HandleEntitySync(key, ep, data);
                    break;
                case PacketType.SyncEvent:
                    HandleSyncEvent(key, data);
                    break;
                case PacketType.ClothingSync:
                    BroadcastExcept(data, ep);
                    break;
                case PacketType.PluginInfo:
                    HandlePluginInfo(key, data);
                    break;
            }
        }

        private void HandleConnect(string key, IPEndPoint ep, byte[] data)
        {
            // Already connected? ignore duplicate
            if (_players.ContainsKey(key)) return;

            if (_players.Count >= MaxPlayers)
            {
                Log($"[RDR2Coop] Rejected {ep} — server full");
                return;
            }

            var packet = PlayerConnectPacket.Deserialize(data);
            var player = new ConnectedPlayer
            {
                Id       = Interlocked.Increment(ref _nextId),
                Username = packet.Username,
                Endpoint = ep
            };
            _players[key] = player;  // ConcurrentDictionary: safe

            // Grant admin to localhost client if enabled
            if (LocalHostIsAdmin && IPAddress.IsLoopback(ep.Address))
            {
                player.IsAdmin = true;
                Log($"[RDR2Coop] {player.Username} is admin (localhost)");
            }

            Log($"[RDR2Coop] {player.Username} connected ({ep}) | {_players.Count}/{MaxPlayers}");

            // Welcome message → appears in the new player's chat box
            if (!string.IsNullOrWhiteSpace(WelcomeMessage))
            {
                var welcome = new ChatMessagePacket
                {
                    Username = "Servidor",
                    Message  = WelcomeMessage,
                };
                Send(ep, welcome.Serialize());
            }

            // Send ServerInfo back to the new player
            var info = new ServerInfoPacket
            {
                AssignedId  = player.Id,
                ServerName  = Name,
                PlayerCount = (ushort)_players.Count,
                MaxPlayers  = (ushort)MaxPlayers
            };
            Send(ep, info.Serialize());

            // Send current weather/time immediately to new player
            var w = new WeatherSyncPacket
            {
                WeatherType = _currentWeather,
                TransitionTime = 5.0f,
                Hour = _currentHour, Minute = _currentMinute, Second = _currentSecond
            };
            Send(ep, w.Serialize());

            // Tell everyone else about the new arrival
            var join = new PlayerConnectPacket { Username = player.Username };
            BroadcastExcept(join.Serialize(), ep);
        }

        private void HandleSync(string key, IPEndPoint ep, byte[] data)
        {
            if (!_players.TryGetValue(key, out var player)) return;

            player.Touch(); // reset 30-second timeout

            var packet = PlayerSyncPacket.Deserialize(data);
            // Stamp server-authoritative id so clients can't spoof others
            packet.PlayerId = player.Id;
            packet.Username = player.Username;
            // Debug
            if (_debugSyncCount++ < 3)
                Log($"[DEBUG] Sync '{player.Username}' id={player.Id}");
            
            // BroadcastAll (not Except) so the sender also gets their own sync echoed back.
            // The client ignores its own sync by playerId check, but the echo updates
            // lastPacketMs — preventing the 10-second "server gone" timeout when alone.
            BroadcastAll(packet.Serialize());
        }

        private void HandleEntitySync(string key, IPEndPoint ep, byte[] data)
        {
            // Verify the sender is a known player (drop spoofed / stale packets).
            if (!_players.TryGetValue(key, out var player)) return;
            player.Touch();   // count as heartbeat — resets the 30-second timeout

            // Relay to every OTHER client unchanged.
            // The senderId inside the payload identifies whose entities these are;
            // we trust it because only authenticated players reach this branch.
            BroadcastExcept(data, ep);
        }

        private void HandleDiscoverRequest(IPEndPoint ep)
        {
            // Muted: LAN working, no need to log every probe
            Send(ep, MakeAdvertise());
        }

        private void HandleChat(string key, byte[] data)
        {
            if (!_players.TryGetValue(key, out var player)) return;

            var packet = ChatMessagePacket.Deserialize(data);
            packet.Username = player.Username;

            Log($"[Chat] {player.Username}: {packet.Message}");
            BroadcastAll(packet.Serialize());
        }

        // ── Helpers ─────────────────────────────────────────────────────────

        private void Send(IPEndPoint ep, byte[] data)
        {
            try { _udp.Send(data, data.Length, ep); }
            catch { }
        }

        private void BroadcastAll(byte[] data)
        {
            foreach (var p in _players.Values) Send(p.Endpoint, data);
        }

        private void BroadcastExcept(byte[] data, IPEndPoint exclude)
        {
            foreach (var p in _players.Values)
                if (!p.Endpoint.Equals(exclude)) Send(p.Endpoint, data);
        }

        // Simple heartbeat/cleanup: remove players silent for >30s
        public void CleanupStale(TimeSpan timeout)
        {
            var now  = DateTime.UtcNow;
            var dead = _players
                .Where(kv => now - kv.Value.LastSeen > timeout)
                .Select(kv => kv.Key).ToList();

            foreach (var key in dead)
            {
                if (!_players.TryRemove(key, out var p)) continue;
                Log($"[RDR2Coop] {p.Username} timed out");

                var disc = new PlayerDisconnectPacket { PlayerId = p.Id, Username = p.Username };
                BroadcastAll(disc.Serialize());
            }
        }

        // ── Weather & Time Sync ─────────────────────────────────────────────────────
        public void SetWeather(uint weatherType)
        {
            _currentWeather = weatherType;
            Log($"[RDR2Coop] Weather changed to 0x{weatherType:X8}");
            BroadcastWeather();
        }

        public void SetTime(int hour, int minute, int second = 0)
        {
            _currentHour = hour;
            _currentMinute = minute;
            _currentSecond = second;
            Log($"[RDR2Coop] Time changed to {hour:D2}:{minute:D2}:{second:D2}");
            BroadcastWeather();
        }

        private void HandleSyncEvent(string key, byte[] data)
        {
            if (!_players.TryGetValue(key, out var player)) return;
            
            var evt = SyncEventPacket.Deserialize(data);
            // Stamp sender's ID
            evt.AttackerId = player.Id;
            
            // Broadcast to all other players
            BroadcastExcept(evt.Serialize(), player.Endpoint);
            
            Log($"[RDR2Coop] SyncEvent: {evt.EventType} - victim={evt.VictimId}, attacker={evt.AttackerId}");
        }

        private void BroadcastWeather()
        {
            if (_players.IsEmpty) return;

            var weatherPacket = new WeatherSyncPacket
            {
                WeatherType = _currentWeather,
                TransitionTime = 5.0f,
                Hour = _currentHour,
                Minute = _currentMinute,
                Second = _currentSecond
            };
            var data = weatherPacket.Serialize();
            BroadcastAll(data);
            // Muted: weather sync works, no need to log every 5s
        }

        // ── Console Commands ───────────────────────────────────────────────────
        public void BroadcastPluginCommand(string command, string args)
        {
            var packet = new Core.Packets.PluginCommandPacket
            {
                Command = command,
                Args = args
            };
            BroadcastAll(packet.Serialize());
            Log($"[RDR2Coop] Plugin cmd broadcast: {command} {args}");
        }

        public void SendPluginCommand(ConnectedPlayer target, string command, string args)
        {
            var packet = new Core.Packets.PluginCommandPacket
            {
                Command = command,
                Args = args
            };
            Send(target.Endpoint, packet.Serialize());
            Log($"[RDR2Coop] Plugin cmd to {target.Username}: {command} {args}");
        }

        public List<ConnectedPlayer> GetPlayers()
        {
            return _players.Values.ToList();
        }

        public void KickPlayer(string username)
        {
            var target = _players.Values.FirstOrDefault(p => 
                p.Username.Equals(username, StringComparison.OrdinalIgnoreCase));
            
            if (target == null)
            {
                Log($"[RDR2Coop] Player '{username}' not found.");
                return;
            }

            Log($"[RDR2Coop] Kicking {target.Username}...");
            
            var disc = new PlayerDisconnectPacket
            {
                PlayerId = target.Id,
                Username = target.Username
            };
            Send(target.Endpoint, disc.Serialize());
            BroadcastAll(disc.Serialize());
            
            _players.TryRemove(target.Endpoint.ToString(), out _);
        }

        public void SetWeatherByName(string weatherName)
        {
            uint weatherHash = Joaat(weatherName.ToLower()); // RDR2 expects lowercase!
            SetWeather(weatherHash);
            Log($"[RDR2Coop] Weather: {weatherName} (0x{weatherHash:X8})");
        }
        
        static uint Joaat(string input)
        {
            uint hash = 0;
            foreach (char c in input)
            {
                hash += c;
                hash += (hash << 10);
                hash ^= (hash >> 6);
            }
            hash += (hash << 3);
            hash ^= (hash >> 11);
            hash += (hash << 15);
            return hash;
        }

        private void HandlePluginInfo(string key, byte[] data)
        {
            if (!_players.TryGetValue(key, out var player)) return;
            var packet = new Core.Packets.PluginInfoPacket();
            packet.Deserialize(data, data.Length);
            player.PluginRaw = packet.RawNames;
            player.Plugins = ParsePluginEntries(packet.RawNames)
                .Select(e => e.Contains('[') ? e[..e.IndexOf('[')] : e)
                .ToArray();
            Log($"[RDR2Coop] {player.Username} plugins: {string.Join(", ", player.Plugins)}");
        }

        static string[] ParsePluginEntries(string raw)
        {
            var entries = new List<string>();
            int depth = 0, start = 0;
            for (int i = 0; i < raw.Length; i++)
            {
                if (raw[i] == '[') depth++;
                else if (raw[i] == ']') depth--;
                else if (raw[i] == ',' && depth == 0)
                {
                    entries.Add(raw[start..i]);
                    start = i + 1;
                }
            }
            if (start < raw.Length) entries.Add(raw[start..]);
            return entries.ToArray();
        }

        public void BroadcastMessage(string message)
        {
            var chatPacket = new ChatMessagePacket
            {
                Username = "SERVIDOR",
                Message = message
            };
            BroadcastAll(chatPacket.Serialize());
            Log($"[Servidor] {message}");
        }

        public void SetAdmin(string name, bool admin)
        {
            var players = GetPlayers();
            var target = players.FirstOrDefault(p => 
                p.Username.StartsWith(name, StringComparison.OrdinalIgnoreCase));
            if (target == null) { Log($"[RDR2Coop] '{name}' not found."); return; }
            target.IsAdmin = admin;
            Log($"[RDR2Coop] {target.Username} is now {(admin ? "" : "NOT ")}admin.");
        }

        public void TeleportPlayer(string destName, string sourceName = "")
        {
            var players = GetPlayers();
            var dest = players.FirstOrDefault(p => 
                p.Username.StartsWith(destName, StringComparison.OrdinalIgnoreCase));
            
            if (dest == null)
            {
                Log($"[RDR2Coop] Destination '{destName}' not found.");
                return;
            }

            var chatPacket = new ChatMessagePacket
            {
                Username = "SERVIDOR",
                Message = $"!cmd tp {dest.Username}"
            };

            // If no source specified, broadcast to all
            if (string.IsNullOrEmpty(sourceName))
            {
                BroadcastAll(chatPacket.Serialize());
                Log($"[RDR2Coop] All players teleported to {dest.Username}");
                return;
            }

            // Send TP only to the specific source player
            var source = players.FirstOrDefault(p => 
                p.Username.StartsWith(sourceName, StringComparison.OrdinalIgnoreCase));
            
            if (source == null)
            {
                Log($"[RDR2Coop] Source '{sourceName}' not found.");
                return;
            }

            Send(source.Endpoint, chatPacket.Serialize());
            Log($"[RDR2Coop] {source.Username} teleported to {dest.Username}");
        }

        public void SendConsoleCommand(string command)
        {
            var chatPacket = new ChatMessagePacket
            {
                Username = "SERVIDOR",
                Message = $"!cmd {command}"
            };
            BroadcastAll(chatPacket.Serialize());
        }
    }
}
