using System;
using System.Text;

namespace RDR2Coop.Core.Packets
{
    public class PluginInfoPacket
    {
        public int PlayerId;
        public string RawNames = "";

        public byte[] Serialize()
        {
            byte[] nameBytes = Encoding.UTF8.GetBytes(RawNames);
            byte nameLen = (byte)Math.Min(nameBytes.Length, 199);
            byte[] buf = new byte[6 + nameLen];
            buf[0] = (byte)PacketType.PluginInfo;
            BitConverter.GetBytes(PlayerId).CopyTo(buf, 1);
            buf[5] = nameLen;
            Array.Copy(nameBytes, 0, buf, 6, nameLen);
            return buf;
        }

        public void Deserialize(byte[] buf, int len)
        {
            if (len < 6) return;
            PlayerId = BitConverter.ToInt32(buf, 1);
            int nameLen = buf[5];
            if (nameLen > 0 && len >= 6 + nameLen)
                RawNames = Encoding.UTF8.GetString(buf, 6, nameLen);
        }
    }
}
