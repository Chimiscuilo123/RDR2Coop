using System;
using System.Net;

namespace RDR2Coop.Server
{
    public class ConnectedPlayer
    {
        public int         Id       { get; set; }
        public string      Username { get; set; } = "";
        public IPEndPoint  Endpoint { get; set; } = null!;
        public DateTime    LastSeen { get; set; } = DateTime.UtcNow;
        public bool        IsAdmin  { get; set; } = false;
        public string[]    Plugins  { get; set; } = Array.Empty<string>();
        public string      PluginRaw { get; set; } = ""; // raw plugin info from client

        public void Touch() => LastSeen = DateTime.UtcNow;
    }
}
