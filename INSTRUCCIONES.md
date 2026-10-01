# RDR2Coop - Guia de Desarrollo

## Estructura del proyecto

```
RDR2Coop/
├── RDR2Coop.Client.Cpp/     # Cliente C++ (ScriptHookRDR2)
│   ├── main.cpp              # Punto de entrada, loop principal, sync
│   ├── packets.h             # Definicion de paquetes de red
│   ├── network.h             # Cliente UDP
│   ├── script.h              # Wrapper de ScriptHookRDR2
│   ├── keyboard.h            # Input de teclado
│   ├── clothes_db.h          # DB de ropa (11,671 hashes)
│   └── ui/                   # Interfaz de usuario
│       ├── draw.h            # Primitivas de dibujo (sprites, texto)
│       ├── menu.h            # Menu nativo RDR2
│       ├── chat.h            # Chat
│       ├── playerlist.h      # Lista de jugadores
│       ├── statusbar.h       # Barra de estado
│       ├── lanscanner.h      # Escaner LAN
│       └── notification.h    # Notificaciones
├── RDR2Coop.Core/            # Libreria compartida C# (paquetes)
│   ├── PacketType.cs
│   └── Packets/
├── RDR2Coop.Server/          # Servidor C#
│   ├── CoopServer.cs
│   └── Program.cs
├── build_cliente.bat         # Compilar cliente
├── build_servidor.bat        # Compilar servidor
├── build_todo.bat            # Compilar todo
└── dist/                     # Binarios compilados
    ├── Client/
    │   ├── RDR2Coop.asi
    │   └── RDR2Coop/
    └── Server/
```

---

## Jugador (RDR2)

```
RDR2/
├── RDR2Coop.asi              # El mod (raiz)
└── RDR2Coop/                 # Carpeta del mod
    ├── RDR2Coop.ini          # Configuracion
    ├── plugins/              # Plugins .dll (futuro)
    ├── scripts/              # Scripts .lua (futuro)
    └── logs/                 # Logs
        └── RDR2Coop_debug.log
```

---

# COMO CREAR PLUGINS (.DLL)

Los plugins son DLLs que se cargan desde `RDR2Coop/plugins/`.
El mod principal escanea la carpeta y carga automaticamente cada .dll.

## Interfaz del Plugin

Cada plugin debe exportar 3 funciones:

```c
// plugins/mi_plugin/mi_plugin.cpp

#include <windows.h>

// Se llama UNA VEZ al cargar el plugin
// Recibe el handle del ped local y el username
extern "C" __declspec(dllexport) void PluginInit(int localPed, const char* username)
{
    // Inicializar tu plugin aqui
    // Ej: cargar config, spawnear NPCs, registrar eventos
}

// Se llama CADA FRAME (60 veces/segundo)
// Recibe el handle del ped local
extern "C" __declspec(dllexport) void PluginTick(int localPed)
{
    // Logica por frame
    // Ej: verificar condiciones, mover entidades, spawnear
}

// Se llama AL CERRAR el plugin (desconexion o cierre del juego)
extern "C" __declspec(dllexport) void PluginShutdown()
{
    // Limpiar recursos
    // Ej: borrar entidades creadas, guardar estado
}
```

## Ejemplo: Plugin de Zombies

```c
// plugins/rdr2coop_zombies/zombies.cpp

#include <windows.h>
#include <vector>

static std::vector<int> g_zombies;

extern "C" __declspec(dllexport) void PluginInit(int localPed, const char* username)
{
    // Spawnear 5 zombies cerca del jugador
    for (int i = 0; i < 5; i++)
    {
        // CREATE_PED(0x..., x, y, z, heading, ...)
        // g_zombies.push_back(handle);
    }
}

extern "C" __declspec(dllexport) void PluginTick(int localPed)
{
    // Mover zombies hacia el jugador
    for (int z : g_zombies)
    {
        // TASK_GO_STRAIGHT_TO_COORD(z, playerX, playerY, playerZ, ...)
    }
}

extern "C" __declspec(dllexport) void PluginShutdown()
{
    // Borrar todos los zombies
    for (int z : g_zombies)
    {
        // DELETE_ENTITY(&z);
    }
    g_zombies.clear();
}
```

