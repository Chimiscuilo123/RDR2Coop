using System;
using System.IO;
using System.Text;

namespace RDR2Coop.Core.Packets
{
    // Wire layout (matches C++ packets.h):
    //   [type:1][username: uint8 len + UTF8 bytes]
    public class PlayerConnectPacket
    {
        public string Username { get; set; } = "";

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write((byte)PacketType.PlayerConnect);
            WriteStr(bw, Username);
            return ms.ToArray();
        }

        public static PlayerConnectPacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms);
            br.ReadByte(); // PacketType
            return new PlayerConnectPacket { Username = ReadStr(br) };
        }

        // ── helpers matching C++ WriteString / ReadString ──────────────────────
        internal static void WriteStr(BinaryWriter bw, string s)
        {
            var bytes = Encoding.UTF8.GetBytes(s);
            byte len  = (byte)Math.Min(bytes.Length, 255);
            bw.Write(len);
            bw.Write(bytes, 0, len);
        }

        internal static string ReadStr(BinaryReader br)
        {
            byte len = br.ReadByte();
            return Encoding.UTF8.GetString(br.ReadBytes(len));
        }
    }
}
