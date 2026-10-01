using System.IO;

namespace RDR2Coop.Core.Packets
{
    // Sent by server → client right after PlayerConnect is received.
    // Layout matches packets.h in the C++ client:
    //   [type:1][assigned_id:4][player_count:2][max_players:2][server_name:str]
    public class ServerInfoPacket
    {
        public int    AssignedId  { get; set; }
        public ushort PlayerCount { get; set; }
        public ushort MaxPlayers  { get; set; }
        public string ServerName  { get; set; } = "";

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write((byte)PacketType.ServerInfo);
            bw.Write(AssignedId);
            bw.Write(PlayerCount);
            bw.Write(MaxPlayers);
            bw.Write((byte)ServerName.Length);
            bw.Write(System.Text.Encoding.UTF8.GetBytes(ServerName));
            return ms.ToArray();
        }

        public static ServerInfoPacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms);
            br.ReadByte(); // type
            var p = new ServerInfoPacket
            {
                AssignedId  = br.ReadInt32(),
                PlayerCount = br.ReadUInt16(),
                MaxPlayers  = br.ReadUInt16(),
            };
            byte nameLen = br.ReadByte();
            p.ServerName = System.Text.Encoding.UTF8.GetString(br.ReadBytes(nameLen));
            return p;
        }
    }
}