## Compilar un Plugin

```batch
:: plugins/rdr2coop_zombies/build.bat
g++ -std=c++17 -shared -o zombies.dll zombies.cpp ^
    -I "../../RDR2Coop.Client.Cpp" ^
    -Wl,-Bstatic -lstdc++ -lgcc_eh -lgcc -lwinpthread ^
    -Wl,-Bdynamic -lws2_32 -Wl,--subsystem,windows -O2
```

## Instalar un Plugin

1. Compilar el .dll
2. Copiarlo a `RDR2Coop/plugins/mi_plugin.dll`
3. El mod lo carga automaticamente al iniciar

---

# COMO CREAR SCRIPTS (.LUA)

Los scripts son archivos .lua en `RDR2Coop/scripts/`.
Se ejecutan con un interprete Lua embebido en el mod.

## API disponible para scripts

```lua
-- scripts/mi_script.lua

-- ===== Jugador =====
local ped = GetLocalPed()           -- handle del ped local
local pos = GetEntityCoords(ped)    -- {x, y, z}
local name = GetUsername()          -- nombre del jugador

-- ===== Red =====
SendChatMessage("Hola mundo!")
IsConnected()                       -- true/false
GetPlayerCount()                    -- numero de jugadores

-- ===== Jugadores remotos =====
local players = GetRemotePlayers()  -- tabla de {id, name, x, y, z}
for _, p in ipairs(players) do
    if Distance(pos, {p.x, p.y, p.z}) < 10 then
        SendChatMessage("Hola " .. p.name .. "!")
    end
end

-- ===== Entidades =====
local horse = CreatePed("A_C_HORSE_KENTUCKYSADDLE_BLACK", pos.x+2, pos.y, pos.z)
SetPedDefaultOutfit(horse)
SetEntityInvincible(horse, true)

-- ===== Notificaciones =====
NotifyAdvanced("Titulo", "Subtitulo", 5000)
NotifyTip("Mensaje inferior", 4000)

-- ===== Eventos =====
-- Registrar handler para cuando un jugador se conecta
OnPlayerConnect(function(playerId, username)
    NotifyTip(username .. " se conecto!", 3000)
end)

-- Registrar handler para cada frame
OnTick(function()
    -- Logica por frame
end)
```

## Ejemplo: Script de Bienvenida

```lua
-- scripts/welcome.lua

OnPlayerConnect(function(id, name)
    NotifyAdvanced("Bienvenido!", name .. " entro al servidor", 6000)
    SendChatMessage("Bienvenido " .. name .. "!")
end)

OnPlayerDisconnect(function(id, name)
    NotifyTip(name .. " se fue", 3000)
end)
```

## Ejemplo: Script de Mision

```lua
-- scripts/bounty_hunt.lua

local active = false
local target = nil
local reward = 0

function StartBounty(playerName, amount)
    -- Buscar al jugador objetivo
    local players = GetRemotePlayers()
    for _, p in ipairs(players) do
        if p.name == playerName then
            target = p
            reward = amount
            active = true
            NotifyObjective("Cazar a " .. playerName .. " - $" .. amount, 0)
            break
        end
    end
end

OnTick(function()
    if not active or not target then return end
    
    local ped = GetLocalPed()
    local pos = GetEntityCoords(ped)
    local dist = Distance(pos, {target.x, target.y, target.z})
    
    if dist < 3 then
        -- Objetivo alcanzado!
        AddMoney(reward)
        NotifyAdvanced("Mision completada!", "Ganaste $" .. reward, 5000)
        active = false
    end
end)

-- Comando de chat: /bounty Nombre 100
OnChatCommand("/bounty", function(args)
    StartBounty(args[1], tonumber(args[2]) or 0)
end)
```

