using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using RDR2Coop.Server;

Console.Title = "RDR2Coop Server";
Console.WriteLine("╔══════════════════════════════════════╗");
Console.WriteLine("║       RDR2 Coop  Server  v0.1        ║");
Console.WriteLine("╚══════════════════════════════════════╝");
Console.WriteLine();

// ── Load settings ────────────────────────────────────────────────────────────
string settingsPath = Path.Combine(AppContext.BaseDirectory, "Settings.xml");
var cfg = Settings.Load(settingsPath);

Console.WriteLine($"[Config] Name        : {cfg.Name}");
Console.WriteLine($"[Config] Port        : {cfg.Port}");
Console.WriteLine($"[Config] Max Players : {cfg.MaxPlayers}");
Console.WriteLine($"[Config] Description : {cfg.Description}");
Console.WriteLine($"[Config] Mode        : {cfg.GameMode}");
Console.WriteLine();

// ── UPnP ────────────────────────────────────────────────────────────────────
Console.WriteLine("[UPnP] Searching router...");
bool upnpOk = await UPnPHelper.OpenPortAsync(cfg.Port, $"RDR2 Coop ({cfg.Name})");
if (!upnpOk)
    Console.WriteLine("[UPnP] LAN play does not require port forwarding.");
Console.WriteLine();

// ── Start server ─────────────────────────────────────────────────────────────
var server = new CoopServer(cfg.Name, cfg.Port, cfg.MaxPlayers, cfg.WelcomeMessage);
server.LocalHostIsAdmin = cfg.LocalHostIsAdmin;
server.Start();

Console.WriteLine("Available commands:");
Console.WriteLine("  /help          - Show commands");
Console.WriteLine("  /players       - List players");
Console.WriteLine("  /op <name>     - Grant admin to player");
Console.WriteLine("  /deop <name>   - Revoke admin");
Console.WriteLine("  /kick <name>   - Kick player");
Console.WriteLine("  /weather <type>- Change weather");
Console.WriteLine("  /time <h> <m>  - Change time");
Console.WriteLine("  /say <message> - Broadcast message");
Console.WriteLine("  /tp <dest> [src] - Teleport player(s)");
Console.WriteLine("  /pcmd <player|all> <cmd> <args> - Plugin command");
Console.WriteLine("  /plugins <name> - Show player plugins");
Console.WriteLine("  /clear         - Clear console");
Console.WriteLine();
Console.WriteLine("Press Ctrl+C to stop.\n");

using var cts = new CancellationTokenSource();
Console.CancelKeyPress += (_, e) => { e.Cancel = true; cts.Cancel(); };

var timeout      = TimeSpan.FromMilliseconds(Math.Max(cfg.MaxLatency * 60, 30_000));
var cleanupTimer = new Timer(_ => server.CleanupStale(timeout),
    null, TimeSpan.FromSeconds(10), TimeSpan.FromSeconds(10));

// Console input loop — keeps prompt at bottom, logs above
var inputTask = Task.Run(() =>
{
    int _logRow = Console.CursorTop;
    object _logLock = new();
    Action<string> onLog = msg =>
    {
        lock (_logLock)
        {
            int left = Console.CursorLeft;
            Console.SetCursorPosition(0, _logRow);
            Console.Write(new string(' ', Console.WindowWidth - 1));
            Console.SetCursorPosition(0, _logRow);
            Console.WriteLine(msg);
            _logRow = Console.CursorTop;
            Console.SetCursorPosition(0, _logRow);
            Console.Write("> ");
            if (left > 2) Console.SetCursorPosition(2 + left - 2, _logRow);
        }
    };

    // Hook server logs to display above prompt
    server.OnLog += onLog;

    while (!cts.Token.IsCancellationRequested)
    {
        Console.SetCursorPosition(0, _logRow);
        Console.Write("> " + new string(' ', 60));
        Console.SetCursorPosition(2, _logRow);

        // Custom input with Tab-completion
        string input = "";
        int cursorPos = 0;
        while (true)
        {
            var key = Console.ReadKey(true);
            if (key.Key == ConsoleKey.Enter)
            {
                Console.WriteLine();
                break;
            }
            else if (key.Key == ConsoleKey.Tab)
            {
                string[] suggestions = GetSuggestions(input.TrimStart());
                if (suggestions.Length == 1)
                {
                    string completion = suggestions[0];
                    if (completion.StartsWith(input.TrimStart(), StringComparison.OrdinalIgnoreCase))
                    {
                        input = completion + " ";
                        cursorPos = input.Length;
                    }
                }
                else if (suggestions.Length > 1)
                {
                    string prefix = input.TrimStart();
                    _logRow = Console.CursorTop;
                    Console.WriteLine();
                    Console.ForegroundColor = ConsoleColor.DarkGray;
                    foreach (var s in suggestions)
                        Console.WriteLine("  " + s);
                    Console.ResetColor();
                    _logRow = Console.CursorTop;
                    Console.Write("> " + input);
                    cursorPos = input.Length;
                }
            }
            else if (key.Key == ConsoleKey.Backspace && cursorPos > 0)
            {
                input = input.Remove(cursorPos - 1, 1);
                cursorPos--;
            }
            else if (key.Key == ConsoleKey.LeftArrow && cursorPos > 0)
            {
                cursorPos--;
            }
            else if (key.Key == ConsoleKey.RightArrow && cursorPos < input.Length)
            {
                cursorPos++;
            }
            else if (key.KeyChar >= ' ')
            {
                input = input.Insert(cursorPos, key.KeyChar.ToString());
                cursorPos++;
            }
            // Redraw
            Console.SetCursorPosition(0, _logRow);
            Console.Write("> " + input + "  ");
            Console.SetCursorPosition(2 + cursorPos, _logRow);
        }
        Console.SetCursorPosition(0, _logRow);
        string? line = input;
        _logRow = Console.CursorTop;
        ProcessCommand(server, line.Trim());
    }
});

