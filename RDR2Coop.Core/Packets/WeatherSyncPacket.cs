using System;
using System.IO;
using System.Text;

namespace RDR2Coop.Core.Packets
{
    // Wire layout: [type:1][weatherType:4][transitionTime:4][hour:1][minute:1][second:1]
    public class WeatherSyncPacket
    {
        public uint WeatherType { get; set; }       // hash of weather type
        public float TransitionTime { get; set; } = 5.0f; // time to transition
        public int Hour { get; set; } = 12;          // hour (0-23)
        public int Minute { get; set; } = 0;         // minute (0-59)
        public int Second { get; set; } = 0;         // second (0-59)

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms, Encoding.UTF8, false);
            bw.Write((byte)PacketType.WeatherSync);
            bw.Write(WeatherType);
            bw.Write(TransitionTime);
            bw.Write((byte)Hour);
            bw.Write((byte)Minute);
            bw.Write((byte)Second);
            return ms.ToArray();
        }

        public static WeatherSyncPacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms, Encoding.UTF8, false);
            br.ReadByte(); // type
            var packet = new WeatherSyncPacket
            {
                WeatherType = br.ReadUInt32(),
                TransitionTime = br.ReadSingle()
            };
            if (ms.Position < ms.Length) packet.Hour = br.ReadByte();
            if (ms.Position < ms.Length) packet.Minute = br.ReadByte();
            if (ms.Position < ms.Length) packet.Second = br.ReadByte();
            return packet;
        }
    }
}