---

# COMO AGREGAR FUNCIONALIDAD AL MOD PRINCIPAL

## Agregar un nuevo paquete de red

1. Definir el tipo en `packets.h`:

```cpp
// En PacketType (packets.h)
enum class PacketType : uint8_t {
    // ... existentes ...
    MyNewPacket = 15,  // Nuevo tipo
};
```

2. Crear la estructura del paquete:

```cpp
// En packets.h o un nuevo archivo
struct MyNewPacket {
    int32_t playerId = 0;
    float someValue = 0;
    
    int Serialize(uint8_t* buf, int maxLen) const { ... }
    static MyNewPacket Deserialize(const uint8_t* buf, int len) { ... }
};
```

3. Enviar desde el cliente (`main.cpp`):

```cpp
MyNewPacket pk;
pk.playerId = g_net.localId;
pk.someValue = 42.0f;
g_net.SendRaw(pk.Serialize());  // Necesitas agregar SendRaw a network.h
```

4. Recibir en el servidor (`CoopServer.cs`):

```csharp
case PacketType.MyNewPacket:
    HandleMyPacket(key, data);
    break;
```

## Agregar un nuevo estado de jugador (flag)

1. Agregar el define del native en `main.cpp`:

```cpp
#define IS_PED_SWIMMING  0x9DE327631295B4C2ULL
```

2. Agregar getter/setter en `packets.h` (si hay bits libres en flags/flags2):

```cpp
// En PlayerSyncPacket
bool IsSwimming() const { return (flags2 & 0x20) != 0; }
void SetSwim(bool v) { if(v) flags2|=0x20; else flags2&=~0x20; }
```

3. Agregar campo en `RemotePlayer` (main.cpp):

```cpp
bool isSwimming = false;
```

4. En `SendSync()` detectar estado:

```cpp
pk.SetSwim(invoke<bool>(IS_PED_SWIMMING, ped));
```

5. En `OnPlayerSync()` almacenar:

```cpp
r.isSwimming = p.IsSwimming();
```

6. En `UpdateRemotes()` aplicar:

```cpp
// Si el remoto esta nadando, aplicar animacion o logica
```

## Compilar

```batch
.\build_cliente.bat    # Solo cliente
.\build_servidor.bat   # Solo servidor
.\build_todo.bat       # Ambos
```

---

# NATIVES UTILES DE RDR2

Ver `natives.h` para la lista completa. Algunos utiles:

| Categoria | Native |
|-----------|--------|
| Entidad | `DOES_ENTITY_EXIST`, `DELETE_ENTITY`, `SET_ENTITY_COORDS` |
| Ped | `CREATE_PED`, `IS_PED_DEAD_OR_DYING`, `APPLY_DAMAGE_TO_PED` |
| Jugador | `GET_PLAYER_PED`, `GET_PLAYER_WANTED_LEVEL` |
| Caballo | `GET_MOUNT`, `SET_PED_ONTO_MOUNT`, `REMOVE_PED_FROM_MOUNT` |
| Arma | `GIVE_WEAPON_TO_PED`, `GET_CURRENT_PED_WEAPON` |
| Ropa | `_APPLY_SHOP_ITEM_TO_PED`, `_GET_PED_META_OUTFIT_HASH` |
| Clima | `SET_WEATHER_TYPE`, `SET_CLOCK_TIME` |
| Tareas | `TASK_GO_STRAIGHT_TO_COORD`, `TASK_MELEE`, `TASK_JUMP` |

---

# DEPURACION

1. Activar logs: `RDR2Coop/RDR2Coop.ini` → logs van a `RDR2Coop/logs/`
2. El archivo `RDR2Coop_debug.log` muestra TODOS los eventos
3. Comandos de chat utiles:
   - `/pos` - coordenadas actuales
   - `/tp nombre` - teletransportarse
   - `/testnotify` - probar notificaciones
