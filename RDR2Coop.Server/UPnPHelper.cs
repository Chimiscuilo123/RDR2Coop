using System;
using System.Threading;
using System.Threading.Tasks;
using Open.Nat;

namespace RDR2Coop.Server
{
    /// <summary>
    /// Wraps Open.NAT to open and close a UDP port via UPnP/NAT-PMP.
    /// Fails silently when the router does not support UPnP.
    /// </summary>
    internal static class UPnPHelper
    {
        private static NatDevice? _device;
        private static Mapping?   _mapping;

        /// <summary>
        /// Tries to open <paramref name="port"/> UDP on the router via UPnP.
        /// Returns true when the mapping was created successfully.
        /// </summary>
        public static async Task<bool> OpenPortAsync(int port,
            string description = "RDR2 Coop Server",
            int timeoutMs = 5_000)
        {
            try
            {
                var discoverer = new NatDiscoverer();
                using var cts  = new CancellationTokenSource(timeoutMs);

                _device = await discoverer.DiscoverDeviceAsync(PortMapper.Upnp, cts);

                _mapping = new Mapping(Protocol.Udp, port, port, 0, description);
                await _device.CreatePortMapAsync(_mapping);

                var ext = await _device.GetExternalIPAsync();
                Console.WriteLine($"[UPnP] Puerto UDP {port} abierto. IP pública: {ext}");
                return true;
            }
            catch (NatDeviceNotFoundException)
            {
                Console.WriteLine("[UPnP] No se encontró router UPnP. Abre el puerto manualmente si juegas por internet.");
                return false;
            }
            catch (OperationCanceledException)
            {
                Console.WriteLine("[UPnP] Tiempo de espera agotado buscando router UPnP.");
                return false;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[UPnP] Error: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// Removes the port mapping created by <see cref="OpenPortAsync"/>.
        /// </summary>
        public static async Task ClosePortAsync()
        {
            if (_device == null || _mapping == null) return;
            try
            {
                await _device.DeletePortMapAsync(_mapping);
                Console.WriteLine($"[UPnP] Puerto {_mapping.PublicPort} UDP cerrado.");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[UPnP] No se pudo cerrar el puerto: {ex.Message}");
            }
        }
    }
}
