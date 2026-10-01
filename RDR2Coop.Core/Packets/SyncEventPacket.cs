using System;
using System.IO;
using System.Text;

namespace RDR2Coop.Core.Packets
{
    // Wire layout: [type:1][eventType:1][victimId:4][attackerId:4][extraData:4]
    // eventos instantáneos como matanza, golpe, robo de caballo
    public class SyncEventPacket
    {
        public SyncEventType EventType { get; set; }
        public int VictimId { get; set; }     // quién recibió el evento
        public int AttackerId { get; set; }    // quién causó el evento
        public uint ExtraData { get; set; }   // datos extra (weapon hash, etc)

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms, Encoding.UTF8, false);
            bw.Write((byte)PacketType.SyncEvent);
            bw.Write((byte)EventType);
            bw.Write(VictimId);
            bw.Write(AttackerId);
            bw.Write(ExtraData);
            return ms.ToArray();
        }

        public static SyncEventPacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms, Encoding.UTF8, false);
            br.ReadByte(); // type
            return new SyncEventPacket
            {
                EventType = (SyncEventType)br.ReadByte(),
                VictimId = br.ReadInt32(),
                AttackerId = br.ReadInt32(),
                ExtraData = br.ReadUInt32()
            };
        }
    }
}