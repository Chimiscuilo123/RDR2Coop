using System;
using System.Text;

namespace RDR2Coop.Core.Packets
{
    public class PluginCommandPacket
    {
        public string Command = "";
        public string Args = "";

        public byte[] Serialize()
        {
            byte[] cmdBytes = Encoding.UTF8.GetBytes(Command);
            byte[] argBytes = Encoding.UTF8.GetBytes(Args);
            byte[] buf = new byte[3 + cmdBytes.Length + argBytes.Length];
            buf[0] = (byte)PacketType.PluginCommand;
            buf[1] = (byte)Math.Min(cmdBytes.Length, 63);
            Array.Copy(cmdBytes, 0, buf, 2, buf[1]);
            int off = 2 + buf[1];
            buf[off] = (byte)Math.Min(argBytes.Length, 127);
            Array.Copy(argBytes, 0, buf, off + 1, buf[off]);
            return buf;
        }

        public static PluginCommandPacket Deserialize(byte[] data)
        {
            var p = new PluginCommandPacket();
            if (data.Length < 3) return p;
            int cmdLen = data[1];
            if (cmdLen > 63) cmdLen = 63;
            p.Command = Encoding.UTF8.GetString(data, 2, cmdLen);
            int off = 2 + cmdLen;
            if (off < data.Length)
            {
                int argLen = data[off];
                if (argLen > 127) argLen = 127;
                if (off + 1 + argLen <= data.Length)
                    p.Args = Encoding.UTF8.GetString(data, off + 1, argLen);
            }
            return p;
        }
    }
}
