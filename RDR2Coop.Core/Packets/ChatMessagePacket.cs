using System.IO;
using System.Text;
using static RDR2Coop.Core.Packets.PlayerConnectPacket;

namespace RDR2Coop.Core.Packets
{
    // Wire layout (matches C++ packets.h):
    //   [type:1][username: uint8 len + UTF8][message: uint8 len + UTF8]
    public class ChatMessagePacket
    {
        public string Username { get; set; } = "";
        public string Message  { get; set; } = "";

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write((byte)PacketType.ChatMessage);
            WriteStr(bw, Username);
            WriteStr(bw, Message);
            return ms.ToArray();
        }

        public static ChatMessagePacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms);
            br.ReadByte(); // type
            return new ChatMessagePacket
            {
                Username = ReadStr(br),
                Message  = ReadStr(br)
            };
        }
    }
}
