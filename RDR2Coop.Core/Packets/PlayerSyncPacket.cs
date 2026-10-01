using System;
using System.IO;
using System.Text;
using static RDR2Coop.Core.Packets.PlayerConnectPacket; // reuse WriteStr/ReadStr

namespace RDR2Coop.Core.Packets
{
    // Wire layout (matches C++ packets.h exactly):
    //   [type:1][playerId:int32][x:float][y:float][z:float]
    //   [heading:float][health:float][speed:byte][flags:byte]
    //   [weaponHash:uint32][modelHash:uint32][horseModelHash:uint32][outfitHash:uint32][horseOutfitHash:uint32]
    //   [wantedLevel:int32][passengerOfId:byte][username: uint8 len + UTF8 bytes]
    //
    // flags bits: bit0=onHorse, bit1=aiming, bit2=shooting, bit3=dead, bit4=isPassenger
    public class PlayerSyncPacket
    {
        public int    PlayerId   { get; set; }
        public string Username   { get; set; } = "";

        public float X       { get; set; }
        public float Y       { get; set; }
        public float Z       { get; set; }
        public float Heading { get; set; }
        public float Health  { get; set; }

        public byte   Speed          { get; set; }
        public uint   WeaponHash     { get; set; }
        public uint   ModelHash      { get; set; }
        public uint   HorseModelHash { get; set; }
        public uint   OutfitHash     { get; set; }
        public uint   HorseOutfitHash{ get; set; }
        public int    WantedLevel    { get; set; }
        public byte   PassengerOfId  { get; set; }   // player ID of horse owner (0=none)

        public bool IsOnHorse   { get; set; }
        public bool IsAiming    { get; set; }
        public bool IsShooting  { get; set; }
        public bool IsDead      { get; set; }
        public bool IsPassenger { get; set; }

        private byte Flags
        {
            get
            {
                byte f = 0;
                if (IsOnHorse)   f |= 0x01;
                if (IsAiming)    f |= 0x02;
                if (IsShooting)  f |= 0x04;
                if (IsDead)      f |= 0x08;
                if (IsPassenger) f |= 0x10;
                return f;
            }
        }

        public byte[] Serialize()
        {
            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms, Encoding.UTF8, false);
            bw.Write((byte)PacketType.PlayerSync);
            bw.Write(PlayerId);
            bw.Write(X); bw.Write(Y); bw.Write(Z);
            bw.Write(Heading);
            bw.Write(Health);
            bw.Write(Speed);
            bw.Write(Flags);
            bw.Write(WeaponHash);
            bw.Write(ModelHash);
            bw.Write(HorseModelHash);
            bw.Write(OutfitHash);
            bw.Write(HorseOutfitHash);
            bw.Write(WantedLevel);
            bw.Write(PassengerOfId);
            WriteStr(bw, Username);
            return ms.ToArray();
        }

        public static PlayerSyncPacket Deserialize(byte[] data)
        {
            using var ms = new MemoryStream(data);
            using var br = new BinaryReader(ms, Encoding.UTF8, false);
            br.ReadByte(); // type

            var p = new PlayerSyncPacket
            {
                PlayerId = br.ReadInt32(),
                X        = br.ReadSingle(),
                Y        = br.ReadSingle(),
                Z        = br.ReadSingle(),
                Heading  = br.ReadSingle(),
                Health   = br.ReadSingle(),
                Speed    = br.ReadByte(),
            };

            byte flags    = br.ReadByte();
            p.IsOnHorse   = (flags & 0x01) != 0;
            p.IsAiming    = (flags & 0x02) != 0;
            p.IsShooting  = (flags & 0x04) != 0;
            p.IsDead      = (flags & 0x08) != 0;
            p.IsPassenger = (flags & 0x10) != 0;
            p.WeaponHash      = br.ReadUInt32();
            p.ModelHash       = br.ReadUInt32();
            p.HorseModelHash  = br.ReadUInt32();
            p.OutfitHash      = ms.Position < ms.Length ? br.ReadUInt32() : 0;
            p.HorseOutfitHash = ms.Position < ms.Length ? br.ReadUInt32() : 0;
            p.WantedLevel     = ms.Position < ms.Length ? br.ReadInt32() : 0;
            p.PassengerOfId   = ms.Position < ms.Length ? br.ReadByte() : (byte)0;
            p.Username        = ReadStr(br);
            return p;
        }
    }
}
