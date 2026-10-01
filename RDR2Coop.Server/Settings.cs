using System;
using System.IO;
using System.Xml.Serialization;

namespace RDR2Coop.Server
{
    [XmlRoot("Settings")]
    public class Settings
    {
        // ── Network ───────────────────────────────────────────────────────────
        public int    Port       { get; set; } = 7777;
        public int    MaxPlayers { get; set; } = 16;

        /// <summary>Kick players whose last packet is older than this (ms).</summary>
        public int    MaxLatency { get; set; } = 500;

        // ── Identity ──────────────────────────────────────────────────────────
        public string Name           { get; set; } = "%computername% RDR2 Server";
        public string Description    { get; set; } = "A Red Dead Redemption 2 co-op server";
        public string WelcomeMessage { get; set; } = "Welcome, cowboy!";
        public string GameMode       { get; set; } = "FreeRoam";
        public string Language       { get; set; } = "English";
        public bool   LocalHostIsAdmin { get; set; } = false;   // first local client = admin

        // ── Behaviour ─────────────────────────────────────────────────────────
        /// <summary>0 = info, 1 = verbose, 2 = debug</summary>
        public int  LogLevel      { get; set; } = 0;
        public bool KickGodMode   { get; set; } = false;
        public bool KickSpamming  { get; set; } = true;
        public int  SpamLimit     { get; set; } = 100;

        public string AllowedUsernameChars { get; set; } =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890-_";

        // ── Streaming ─────────────────────────────────────────────────────────
        /// <summary>Distance (units) at which remote players are synced. -1 = unlimited.</summary>
        public float PlayerStreamingDistance { get; set; } = -1;

        // ── Constants ─────────────────────────────────────────────────────────
        private static readonly XmlSerializer _xml = new(typeof(Settings));
        private const string DefaultPath = "Settings.xml";

        // ── Load / Save ───────────────────────────────────────────────────────

        /// <summary>
        /// Loads Settings.xml next to the executable.
        /// If the file does not exist, writes a default one and returns defaults.
        /// </summary>
        public static Settings Load(string path = DefaultPath)
        {
            if (!File.Exists(path))
            {
                var def = new Settings();
                def.Save(path);
                Console.WriteLine($"[Config] Settings.xml not found — creating with defaults.");
                return def;
            }

            try
            {
                using var fs = File.OpenRead(path);
                var s = (Settings)_xml.Deserialize(fs)!;
                // Resolve %computername% in server name
                if (s.Name.Contains("%computername%"))
                    s.Name = s.Name.Replace("%computername%", Environment.MachineName);
                return s;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Config] Error leyendo {path}: {ex.Message}");
                Console.WriteLine("[Config] Usando valores por defecto.");
                return new Settings();
            }
        }

        public void Save(string path = DefaultPath)
        {
            try
            {
                var ns = new XmlSerializerNamespaces();
                ns.Add("", ""); // no namespace prefixes
                using var fs = File.Create(path);
                _xml.Serialize(fs, this, ns);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[Config] No se pudo guardar {path}: {ex.Message}");
            }
        }
    }
}
