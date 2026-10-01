using System.IO;
using System.Text;
using static RDR2Coop.Core.Packets.PlayerConnectPacket;

namespace RDR2Coop.Core.Packets
{
    // Wire layout (matches C++ packets.h):
    //   [type:1][playerId:int32][username: uint8 len + UTF8]
    public class PlayerDisconnectPacket
    {
        public int    PlayerId { get; set; }
        public string Username { get; set; } = "";

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write((byte)PacketType.PlayerDisconnect);
            bw.Write(PlayerId);
            WriteStr(bw, Username);
            return ms.ToArray();
        }

        public static PlayerDisconnectPacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms);
            br.ReadByte(); // type
            return new PlayerDisconnectPacket
            {
                PlayerId = br.ReadInt32(),
                Username = ReadStr(br)
            };
        }
    }
}
