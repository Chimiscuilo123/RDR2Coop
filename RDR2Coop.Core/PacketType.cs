namespace RDR2Coop.Core
{
    public enum PacketType : byte
    {
        PlayerConnect    = 1,
        PlayerDisconnect = 2,
        PlayerSync       = 3,
        ChatMessage      = 4,
        ServerInfo       = 5,
        ServerAdvertise  = 6,
        DiscoverRequest  = 7,
        EntitySync       = 8,
        WeatherSync      = 9,
        SyncEvent        = 10,
        ClothingSync     = 11,
        PluginInfo       = 12,
        PluginCommand    = 13,  // Server → client: relay plugin commands from console
    }

    // Tipos de eventos de sincronización
    public enum SyncEventType : byte
    {
        PedKilled     = 1,
        PedHit        = 2,
        HorseStolen   = 3,
        Shot          = 4,
        EntityKilled  = 5,
    }
}
