using System;
using System.IO;
using System.Text;
using static RDR2Coop.Core.Packets.PlayerConnectPacket;

namespace RDR2Coop.Core.Packets
{
    // Wire layout (matches C++ packets.h):
    //   [type:1][port:uint16][players:byte][maxPlayers:byte][name: uint8 len + UTF8]
    public class ServerAdvertisePacket
    {
        public ushort Port       { get; set; } = 7777;
        public byte   Players    { get; set; } = 0;
        public byte   MaxPlayers { get; set; } = 16;
        public string Name       { get; set; } = "";

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);
            bw.Write((byte)PacketType.ServerAdvertise);
            bw.Write(Port);
            bw.Write(Players);
            bw.Write(MaxPlayers);
            WriteStr(bw, Name);
            return ms.ToArray();
        }

        public static ServerAdvertisePacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms);
            br.ReadByte(); // type
            return new ServerAdvertisePacket
            {
                Port       = br.ReadUInt16(),
                Players    = br.ReadByte(),
                MaxPlayers = br.ReadByte(),
                Name       = ReadStr(br),
            };
        }
    }
}