await inputTask;
cleanupTimer.Dispose();
server.Stop();
await UPnPHelper.ClosePortAsync();
Console.WriteLine("Server stopped.");

static string[] GetSuggestions(string input)
{
    string[] commands = {"/help","/players","/op","/deop","/kick","/weather","/time","/say","/tp","/clear","/pcmd","/plugins"};
    if (string.IsNullOrEmpty(input)) return commands;
    var matches = commands.Where(c => c.StartsWith(input, StringComparison.OrdinalIgnoreCase)).ToArray();
    if (matches.Length > 0) return matches;
    // Partial match
    return commands.Where(c => c.Contains(input, StringComparison.OrdinalIgnoreCase)).ToArray();
}

static string[] SplitPluginEntries(string raw)
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

static void ProcessCommand(CoopServer server, string cmd)
{
    if (string.IsNullOrEmpty(cmd)) return;
    
    string[] parts = cmd.Split(' ', StringSplitOptions.RemoveEmptyEntries);
    string command = parts[0].ToLower();
    
    switch (command)
    {
        case "/":
            Console.WriteLine("=== COMMANDS ===");
            Console.WriteLine("/players /op /deop /kick /weather /time /say /tp /clear /help");
            break;

        case "/help":
        case "help":
            Console.WriteLine("=== COMMANDS ===");
            Console.WriteLine("/players       - List connected players");
            Console.WriteLine("/op <name>     - Grant admin");
            Console.WriteLine("/deop <name>   - Revoke admin");
            Console.WriteLine("/kick <name>   - Kick player");
            Console.WriteLine("/weather <type>- Change weather");
            Console.WriteLine("/time <h> <m>  - Change time");
            Console.WriteLine("/say <msg>     - Broadcast message");
            Console.WriteLine("/tp <dest> [src] - Teleport player(s)");
            Console.WriteLine("/clear         - Clear console");
            break;

        case "/tp":
        case "tp":
            if (parts.Length < 2)
            {
                ShowPlayerList(server);
                Console.WriteLine("Usage: /tp <dest> [source]");
                Console.WriteLine("  /tp Diego        -> everyone to Diego");
                Console.WriteLine("  /tp Diego PC05   -> only PC05 to Diego");
                break;
            }
            server.TeleportPlayer(parts[1], parts.Length > 2 ? parts[2] : "");
            break;

        case "/op":
        case "op":
            if (parts.Length < 2) { ShowPlayerList(server); Console.WriteLine("Usage: /op <name>"); break; }
            server.SetAdmin(parts[1], true);
            break;
        case "/deop":
        case "deop":
            if (parts.Length < 2) { ShowPlayerList(server); Console.WriteLine("Usage: /deop <name>"); break; }
            server.SetAdmin(parts[1], false);
            break;
            
        case "/kick":
        case "kick":
            if (parts.Length < 2) { ShowPlayerList(server); Console.WriteLine("Usage: /kick <name>"); break; }
            server.KickPlayer(string.Join(" ", parts.Skip(1)));
            break;
            
        case "/players":
        case "players":
            ShowPlayerList(server);
            break;
            
        case "/pcmd":
        case "pcmd":
            if (parts.Length < 3) {
                Console.WriteLine("Usage: /pcmd <player|all> <command> [args]");
                Console.WriteLine("  /pcmd all wlock rain");
                Console.WriteLine("  /pcmd PC05 tlock 15 30");
                // Show available commands from all connected players
                var allPlayers = server.GetPlayers();
                var seen = new HashSet<string>();
                Console.WriteLine("\nAvailable plugin commands:");
                foreach (var pl in allPlayers)
                {
                    if (string.IsNullOrEmpty(pl.PluginRaw)) continue;
                    foreach (var entry in SplitPluginEntries(pl.PluginRaw))
                    {
                        var name = entry.Contains('[') ? entry[..entry.IndexOf('[')] : entry;
                        var cmds = entry.Contains('[') ? entry[(entry.IndexOf('[')+1)..].TrimEnd(']') : "";
                        if (!string.IsNullOrEmpty(cmds))
                        {
                            foreach (var c in cmds.Split(','))
                            {
                                var key = $"{c.Trim()} ({name.Trim()})";
                                if (seen.Add(key)) Console.WriteLine($"  /pcmd <player> {c.Trim()}");
                            }
                        }
                    }
                }
                break;
            }
            {
                string target = parts[1];
                string pcmd   = parts[2];
                string pargs  = parts.Length > 3 ? string.Join(" ", parts.Skip(3)) : "";
                if (target.Equals("all", StringComparison.OrdinalIgnoreCase))
                    server.BroadcastPluginCommand(pcmd, pargs);
                else
                {
                    var player = server.GetPlayers().FirstOrDefault(p =>
                        p.Username.StartsWith(target, StringComparison.OrdinalIgnoreCase));
                    if (player == null) Console.WriteLine("Player not found.");
                    else server.SendPluginCommand(player, pcmd, pargs);
                }
            }
            break;

        case "/plugins":
        case "plugins":
            if (parts.Length < 2)
            {
                var players = server.GetPlayers();
                Console.WriteLine("Usage: /plugins <name>");
                Console.WriteLine($"Players with plugins: {players.Count(p => p.Plugins.Length > 0)}");
                break;
            }
            {
                var target = server.GetPlayers().FirstOrDefault(p =>
                    p.Username.StartsWith(parts[1], StringComparison.OrdinalIgnoreCase));
                if (target == null) { Console.WriteLine("Player not found."); break; }
                if (target.Plugins.Length == 0)
                    Console.WriteLine($"{target.Username}: no plugins");
                else
                {
                    Console.WriteLine($"{target.Username} plugins ({target.Plugins.Length}):");
                    if (!string.IsNullOrEmpty(target.PluginRaw))
                    {
                        foreach (var entry in SplitPluginEntries(target.PluginRaw))
                        {
                            var name = entry.Contains('[') ? entry[..entry.IndexOf('[')] : entry;
                            var cmds = entry.Contains('[') ? entry[(entry.IndexOf('[')+1)..].TrimEnd(']') : "";
                            Console.WriteLine($"  {name}" + (string.IsNullOrEmpty(cmds) ? "" : $" -> {cmds}"));
                        }
                    }
                }
            }
            break;
            
        case "/weather":
        case "weather":
            if (parts.Length < 2)
            {
                Console.WriteLine("Usage: /weather <type>");
                Console.WriteLine("Types: CLEAR EXTRASUNNY SUNNY OVERCAST RAIN DRIZZLE THUNDER THUNDERSTORM FOG MISTY SNOW SNOWLIGHT BLIZZARD GROUNDBLIZZARD HURRICANE HIGHPRESSURE SANDSTORM SMOG CLOUDS");
                break;
            }
            server.SetWeatherByName(parts[1]);
            break;
            
        case "/time":
        case "time":
            if (parts.Length < 2) { Console.WriteLine("Usage: /time <hour> [minute] (0-23)"); break; }
            if (int.TryParse(parts[1], out int hour))
            {
                int minute = parts.Length > 2 && int.TryParse(parts[2], out int m) ? m : 0;
                server.SetTime(hour, minute);
            }
            else Console.WriteLine("Invalid hour.");
            break;
            
        case "/say":
        case "say":
            if (parts.Length < 2) { Console.WriteLine("Usage: /say <message>"); break; }
            server.BroadcastMessage(string.Join(" ", parts.Skip(1)));
            break;
            
        case "/clear":
        case "clear":
            Console.Clear();
            Console.WriteLine("=== CONSOLE CLEARED ===");
            break;
            
        default:
            Console.WriteLine($"Unknown: {command}. Type / for commands.");
            break;
    }
}

static void ShowPlayerList(CoopServer server)
{
    var players = server.GetPlayers();
    Console.WriteLine($"=== PLAYERS ({players.Count}) ===");
    foreach (var p in players)
        Console.WriteLine($"  [{p.Id}] {p.Username}{(p.IsAdmin ? " [ADMIN]" : "")}");
    if (players.Count == 0)
        Console.WriteLine("  No players connected.");
}