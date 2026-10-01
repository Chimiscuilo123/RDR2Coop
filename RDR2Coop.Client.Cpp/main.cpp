#define NOMINMAX          // Prevent windows.h from defining min/max macros
#include "script.h"
#include "network.h"
#include "packets.h"
#include "ui/menu.h"
#include "ui/chat.h"
#include "ui/playerlist.h"
#include "ui/statusbar.h"
#include "ui/lanscanner.h"
#include "ui/notification.h"
#include "clothes_db.h"
#include "clothes_db2.h"
#include "horse_clothes_db.h"
#include "plugin_system.h"
#include "language.h"
#include "entity_pool.h"
#define WEATHER_EXPORTS
#include "weather_lock.h"
#include "weather_control.h"
#include <windows.h>
#include <cstdio>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <sstream>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Native hashes — slipzdev/ScriptHookRDR2DotNet/tools/NativeHashes.hpp
// Cross-checked with CustomizableTrainer (gudmunduro) for entity/ped natives
// ─────────────────────────────────────────────────────────────────────────────

// ── Player ───────────────────────────────────────────────────────────────────
#define GET_PLAYER_INDEX               0x47E385B0D957C8D4ULL
#define GET_PLAYER_PED                 0x275F255ED201B937ULL
#define DISABLE_ALL_CONTROL_ACTIONS    0x5F4B6931816E599BULL

// ── Entity ───────────────────────────────────────────────────────────────────
#define GET_ENTITY_COORDS              0xA86D5F069399F44DULL
#define GET_ENTITY_HEADING             0xC230DD956E2F5507ULL
#define GET_ENTITY_HEALTH              0x82368787EA73C0F7ULL
#define GET_ENTITY_MAX_HEALTH          0x15D757606D170C3CULL
#define GET_ENTITY_MODEL               0xDA76A9F39210D365ULL
#define DOES_ENTITY_EXIST              0xD42BD6EB2E0F1677ULL
#define DELETE_ENTITY                  0x4CD38C78BD19A497ULL
#define SET_ENTITY_COORDS_NO_OFFSET    0x239A3351AC1DA385ULL
#define SET_ENTITY_HEADING             0xCF2B9C0645C4651BULL
#define SET_ENTITY_HEALTH              0xAC2767ED8BDFAB15ULL
#define SET_ENTITY_INVINCIBLE          0xA5C38736C426FCB8ULL
#define SET_ENTITY_AS_MISSION_ENTITY   0xDC19C288082E586EULL
#define SET_ENTITY_VISIBLE             0x1794B4FCC84D812FULL
#define SET_ENTITY_ALPHA               0x0DF7692B1D9E7BA7ULL
#define RESET_ENTITY_ALPHA             0x744B9EF44779D9ABULL
#define SET_ENTITY_LOD_DIST            0x5FB407F0A7C877BFULL
#define FREEZE_ENTITY_POSITION         0x7D9EFB7AD6B19754ULL

// ── Ped ──────────────────────────────────────────────────────────────────────
#define CREATE_PED                     0xD49F9B0955C367DEULL
#define IS_PED_ON_MOUNT                0x460BC76A0E10655EULL
#define _IS_THIS_MODEL_A_HORSE          0x772A1969F649E902ULL  // (model) -> BOOL
#define IS_PLAYER_FREE_AIMING          0x936F967D4BE1CE9DULL
#define IS_PED_SHOOTING                0x34616828CD07F1A1ULL
#define IS_PED_RUNNING                 0xC5286FFC176F28A2ULL
#define IS_PED_SPRINTING               0x57E457CD2C0FC168ULL
#define IS_PED_WALKING                 0xDE4C184B2B9B071AULL
#define SET_BLOCKING_OF_NON_TEMP_EVENTS    0x9F8AA94D6D97DBF4ULL
// Initialise outfit/textures — without this the ped spawns transparent
#define SET_PED_RANDOM_COMPONENT_VARIATION 0xC8A9481A01E63C28ULL
// Drawable variation system (wardrobe / SET_PED_COMPONENT_VARIATION)
#define GET_PED_DRAWABLE_VARIATION          0x67F3780DD425D4FCULL  // (ped, componentId) -> int
#define GET_PED_TEXTURE_VARIATION           0x04A355E041E004E6ULL  // (ped, componentId) -> int
#define SET_PED_COMPONENT_VARIATION          0x262B14F48D29DE80ULL  // (ped, compId, drawable, texture, palette)

// ── Movement tasks (alloc8or confirmed) ──────────────────────────────────────
// TASK_GO_STRAIGHT_TO_COORD(ped,x,y,z,moveBlendSpeedY,timeBeforeTeleport,finalHeading,targetRadius,p8)
#define TASK_GO_STRAIGHT_TO_COORD          0xD76B57B44F1E6F8BULL
#define TASK_STAND_STILL                   0x919BE13EED931959ULL
#define CLEAR_PED_TASKS                    0xE1EF3C1216AFF2CDULL
#define CREATE_VEHICLE                      0xAF35D0D2583051B0ULL
#define IS_PED_IN_COMBAT                0x4859F1FC66A6278EULL  // (ped, target)
#define IS_PED_HUMAN                    0xB980061DA992779DULL  // (ped) -> BOOL
#define SET_ENTITY_VELOCITY                0x1C99BB7B6E96D16FULL

// ── Aiming / Shooting (alloc8or confirmed) ────────────────────────────────────
// TASK_AIM_GUN_AT_COORD(ped, x,y,z, time, p5, p6)
#define TASK_AIM_GUN_AT_COORD              0x6671F3EEC681BDA1ULL
// TASK_SHOOT_AT_COORD(ped, x,y,z, duration, firingPattern, p6)
#define TASK_SHOOT_AT_COORD                0x46A6CC01E0826106ULL
#define TASK_COMBAT_HATED_TARGETS_AROUND_PED 0x7BF835BB9E2698C8ULL
#define TASK_AIM_GUN_AT_ENTITY              0x9B53BB6E8943AF53ULL
#define TASK_COMBAT_PED                     0xF166E48407BAC484ULL
// Cancels only the secondary (aim/shoot) task without affecting locomotion.
// Hash confirmed for RDR2 (same as GTAV alloc8or).
#define CLEAR_PED_SECONDARY_TASK            0x176CECF6F920D707ULL
#define NETWORK_REGISTER_ENTITY_AS_NETWORKED 0x06FAACD625D80CAAULL
#define NETWORK_HAS_CONTROL_OF_ENTITY        0x01BF60A500E28887ULL

// ── Weather / Time ───────────────────────────────────────────────────────────
#define SET_WEATHER_TYPE                    0x59174F1AFE095B5AULL
#define SET_OVERRIDE_WEATHER                 0xBE83CAE8ED77A94FULL
#define SET_CLOCK_TIME                       0x3A52C59FFB2DEED8ULL
#define CLEAR_OVERRIDE_WEATHER               0x80A398F16FFE3CC3ULL

// ── Death / Damage (alloc8or confirmed) ──────────────────────────────────────
// IS_PED_DEAD_OR_DYING(ped, p1:BOOL) → BOOL
#define IS_PED_DEAD_OR_DYING               0x3317DEDB88C95038ULL
// SET_PED_TO_RAGDOLL(ped, timeMin, timeMax, ragdollType, abortIfInjured, abortIfDead, nmMsg)
#define SET_PED_TO_RAGDOLL                 0xAE99FB955581844AULL
// SET_PED_CAN_RAGDOLL(ped, toggle)
#define SET_PED_CAN_RAGDOLL                0xB128377056A54E2AULL
// RESURRECT_PED(ped)
#define RESURRECT_PED                      0x71BC8E838B9C6035ULL
// APPLY_DAMAGE_TO_PED(ped, damageAmount, damageArmour, boneId, pedKiller)
#define APPLY_DAMAGE_TO_PED                0x697157CED63F18D4ULL

// ── Relationship groups (alloc8or confirmed) ──────────────────────────────────
// ADD_RELATIONSHIP_GROUP(name, groupHash*) → BOOL
#define ADD_RELATIONSHIP_GROUP             0xF372BC22FCB88606ULL
// SET_RELATIONSHIP_BETWEEN_GROUPS(relationship, group1, group2)
// relationship: 0=COMPANION 1=RESPECT 2=LIKE 3=NEUTRAL 4=DISLIKE 5=HATE
#define SET_RELATIONSHIP_BETWEEN_GROUPS    0xBF25EB89375A37ADULL
// SET_PED_RELATIONSHIP_GROUP_HASH(ped, groupHash)
#define SET_PED_RELATIONSHIP_GROUP_HASH    0xC80A74AC829DDD92ULL
// GET_PED_RELATIONSHIP_GROUP_HASH(ped) → Hash
#define GET_PED_RELATIONSHIP_GROUP_HASH    0x7DBDD04862D95F04ULL
// SET_ENTITY_ONLY_DAMAGED_BY_PLAYER(entity, toggle)
// Peds with this flag take damage only from the local player, not from NPCs.
#define SET_ENTITY_ONLY_DAMAGED_BY_PLAYER  0x473598683095D430ULL
// SET_PED_CAN_BE_TARGETTED(ped, toggle)
// Explicitly allows the player's reticle / lock-on to select this ped.
// Without this, created NPCs may be invisible to the targeting system even if
// the relationship groups are configured for HATE.
#define SET_PED_CAN_BE_TARGETTED           0x63F58F7C80513AADULL
// SET_PED_CAN_BE_TARGETTED_BY_PLAYER(ped, player, toggle)
// Grants a specific player index the ability to lock on and deal damage.
// Pair with SET_PED_CAN_BE_TARGETTED for reliable mutual targeting.
#define SET_PED_CAN_BE_TARGETTED_BY_PLAYER 0x66B57B72E0836A76ULL
// SET_ENTITY_CAN_BE_TARGETED_WITHOUT_LOS(entity, toggle)
// Allows targeting the ped even when partially behind geometry / another entity.
#define SET_ENTITY_CAN_BE_TARGETED_WITHOUT_LOS 0x6D09F32E284D0FB7ULL

// ── Weapons (alloc8or confirmed) ──────────────────────────────────────────────
// GIVE_WEAPON_TO_PED(ped,weaponHash,ammoCount,forceInHand,forceInHolster,
//                   attachPoint,allowMultiple,p7,p8,addReason,ignoreUnlocks,permDeg,p12)
#define GIVE_WEAPON_TO_PED                 0x5E3BDDBCB83F3D84ULL
// GET_CURRENT_PED_WEAPON(ped, weaponHash*, p2, attachPoint, p4) -> BOOL
#define GET_CURRENT_PED_WEAPON             0x3A87E44BB9A01D54ULL
#define REMOVE_WEAPON_FROM_PED             0x4899CB088EDF59B8ULL

// ── Horse / Mount (alloc8or confirmed) ────────────────────────────────────────
// TASK_MOUNT_ANIMAL(ped, mount, timer, seatIndex, moveBlendRatio, mountStyle, p6, p7)
#define TASK_MOUNT_ANIMAL                  0x92DB0739813C5186ULL
// TASK_DISMOUNT_ANIMAL(rider, taskFlag, p2, p3, p4, targetPed)
#define TASK_DISMOUNT_ANIMAL               0x48E92D3DDE23C23AULL
// GET_MOUNT(ped) → Ped  — confirmed from natives.h (alloc8or)
#define GET_MOUNT                          0xE7E11B8DCBED1058ULL
// GET_HASH_KEY(string) -> Hash
#define GET_HASH_KEY_NATIVE                0xFD340785ADF8CFB7ULL

// ── Horse appearance ──────────────────────────────────────────────────────────
// SET_PED_DEFAULT_OUTFIT(ped, p1:bool) — used by Badlands_Horses (RedEM) for animals.
// For horses this loads the default saddle/blanket; SET_PED_RANDOM_COMPONENT_VARIATION
// works for human peds but does NOT initialise horse equipment correctly.
#define SET_PED_DEFAULT_OUTFIT             0x283978A15512B2FEULL

// ── Quick mount ───────────────────────────────────────────────────────────────
// Observed in Badlands_Horses client.lua: instantly seats ped on mount.
// Used as primary mount call; TASK_MOUNT_ANIMAL kept as fallback.
#define TASK_MOUNT_ANIMAL_QUICK            0x9A7A4A54596FE09DULL

// ── Horse outfit / variation ──────────────────────────────────────────────────
// _EQUIP_META_PED_OUTFIT(ped, outfitHash):
//   Equips a specific meta-ped outfit by hash (e.g. a named saddle/clothing preset).
//   Alt name: _EQUIP_META_PED_OUTFIT_COMPONENT.
#define _EQUIP_META_PED_OUTFIT             0x1902C4CFCC5BE57CULL
// _EQUIP_META_PED_OUTFIT_PRESET(ped, presetId, p2):
//   Equips the ped's outfit preset by index (0 = default).  For horses preset 0
//   is the saddled variant.  Must be followed by _UPDATE_PED_VARIATION to apply.
//   Old name: _SET_PED_OUTFIT_PRESET.
#define _EQUIP_META_PED_OUTFIT_PRESET      0x77FF8D35EEC6BBC4ULL
// _EQUIP_META_PED_SUBOUTFIT(ped, suboutfitHash, p2):
//   Equips a sub-outfit (layer/accessory) on top of the current meta-ped outfit.
//   p2 is always 0 in R* scripts.
#define _EQUIP_META_PED_SUBOUTFIT          0x66FF395445A88A6EULL
// _UPDATE_PED_VARIATION(ped,p1,p2,p3,p4,p5):
//   Finalises outfit / component changes — "needed after first creation, or when
//   component or texture/overlay is changed" (natives.h).
//   Typical call: (ped, false, true, true, true, true).
#define _UPDATE_PED_VARIATION              0xCC8CA3E88256E58FULL
// _SET_META_PED_TAG(ped, drawable, albedo, normal, material, palette, t0, t1, t2):
//   Applies a texture tag/overlay to a specific meta-ped component slot.
//   Used for clothing tints/decals after outfit is equipped.
#define _SET_META_PED_TAG                  0xBC6DF00D7A4A6819ULL
// _SET_HORSE_SCRIPTED_FLAG(ped, toggle):
//   Sets an internal horse-component flag (bit 0x04) used for scripted control
//   of mounts (e.g., after placing/rider-seating).  Call with true right after
//   spawning a scripted horse so the engine treats it as a placed/owned mount.
#define _SET_HORSE_SCRIPTED_FLAG           0xB8AB265426CFE6DDULL
// SET_PED_ONTO_MOUNT(ped, mount, seatIndex, p3) — instant mount (no animation)
#define SET_PED_ONTO_MOUNT                  0x028F76B6E78246EBULL
// _APPLY_SHOP_ITEM_TO_PED(ped, componentHash, p2, isMP, p4) — apply saddle/blanket
#define _APPLY_SHOP_ITEM_TO_PED             0xD3A7B003ED343FD9ULL
// _IS_MOUNT_SEAT_FREE(mount, seatIndex) → BOOL
#define _IS_MOUNT_SEAT_FREE                 0xAAB0FE202E9FC9F0ULL

// ── Fallback horse model (used only when remote sends horseModelHash == 0) ───
// a_c_horse_americanstandardbred_black
static constexpr UINT64 REMOTE_HORSE_FALLBACK = 0xB57D0193ULL;

// ── Outfit / appearance sync ──────────────────────────────────────────────────
// _GET_PED_META_OUTFIT_HASH(ped) → Hash — returns the hash of the outfit currently
// worn by the ped (Arthur, John, etc.).  Sent every PlayerSync tick.
#define _GET_PED_META_OUTFIT_HASH          0x30569F348D126A5AULL
#define _IS_META_PED_USING_COMPONENT        0xFB4891BD7578CDC1ULL
#define _SET_PED_COMPONENT_ENABLED           0xD3A7B003ED343FD9ULL
// _DOES_META_PED_OUTFIT_EXIST_FOR_PED_MODEL(outfitHash, modelHash) → BOOL
// Guards the request so we don't try to stream an outfit for the wrong model.
#define _DOES_META_PED_OUTFIT_EXIST_FOR_PED_MODEL  0xC0E880B7A441164DULL
// _REQUEST_META_PED_OUTFIT(modelHash, outfitHash) → int requestId
// Starts async streaming of the outfit asset for a given ped model.
#define _REQUEST_META_PED_OUTFIT           0x13154A76CE0CF9ABULL
// _HAS_META_PED_OUTFIT_LOADED(requestId) → BOOL — poll each frame.
#define _HAS_META_PED_OUTFIT_LOADED        0x610438375E5D1801ULL
// _APPLY_PED_META_PED_OUTFIT(requestId, ped, p2, p3) → BOOL
// Applies the streamed outfit to the ped.  p2=true, p3=false in R* scripts.
#define _APPLY_PED_META_PED_OUTFIT         0x74F512E29CB717E2ULL
// _RELEASE_META_PED_OUTFIT_REQUEST(requestId) — free the streaming slot.
#define _RELEASE_META_PED_OUTFIT_REQUEST   0x4592B8B9B0EF5F48ULL

// ── Camera ───────────────────────────────────────────────────────────────────
// GET_GAMEPLAY_CAM_ROT(rotationOrder) → Vector3   camera rotation (order 2 = ZYX)
#define GET_GAMEPLAY_CAM_ROT              0x0252D2B5582957A6ULL
// GET_GAMEPLAY_CAM_RELATIVE_HEADING() → float   heading relative to ped (0=forward)
#define GET_GAMEPLAY_CAM_RELATIVE_HEADING 0xC4ABF536048998AAULL

// Read camera yaw directly (avoids RVector3 padding issues)
static float GetCameraYaw()
{
    nativeInit(GET_GAMEPLAY_CAM_ROT);
    push(2); // rotationOrder = 2 (ZYX)
    PUINT64 res = nativeCall();
    if (!res) return -999.f;
    float z = 0;
    memcpy(&z, (char*)res + 8, 4); // Z is 3rd float (offset 8)
    return z;
}

// ── World entity scanning ─────────────────────────────────────────────────────
// IS_ENTITY_A_MISSION_ENTITY(entity) → BOOL
// Only mission entities are "owned" — world-ambient NPCs are excluded.
#define IS_ENTITY_A_MISSION_ENTITY     0x138190F64DB4BBD1ULL
// IS_PED_A_PLAYER(ped) → BOOL  — filter out the local player ped from scans.
#define IS_PED_A_PLAYER                0x12534C348C6CB68BULL
// GET_PED_NEARBY_PEDS(ped, sizeAndPeds*, ignoredPedType, p3) → int (count)
// sizeAndPeds[0] = count on return; sizeAndPeds[1..n] = ped handles.
// ignoredPedType = -1 → return all ped types; p3 = 0.
#define GET_PED_NEARBY_PEDS            0x23F8F5FC7E8C4A6BULL
// GET_ENTITY_VELOCITY(entity, p1:0) → Vector3 — physics velocity in m/s.
#define GET_ENTITY_VELOCITY            0x4805D2B1D8CF94A9ULL
// GET_ENTITY_TYPE(entity) → int  — 1=ped, 2=vehicle, 3=object.
#define GET_ENTITY_TYPE                0x97F696ACA466B4E0ULL
// IS_ENTITY_AN_OBJECT(entity) → BOOL
#define IS_ENTITY_AN_OBJECT            0x0A8A4A12A7C5E25DULL

// ── Object creation ───────────────────────────────────────────────────────────
// CREATE_OBJECT(modelHash, x, y, z, isNetwork, netMissionObject, dynamic) → Object
// Used to spawn world props (cannons, crates, wagons, etc.) on the remote client.
// Equivalent to OBJECT::CREATE_OBJECT in the RDR2 native namespace.
#define CREATE_OBJECT                  0xEB3505C8FAC33D40ULL

// ── Streaming / Model (alloc8or RDR2 nativedb — authoritative) ───────────────
// REQUEST_MODEL(model, p1:BOOL) — p1 = false (standard load, not urgent)
#define REQUEST_MODEL                  0xFA28FE3A6246FC30ULL
#define HAS_MODEL_LOADED                  0x1283B8B89DD5D1B6ULL
#define IS_MODEL_VALID                     0x392C8D8E07B70EFCULL  // (model) -> BOOL
#define SET_MODEL_AS_NO_LONGER_NEEDED  0x4AD96EF928BD4F9AULL

// ── Blips (RDR2 — confirmed alloc8or) ────────────────────────────────────────
// BlipAddForEntity(blipStyleHash, entity) → blipHandle
#define BLIP_ADD_FOR_ENTITY              0x23F74C2FDA6E7C61ULL
// BlipAddForCoords(blipStyleHash, x, y, z) → blipHandle  ← used for remote players
#define BLIP_ADD_FOR_COORDS              0x554D9D53F696D002ULL
// REMOVE_BLIP(blip*) — alloc8or confirmed
#define BLIP_REMOVE                      0xF2C3C9DA47AAA54AULL
// _SET_BLIP_NAME(blip, str)
#define SET_BLIP_NAME_FROM_PLAYER_STRING 0x9CB1A1623062F402ULL
// SET_BLIP_SCALE(blip, scale)
#define SET_BLIP_SCALE                   0xD38744167B2FA257ULL
// SET_BLIP_ROTATION(blip, rotationDegrees:int)
#define SET_BLIP_ROTATION                0x6049966A94FBE706ULL
// SET_BLIP_COORDS(blip, x, y, z) — update a coords-based blip each frame
#define SET_BLIP_COORDS                  0x4FF674F5E23D49CEULL
// _BLIP_SET_STYLE(blip, styleHash) — set style after creation
#define BLIP_SET_STYLE                   0xEDD964B7984AC291ULL
// BLIP_ADD_MODIFIER(blip, modifierHash) — add a behaviour modifier to a blip
#define BLIP_ADD_MODIFIER                0x662D364ABF16DE2FULL

// ── Blip style for remote players (confirmed from femga/rdr3_discoveries) ────
// BLIP_STYLE_FRIENDLY    = 0xFAAB68A9  Category:COMPANION  Range:Always  → always on minimap
// BLIP_STYLE_ENEMY_NO_THREAT = 0xCDF83C77  Category:ENEMY  Range:Always  → red dot always
// 0xF4813818 was a generic map-pin — appeared on big map only, NOT on minimap radar.
static constexpr UINT64 BLIP_STYLE_FRIENDLY    = 0xFAAB68A9ULL;   // green dot, always visible
static constexpr UINT64 BLIP_STYLE_ENEMY_ALWAYS = 0xCDF83C77ULL;  // red dot, always visible

// ── eBlipModifier hashes (alloc8or confirmed) ─────────────────────────────────
// Keeps the blip pinned to the radar edge when the entity is far away.
static constexpr UINT64 BLIP_MOD_RADAR_EDGE_ALWAYS = 0x32850803ULL;

// ── Remote ped model ─────────────────────────────────────────────────────────
// a_m_m_armtownfolk_01: generic story-mode ambient NPC, small, multiple instances OK
static constexpr UINT64 REMOTE_PED_MODEL = 0xCF7E73BEULL;

struct RVector3 { float x, _px, y, _py, z, _pz; };

// ─────────────────────────────────────────────────────────────────────────────
// Debug log
// ─────────────────────────────────────────────────────────────────────────────
static FILE* g_log = nullptr;
static void Log(const char* msg)
{ if (g_log) { fprintf(g_log, "%s\n", msg); fflush(g_log); } }
static bool IsHorseModel(uint32_t m)
{
    // Use native if available (build 1207+)
    if (invoke<bool>(_IS_THIS_MODEL_A_HORSE, (int)m)) return true;
    // Fallback: hardcoded list for build 1491 or older
    switch(m) {
        case 0xA1A5E5C5: case 0x056E0529: case 0x9B77D6E1: case 0xA49E1F56:
        case 0x4701C65C: case 0x1B7F3F6A: case 0x5E2B131F: case 0x6A6F2C53:
        case 0xB57D0193: case 0x0D3D0A38: case 0x8EF089E3: case 0x67E4FC7E:
        case 0x0D3E2F4E: case 0xA6FBD3A6: case 0xE8B9A5C5: case 0x9D7E6E1:
        return true;
        default: return false;
    }
}
static void LogFmt(const char* fmt, ...)
{ if (!g_log) return; va_list v; va_start(v,fmt); vfprintf(g_log,fmt,v); va_end(v); fprintf(g_log,"\n"); fflush(g_log); }

static int ScanActiveComponents(int ped, uint32_t* out, int max)
{
    int f = 0;
    for (int c = 0; c < ALL_CATEGORIES_COUNT && f < max; c++)
        for (int i = 0; i < ALL_CATEGORIES[c].count && f < max; i++)
            if (invoke<bool>(_IS_META_PED_USING_COMPONENT, ped, ALL_CATEGORIES[c].hashes[i]))
                { out[f++] = ALL_CATEGORIES[c].hashes[i]; break; }
    // Horse categories
    if (f < max)
    {
        for (int c = 0; c < HORSE_CATEGORIES_COUNT && f < max; c++)
            for (int i = 0; i < HORSE_CATEGORIES[c].count && f < max; i++)
                if (invoke<bool>(_IS_META_PED_USING_COMPONENT, ped, HORSE_CATEGORIES[c].hashes[i]))
                    { out[f++] = HORSE_CATEGORIES[c].hashes[i]; break; }
    }
    return f;
}

static bool IsHorseComponent(uint32_t hash)
{
    for (int c = 0; c < HORSE_CATEGORIES_COUNT; c++)
        for (int i = 0; i < HORSE_CATEGORIES[c].count; i++)
            if (HORSE_CATEGORIES[c].hashes[i] == hash) return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// State
// ─────────────────────────────────────────────────────────────────────────────
static NetworkClient    g_net;
static NativeMenu       g_menu;
static ChatBox          g_chat;
static PlayerListPanel  g_playerList;
static StatusBar        g_statusBar;
static LanScanner       g_lan;
static NotifyManager    g_notify;
static PluginSystem     g_plugins;
static WeatherControl   g_weatherCtrl;
static Language         g_lang;
static EntityPool       g_pool(50, 5000);
static int g_spectateTarget = -1;
static bool g_wasSpectating = false;
static bool g_autoConnect = false;
bool     g_weatherLocked = false;
uint32_t g_weatherTarget = 0;
uint32_t g_lastWeather  = 0;
bool     g_timeLocked   = false;
int      g_timeHour     = 12;
int      g_timeMinute   = 0;
static bool g_clothingDirty = false;
static std::string g_dllDir;
static int g_passengerHorse = -1;

static std::string g_username   = "Cowboy";
static std::string g_serverIp   = "127.0.0.1";
static int         g_serverPort = 7777;

// ── Remote player relationship group ─────────────────────────────────────────
// Remote player peds are set INVINCIBLE + non-targetable by auto-aim.
// Damage is applied exclusively via the SyncEvent system (receiver-side cone check).
static uint32_t g_remoteGroup = 0;

static void InitRemoteGroup()
{
    if (g_remoteGroup != 0) return;

    invoke<bool>(ADD_RELATIONSHIP_GROUP, "REMOTE_PLAYERS", &g_remoteGroup);
    if (g_remoteGroup == 0) return;

    int      player    = invoke<int>(GET_PLAYER_INDEX);
    int      playerPed = invoke<int>(GET_PLAYER_PED, player);
    uint32_t playerGrp = invoke<uint32_t>(GET_PED_RELATIONSHIP_GROUP_HASH, playerPed);

    // NEUTRAL — remote peds are not hostile NPCs; damage goes through SyncEvent only.
    invoke<void>(SET_RELATIONSHIP_BETWEEN_GROUPS, 3, playerGrp,     g_remoteGroup);
    invoke<void>(SET_RELATIONSHIP_BETWEEN_GROUPS, 3, g_remoteGroup, playerGrp);

    Log("InitRemoteGroup: relationship NEUTRAL (damage via SyncEvent only)");
}

// ── Animal / world-entity relationship group ──────────────────────────────────
// EntitySync-spawned animals/NPCs get this group so the auto-aim can lock onto
// them instead of accidentally targeting remote player peds.
static uint32_t g_animalGroup = 0;

static void InitAnimalGroup()
{
    if (g_animalGroup != 0) return;

    invoke<bool>(ADD_RELATIONSHIP_GROUP, "WORLD_ENTITIES", &g_animalGroup);
    if (g_animalGroup == 0) return;

    int      player    = invoke<int>(GET_PLAYER_INDEX);
    int      playerPed = invoke<int>(GET_PLAYER_PED, player);
    uint32_t playerGrp = invoke<uint32_t>(GET_PED_RELATIONSHIP_GROUP_HASH, playerPed);

    // Player HATES world entities → auto-aim engages animals & hostile NPCs
    invoke<void>(SET_RELATIONSHIP_BETWEEN_GROUPS, 5, playerGrp,     g_animalGroup);
    // World entities HATE player → animals/NPCs fight back
    invoke<void>(SET_RELATIONSHIP_BETWEEN_GROUPS, 5, g_animalGroup, playerGrp);
    // World entities neutral to remote players (don't attack synced player peds)
    if (g_remoteGroup != 0)
    {
        invoke<void>(SET_RELATIONSHIP_BETWEEN_GROUPS, 3, g_animalGroup, g_remoteGroup);
        invoke<void>(SET_RELATIONSHIP_BETWEEN_GROUPS, 3, g_remoteGroup, g_animalGroup);
    }

    Log("InitAnimalGroup: relationship HATE vs player, NEUTRAL vs remote players");
}

// Per-entity state for a world entity owned by a remote player.
// Follows the same lerp + dead-reckoning pattern used for horse sync.
struct RemoteEntityState
{
    int      handle      = -1;    // local spawn handle (-1 = not yet spawned)
    int      blipHandle  =  0;    // minimap blip (0 = none)
    uint32_t modelHash   = 0;
    uint8_t  entityType  = 0;     // 1=ped, 3=object
    // Raw target from last received EntitySyncEntry
    float    x=0, y=0, z=0, heading=0;
    float    velX=0, velY=0, velZ=0;
    // Smoothed position updated every frame
    float    smoothX=0, smoothY=0, smoothZ=0, smoothH=0;
    bool     smoothInit  = false;
    DWORD    lastSeenMs  = 0;     // GetTickCount() of last packet that included this entity
};

struct RemotePlayer {
    int         pedHandle    = -1;
    int         blipHandle   =  0;
    int         horseHandle  = -1;
    bool        wasOnHorse   = false;
    uint32_t    lastWeapon   =  0;
    uint32_t    wantedModel  =  0;
    uint32_t    pendingModel =  0;   // model being streamed in (don't delete ped yet)
    uint32_t    horseModel   =  0;   // model hash of the spawned horse (for change detection)

    std::string username;

    // ── Raw target received from last sync packet ─────────────────────────────
    float x=0, y=0, z=0, heading=0;
    uint8_t speed      = 0;
    bool    isAiming   = false;
    bool    isShooting = false;
    bool    isDead     = false;   // flag from remote packet

    // ── Death tracking ────────────────────────────────────────────────────────
    bool    wasDead    = false;   // last known death state (for transition detection)
    float   lastHealth = 1.0f;   // last received health ratio (for hit-reaction)

    // ── Interpolated position (updated every frame) ───────────────────────────
    float   smoothX=0, smoothY=0, smoothZ=0, smoothH=0;
    bool    smoothInit = false;   // true once first position is received

    // ── Last position passed to TASK_GO_STRAIGHT_TO_COORD ────────────────────
    float   taskX=0, taskY=0, taskZ=0;

    // ── Previous frame speed (for idle transition detection) ─────────────────
    uint8_t prevSpeed = 0;

    // ── Last heading applied via SET_ENTITY_HEADING ───────────────────────────
    // Used to skip redundant calls that interrupt idle animations.
    float   lastSetHeading = -999.f;

    // ── Targeting setup flag ──────────────────────────────────────────────────
    // True once SET_PED_RELATIONSHIP_GROUP_HASH + SET_PED_CAN_BE_TARGETTED have
    // been applied to this ped.  We retry every frame in UpdateRemotes until the
    // relationship group exists (timing race: ped may spawn before InitRemoteGroup
    // runs, which only happens inside SendSync — AFTER g_net.Poll processes the
    // first sync packet).
    bool    targetingSetup = false;

    // ── Aim/shoot task timer ──────────────────────────────────────────────────
    // Tracks how many frames remain on the current TASK_AIM / TASK_SHOOT task.
    // We re-issue only when it's about to expire so the animation isn't interrupted.
    int aimTaskTimer = 0;

    // ── Dead-reckoning (velocity extrapolation) ───────────────────────────────
    // Velocity computed from consecutive position packets (m/s).
    // Each frame we project: predictedPos = lastPos + vel * timeSinceLastPacket
    // so the smooth target is always ahead of the stale packet position.
    float    velX = 0.f, velY = 0.f, velZ = 0.f;
    DWORD    lastPacketMs = 0;   // GetTickCount() timestamp of last received sync

    // ── Horse mount state ─────────────────────────────────────────────────────
    // isMounting = true while TASK_MOUNT_ANIMAL animation is in progress.
    // While true we freeze the horse so it doesn't walk away and break the anim.
    // mountTimer prevents re-issuing the task every packet (same pattern as aimTaskTimer).
    bool    isMounting      = false;
    int     mountTimer      = 0;    // frames remaining before we can re-issue TASK_MOUNT_ANIMAL

    // ── Horse locomotion task timer ───────────────────────────────────────────
    // TASK_GO_STRAIGHT_TO_COORD drives gallop/trot/walk animations on the horse.
    // We rate-limit re-issue to every N frames to avoid restarting the animation
    // every frame (which freezes the horse in the first frame of the gait cycle).
    int     horseTaskTimer  = 0;

    // ── Saddle refresh timer ──────────────────────────────────────────────────
    // RDR2's engine resets the horse's outfit/variation during and after the
    // TASK_MOUNT_ANIMAL animation — applying the saddle once at mount-completion
    // is not enough.  This timer counts down for 3 s after mounting; every 15
    // frames we re-apply SET_PED_DEFAULT_OUTFIT + _EQUIP_META_PED_OUTFIT_PRESET
    // + _UPDATE_PED_VARIATION until the engine stops resetting it.
    int     saddleTimer     = 0;

    // ── Outfit / appearance sync ──────────────────────────────────────────────
    // outfitHash: last hash received from the remote player.
    // outfitRequestId: async streaming handle from _REQUEST_META_PED_OUTFIT (-1=none).
    // outfitApplied: true once _APPLY_PED_META_PED_OUTFIT succeeded for current hash.
    uint32_t outfitHash       = 0;
    int      outfitRequestId  = -1;
    bool     outfitApplied    = false;
    std::vector<uint32_t> pendingBodyClothing;
    bool     bodyClothingPending = false;

    // ── Horse saddle outfit sync ───────────────────────────────────────────────
    // horseOutfitHash: _GET_PED_META_OUTFIT_HASH read from the sender's actual
    // mount.  Used to apply the same saddle/blanket on the remote clone.
    // Applied asynchronously: request → HAS_LOADED → _APPLY → release (same
    // pipeline as the player outfit above).
    uint32_t horseOutfitHash       = 0;
    int      horseOutfitRequestId  = -1;
    bool     horseOutfitApplied    = false;
    std::vector<uint32_t> pendingHorseClothing;
    bool     horseClothingPending = false;

    // ── World entity sync ─────────────────────────────────────────────────────
    // Mission-entity NPCs / animals this remote player owns.
    // Key = netId assigned by the remote player (stable per session).
    // Cleaned up automatically: entity is deleted if absent from packets > 500 ms.
    std::map<uint16_t, RemoteEntityState> entities;
};
static std::map<int, RemotePlayer> g_players;
static int g_entityScanTimer = 0;
static int g_connectingTimer = -1;   // -1 = idle, >=0 = frames since last Connect()
static DWORD g_lastPlayerSyncMs = 0;
static DWORD g_lastEntitySyncMs = 0;
constexpr DWORD PLAYER_SYNC_INTERVAL_MS = 33; // ≈ 30 Hz, independent of game FPS
constexpr DWORD ENTITY_SYNC_INTERVAL_MS = 66; // ≈ 15 Hz, independent of game FPS
constexpr int ENTITY_SCAN_INTERVAL = 30;   // every 30 frames ≈ 2 Hz  (detect new/removed entities)
constexpr int CONNECT_TIMEOUT_FRAMES = 300;  // ~5 seconds at 60 fps

// ── Local owned-entity registry ───────────────────────────────────────────────
// Mission-entity peds near the local player that we want to sync to remotes.
// Key = local entity handle, value = stable netId assigned to that entity.
struct OwnedEntityInfo { uint16_t netId = 0; };
static std::map<int, OwnedEntityInfo> g_ownedEntities;
static uint16_t g_nextEntityNetId = 1;   // wraps at 65535; 0 is reserved

// ─────────────────────────────────────────────────────────────────────────────
// Input — GetAsyncKeyState (works without keyboardHandlerRegister)
// ─────────────────────────────────────────────────────────────────────────────
static bool g_prevKeys[256] = {};

// Returns true the first frame a key is held down (rising edge).
static bool KeyJustDown(int vk)
{
    bool cur = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool was = g_prevKeys[vk];
    g_prevKeys[vk] = cur;
    return cur && !was;
}

// Check whether a key is currently held (no edge detection).
static bool KeyHeld(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static void PollInput()
{
    // ── Chat input ────────────────────────────────────────────────────────────
    if (g_chat.isOpen)
    {
        if (KeyJustDown(VK_RETURN)) {
            std::string msg = g_chat.SubmitAndClose();
            if (!msg.empty()) {
                if (msg[0] == '/')
                {
                    if (msg == "/help" || msg == "/ayuda") {
                        g_chat.AddMessage("Sistema","/help /players /tp <n> /pos /spectate <n> /unspectate /clear /weather /time");
                    } else if (msg == "/players") {
                        for(auto&[id,rp]:g_players) g_chat.AddMessage("Sistema",rp.username);
                    } else if (msg.substr(0,4) == "/tp ") {
                        std::string t=msg.substr(4);
                        for(auto&[id,rp]:g_players) if(rp.username.find(t)==0){int p=invoke<int>(GET_PLAYER_INDEX);int pd=invoke<int>(GET_PLAYER_PED,p);invoke<void>(SET_ENTITY_COORDS_NO_OFFSET,pd,rp.x,rp.y,rp.z,false,false,false);g_chat.AddMessage("Sistema","TP a "+rp.username);g_notify.Objective("TP: "+rp.username,3000);break;}
                    } else if (msg.substr(0,10) == "/spectate ") {
                        std::string t=msg.substr(10);
                        for(auto&[id,rp]:g_players) if(rp.username.find(t)==0&&rp.pedHandle>0&&invoke<bool>(DOES_ENTITY_EXIST,rp.pedHandle)){g_spectateTarget=rp.pedHandle;g_chat.AddMessage("Sistema","Spectando: "+rp.username);g_notify.Objective("Spectando: "+rp.username,0);break;}
                    } else if (msg == "/unspectate") {
                        g_spectateTarget=-1; g_chat.AddMessage("Sistema","Spectate fin.");
                    } else if (msg == "/pos") {
                        int p=invoke<int>(GET_PLAYER_INDEX);int pd=invoke<int>(GET_PLAYER_PED,p);
                        RVector3 ps=invoke<RVector3>(GET_ENTITY_COORDS,pd,false);
                        char b[64];snprintf(b,64,"Pos: %.1f %.1f %.1f",ps.x,ps.y,ps.z);
                        g_chat.AddMessage("Sistema",b);g_notify.Tip(b,4000);
                    } else {
                        g_chat.AddMessage("Sistema","/help para comandos");
                        if(g_plugins.events.onChatCommand){size_t sp=msg.find(' ');g_plugins.events.onChatCommand((sp!=std::string::npos?msg.substr(0,sp):msg).c_str(),(sp!=std::string::npos?msg.substr(sp+1):"").c_str());}
                    }
                }
                else g_net.SendChat(g_username, msg);
            }
        }
        else if (KeyJustDown(VK_ESCAPE)) g_chat.Close();
        else if (KeyJustDown(VK_BACK))   g_chat.Backspace();
        else {
            // Character input via ToUnicode
            BYTE ks[256]; GetKeyboardState(ks);
            for (int vk = 32; vk < 128; vk++) {
                if (KeyJustDown(vk)) {
                    WCHAR out[2] = {};
                    UINT scan = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
                    if (ToUnicode(vk, scan, ks, out, 2, 0) == 1 && out[0] >= 32)
                        g_chat.AddChar((char)out[0]);
                }
            }
        }
        return;
    }

    // ── Menu input ────────────────────────────────────────────────────────────
    if (g_menu.isOpen)
    {
        if      (KeyJustDown(VK_UP))     g_menu.Nav(-1);
        else if (KeyJustDown(VK_DOWN))   g_menu.Nav(+1);
        else if (KeyJustDown(VK_RETURN)) g_menu.Select();
        else if (KeyJustDown(VK_ESCAPE)) g_menu.Back();
        else if (KeyJustDown(VK_F10))    g_menu.Close();
        return;
    }

    // ── Global shortcuts ──────────────────────────────────────────────────────
    if (KeyJustDown(VK_F10)) {
        Log("F10 — opening menu");
        g_menu.isConnected = g_net.connected;
        g_menu.serverIp    = g_serverIp;
        g_menu.serverPort  = g_serverPort;
        g_menu.username    = g_username;
        g_menu.Open();
    }

    if (KeyJustDown('O'))
        g_playerList.visible = !g_playerList.visible;

    if (KeyJustDown('T') && g_net.connected)
        g_chat.Open();

    if (KeyJustDown('G') && g_net.connected && !g_chat.isOpen && !g_menu.isOpen)
    {
        int myPed = invoke<int>(GET_PLAYER_PED, invoke<int>(GET_PLAYER_INDEX));
        if (g_passengerHorse > 0)
        {
            invoke<void>(CLEAR_PED_TASKS, myPed, true, false);
            g_passengerHorse = -1;
            g_notify.Tip("Desmontado", 2000);
        }
        else
        {
            RVector3 mp = invoke<RVector3>(GET_ENTITY_COORDS, myPed, false);
            int bh = -1; float bd = 5.0f;
            for (auto& [id, rp] : g_players)
            {
                if (rp.pedHandle <= 0 || rp.horseHandle <= 0) continue;
                if (!invoke<bool>(DOES_ENTITY_EXIST, rp.horseHandle)) continue;
                if (!invoke<bool>(IS_PED_ON_MOUNT, rp.pedHandle)) continue;
                if (invoke<bool>(IS_PED_ON_MOUNT, myPed)) continue;
                if (!invoke<bool>(_IS_MOUNT_SEAT_FREE, rp.horseHandle, 1)) continue;
                RVector3 hp = invoke<RVector3>(GET_ENTITY_COORDS, rp.horseHandle, false);
                float d = sqrtf((hp.x-mp.x)*(hp.x-mp.x)+(hp.y-mp.y)*(hp.y-mp.y)+(hp.z-mp.z)*(hp.z-mp.z));
                if (d < bd) { bd = d; bh = rp.horseHandle; }
            }
            if (bh > 0)
            {
                invoke<void>(CLEAR_PED_TASKS, myPed, true, false);
                invoke<void>(_SET_HORSE_SCRIPTED_FLAG, bh, true);
                invoke<void>(TASK_MOUNT_ANIMAL, myPed, bh, 8000, 1, 1, 1, false, false);
                g_passengerHorse = bh;
                g_notify.Tip("Montando en ancas (G para bajar)", 3000);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
// IS_BLIP_ON_MINIMAP(blip) — confirmed from natives.h
#define IS_BLIP_ON_MINIMAP  0x46534526B9CD2D17ULL

// Creates a coord-based blip for the remote player.
// Style BLIP_STYLE_FRIENDLY (0xFAAB68A9) — Category:COMPANION, Range:Always.
// "Range:Always" means it shows on BOTH the minimap radar AND the big map at all times.
// Previous style 0xF4813818 was a generic map-pin (big map only).
// SET_BLIP_COORDS is called every frame in UpdateRemotes to keep the position current.
static void AddPlayerBlip(RemotePlayer& r, float x, float y, float z)
{
    // Use BLIP_STYLE_FRIENDLY: confirmed always-visible on minimap (Range:Always, no range limit).
    r.blipHandle = invoke<int>(BLIP_ADD_FOR_COORDS, BLIP_STYLE_FRIENDLY, x, y, z);
    if (r.blipHandle == 0)
    {
        Log("AddPlayerBlip: BLIP_ADD_FOR_COORDS returned 0 — blip creation failed");
        return;
    }

    invoke<bool>(BLIP_SET_STYLE, r.blipHandle, BLIP_STYLE_FRIENDLY);
    invoke<void>(SET_BLIP_SCALE, r.blipHandle, 1.1f);
    invoke<void>(SET_BLIP_NAME_FROM_PLAYER_STRING, r.blipHandle, r.username.c_str());

    // RADAR_EDGE_ALWAYS ensures the blip stays visible at the radar edge when far away.
    invoke<bool>(BLIP_ADD_MODIFIER, r.blipHandle, BLIP_MOD_RADAR_EDGE_ALWAYS);

    bool onMinimap = invoke<bool>(IS_BLIP_ON_MINIMAP, r.blipHandle);
    LogFmt("AddPlayerBlip: handle=%d onMinimap=%d style=FRIENDLY for %s",
           r.blipHandle, (int)onMinimap, r.username.c_str());
}

static void RemovePlayerBlip(RemotePlayer& r)
{
    if (r.blipHandle != 0)
    {
        invoke<void>(BLIP_REMOVE, r.blipHandle);
        r.blipHandle = 0;
    }
}

static void DeleteRemoteEntities(RemotePlayer& rp)
{
    for (auto& [netId, re] : rp.entities)
    {
        if (re.blipHandle != 0)
            invoke<void>(BLIP_REMOVE, re.blipHandle);
        if (re.handle > 0)
        {
            int h = re.handle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, h, false, false);
            invoke<void>(DELETE_ENTITY, &h);
        }
    }
    rp.entities.clear();
}

static void DeleteRemote(int id)
{
    auto it = g_players.find(id);
    if (it != g_players.end()) {
        RemovePlayerBlip(it->second);
        DeleteRemoteEntities(it->second);
        // Un-mark as mission entity BEFORE deleting — without this, DELETE_ENTITY
        // silently fails and the ped stays in the world (clone bug).
        if (it->second.horseHandle > 0) {
            int hh = it->second.horseHandle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, hh, false, false);
            invoke<void>(DELETE_ENTITY, &hh);
        }
        if (it->second.pedHandle > 0) {
            int h = it->second.pedHandle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, h, false, false);
            invoke<void>(DELETE_ENTITY, &h);
        }
        g_players.erase(it);
    }
}
static void DeleteAllRemotes()
{
    for (auto& [id,p]: g_players) {
        RemovePlayerBlip(p);
        DeleteRemoteEntities(p);
        if (p.horseHandle > 0) {
            int hh = p.horseHandle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, hh, false, false);
            invoke<void>(DELETE_ENTITY, &hh);
        }
        if (p.pedHandle > 0) {
            int h = p.pedHandle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, h, false, false);
            invoke<void>(DELETE_ENTITY, &h);
        }
    }
    g_players.clear();
}

// ─────────────────────────────────────────────────────────────────────────────
// Config
// ─────────────────────────────────────────────────────────────────────────────
static std::string g_configPath;

// Returns the Windows computer name, or "Cowboy" if it can't be read.
static std::string GetPcName()
{
    char buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD len = sizeof(buf);
    if (GetComputerNameA(buf, &len) && len > 0) return std::string(buf);
    return "Cowboy";
}

static void LoadConfig()
{
    // Default username = PC name (written if the INI doesn't exist yet)
    g_username = GetPcName();

    std::ifstream f(g_configPath);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line))
    {
        if (line.rfind("username=", 0) == 0)
        {
            std::string val = line.substr(9);
            // Strip trailing CR so Windows line-endings don't sneak in
            if (!val.empty() && val.back() == '\r') val.pop_back();
            // %computername% → resolve to actual PC name
            if (val == "%computername%")  g_username = GetPcName();
            else if (!val.empty())        g_username = val;
        }
        else if (line.rfind("server_ip=", 0) == 0)
        {
            g_serverIp = line.substr(10);
            if (!g_serverIp.empty() && g_serverIp.back() == '\r')
                g_serverIp.pop_back();
        }
        else if (line.rfind("server_port=", 0) == 0)
        {
            int v = atoi(line.substr(12).c_str());
            if (v > 0) g_serverPort = v;
        }
    }
}
static void SaveConfig()
{
    std::ofstream f(g_configPath);
    f<<"username="<<g_username<<"\nserver_ip="<<g_serverIp<<"\nserver_port="<<g_serverPort<<"\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Network callbacks
// ─────────────────────────────────────────────────────────────────────────────
static void OnServerInfo(const ServerInfoPacket& p)
{
    g_net.connected=true; g_statusBar.connected=true;
    g_statusBar.serverName=p.serverName; g_statusBar.maxPlayers=p.maxPlayers;
    g_playerList.serverName=p.serverName; g_playerList.maxPlayers=p.maxPlayers;
    g_menu.isConnected=true;
    g_connectingTimer=-1;
    g_chat.AddMessage("Sistema","Conectado a '"+p.serverName+"' como "+g_username);
    g_notify.Advanced("RDR2Coop", "Conectado a "+p.serverName, "generic_textures","tick",5000);
    Log("Connected to server");
    {
        std::string plist;
        for (size_t i = 0; i < g_plugins.plugins.size(); i++)
        {
            if (i > 0) plist += ",";
            plist += g_plugins.plugins[i].name;
            if (!g_plugins.plugins[i].cmdList.empty())
                plist += "[" + g_plugins.plugins[i].cmdList + "]";
        }
        LogFmt("PluginInfo: %d plugins, raw='%s'", (int)g_plugins.plugins.size(), plist.c_str());
        if (plist.empty()) plist = "_builtin[wlock,wunlock,wlist,tlock,tunlock,wevents,ambush,spawnanimal,zombie]";
        else plist += ",_builtin[wlock,wunlock,wlist,tlock,tunlock,wevents,ambush,spawnanimal,zombie]";
        g_net.SendPluginInfo(g_net.localId, plist.c_str());
    }
}
static void OnPlayerConnect(const PlayerConnectPacket& p)
{ g_chat.AddMessage("Sistema",p.username+" entro al servidor.");
  g_notify.Tip(p.username+" se conecto",3000);
  if(g_plugins.events.onPlayerConnect)g_plugins.events.onPlayerConnect(0,p.username.c_str()); }
static void OnPlayerDisconnect(const PlayerDisconnectPacket& p)
{ DeleteRemote(p.playerId); g_chat.AddMessage("Sistema",p.username+" salio."); g_statusBar.players=(int)g_players.size()+1;
  if(g_plugins.events.onPlayerDisconnect)g_plugins.events.onPlayerDisconnect(p.playerId,p.username.c_str()); }
static void OnPlayerSync(const PlayerSyncPacket& p)
{
    if (p.playerId == g_net.localId) return;

    auto& r = g_players[p.playerId];
    // Log first sync to verify username received correctly
    if (!p.username.empty()) {
        static std::set<int> loggedIds;
        if (!loggedIds.count(p.playerId)) {
            loggedIds.insert(p.playerId);
            LogFmt("Username received: '%s' (len=%d)", p.username.c_str(), (int)p.username.size());
        }
    }

    // ── Update raw target state ───────────────────────────────────────────────
    // firstSync = this is the very first packet from this player (smoothInit not set yet).
    // Do NOT use r.username.empty() — the server may forward an empty username if it
    // hasn't been recompiled with the new wire format, causing infinite first-sync loops.
    bool firstSync = !r.smoothInit;
    if (!p.username.empty()) r.username = p.username;  // keep last known name if server sends empty

    // ── Dead-reckoning: compute velocity BEFORE overwriting old position ──────
    // vel = (newPos - oldPos) / dt  so each frame we can predict current position.
    DWORD nowMs = GetTickCount();
    if (r.smoothInit && r.lastPacketMs > 0)
    {
        float dt = (nowMs - r.lastPacketMs) / 1000.0f;
        if (dt > 0.005f && dt < 1.0f)   // 5ms–1s sanity window
        {
            r.velX = (p.x - r.x) / dt;
            r.velY = (p.y - r.y) / dt;
            r.velZ = (p.z - r.z) / dt;
        }
    }
    r.lastPacketMs = nowMs;

    r.x          = p.x; r.y = p.y; r.z = p.z;
    r.heading    = p.heading;
    r.speed      = p.speed;
    r.isAiming   = p.IsAiming();
    r.isShooting = p.IsShooting();
    r.isDead     = p.IsDead();

    // Outfit change detection — triggers async re-stream when the player
    // changes clothes or the ped is spawned fresh.
    if (p.outfitHash != 0 && p.outfitHash != r.outfitHash)
    {
        r.outfitHash    = p.outfitHash;
        r.outfitApplied = false;   // queue re-apply
        // Release any in-flight streaming request for the old outfit.
        if (r.outfitRequestId >= 0)
        {
            invoke<void>(_RELEASE_META_PED_OUTFIT_REQUEST, r.outfitRequestId);
            r.outfitRequestId = -1;
        }
    }

    // Horse outfit change detection — saddle/blanket on the mount.
    // When the hash changes (or horse spawns fresh) queue async streaming.
    if (p.horseOutfitHash != 0 && p.horseOutfitHash != r.horseOutfitHash)
    {
        r.horseOutfitHash    = p.horseOutfitHash;
        r.horseOutfitApplied = false;
        if (r.horseOutfitRequestId >= 0)
        {
            invoke<void>(_RELEASE_META_PED_OUTFIT_REQUEST, r.horseOutfitRequestId);
            r.horseOutfitRequestId = -1;
        }
    }

    // Zero velocity when idle so dead-reckoning doesn't slide a stopped player.
    if (p.speed == 0) { r.velX = 0.f; r.velY = 0.f; r.velZ = 0.f; }

    // Initialise smooth position on very first sync (no lerp from origin)
    if (!r.smoothInit)
    {
        r.smoothX = p.x; r.smoothY = p.y; r.smoothZ = p.z; r.smoothH = p.heading;
        r.taskX   = p.x; r.taskY   = p.y; r.taskZ   = p.z;
        r.velX = 0.f; r.velY = 0.f; r.velZ = 0.f;  // no velocity yet — avoid ghost extrapolation
        r.smoothInit = true;
    }

    if (firstSync)
        LogFmt("First sync from %s id=%d pos=%.0f,%.0f,%.0f model=0x%X",
               p.username.c_str(), p.playerId, p.x, p.y, p.z, p.modelHash);

    // ── Resolve desired model ─────────────────────────────────────────────────
    // Prefer the real player model; fall back to generic NPC if hash is missing.
    uint32_t wantModel = (p.modelHash != 0) ? p.modelHash : (uint32_t)REMOTE_PED_MODEL;

    // ── Respawn on model change ───────────────────────────────────────────────
    if (r.pedHandle > 0 && r.wantedModel != 0 && r.wantedModel != wantModel)
    {
        // Don't delete old ped yet — keep showing it while new model streams in
        // Validate model before attempting to stream
        if (wantModel == 0 || !invoke<bool>(IS_MODEL_VALID, wantModel))
        {
            LogFmt("Model change rejected: invalid 0x%X for %s, using fallback", wantModel, r.username.c_str());
            wantModel = (uint32_t)REMOTE_PED_MODEL;
        }
        r.pendingModel = wantModel;
        invoke<void>(REQUEST_MODEL, wantModel, false);
        LogFmt("Model change queued player=%d 0x%X→0x%X (keeping old ped)", p.playerId, r.wantedModel, wantModel);
    }

    // ── Spawn ped if missing or pending model ready ────────────────────────────
    bool modelReady = (r.pendingModel != 0 && invoke<bool>(HAS_MODEL_LOADED, r.pendingModel));
    if (r.pedHandle <= 0 || modelReady)
    {
        if (modelReady && r.pedHandle > 0)
        {
            // Delete old ped now that new model is ready
            RemovePlayerBlip(r);
            int oh = r.pedHandle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, oh, false, false);
            invoke<void>(DELETE_ENTITY, &oh);
            r.pedHandle      = -1;
            r.lastWeapon     = 0;
            r.wasDead        = false;
            r.lastHealth     = 1.0f;
            r.smoothInit     = false;
            r.velX = 0.f; r.velY = 0.f; r.velZ = 0.f;
            r.targetingSetup = false;
            if (r.outfitRequestId >= 0) { invoke<void>(_RELEASE_META_PED_OUTFIT_REQUEST, r.outfitRequestId); r.outfitRequestId = -1; }
            r.outfitApplied = false;
            r.bodyClothingPending = !r.pendingBodyClothing.empty();
            wantModel = r.pendingModel;
            r.pendingModel = 0;
            LogFmt("Model change applied player=%d → 0x%X", p.playerId, wantModel);
        }

        // Try the desired model first; fall back to generic NPC if it fails.
        uint32_t spawnModel = wantModel;

        if (!invoke<bool>(HAS_MODEL_LOADED, spawnModel))
        {
            invoke<void>(REQUEST_MODEL, spawnModel, false);
            return;  // wait for streaming — retry next sync tick
        }

        int newPed = invoke<int>(CREATE_PED,
            spawnModel, p.x, p.y, p.z + 0.5f, p.heading,
            false, false, false, false);

        // If player model failed (engine rejects it as NPC), use generic NPC
        if (newPed <= 0 && spawnModel != (uint32_t)REMOTE_PED_MODEL)
        {
            LogFmt("CREATE_PED model=0x%X failed — falling back to generic NPC", spawnModel);
            spawnModel = (uint32_t)REMOTE_PED_MODEL;
            if (invoke<bool>(HAS_MODEL_LOADED, spawnModel))
                newPed = invoke<int>(CREATE_PED,
                    spawnModel, p.x, p.y, p.z + 0.5f, p.heading,
                    false, false, false, false);
        }

        if (newPed > 0)
        {
            r.pedHandle   = newPed;
            r.wantedModel = wantModel;   // store DESIRED hash — not the actual spawn model

            invoke<void>(SET_MODEL_AS_NO_LONGER_NEEDED, spawnModel);
            LogFmt("Spawned ped=%d spawnModel=0x%X wantedModel=0x%X player=%d",
                   newPed, spawnModel, wantModel, p.playerId);

            // Initialise outfit — MUST be first or ped appears transparent
            invoke<void>(SET_PED_RANDOM_COMPONENT_VARIATION, r.pedHandle, 0);
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY,       r.pedHandle, true, true);
            // Invincible: physical bullets deal 0 damage to remote player peds.
            // All damage is applied exclusively via the SyncEvent cone-check system.
            invoke<void>(SET_ENTITY_INVINCIBLE,              r.pedHandle, true);
            invoke<void>(SET_PED_CAN_RAGDOLL,                r.pedHandle, true);
            // ── Targeting: NOT auto-aim lockable ──────────────────────────────
            // Animals and hostile NPCs (g_animalGroup) get auto-aim priority.
            // Remote player peds must not compete with them for the lock-on cursor.
            invoke<void>(SET_PED_CAN_BE_TARGETTED,           r.pedHandle, false);
            // Relationship group — retried in UpdateRemotes if g_remoteGroup == 0.
            if (g_remoteGroup != 0)
            {
                invoke<void>(SET_PED_RELATIONSHIP_GROUP_HASH, r.pedHandle, g_remoteGroup);
                r.targetingSetup = true;
            }
            invoke<void>(SET_ENTITY_VISIBLE,                 r.pedHandle, true);
            invoke<void>(SET_ENTITY_ALPHA,                   r.pedHandle, 255, false);
            invoke<void>(SET_ENTITY_LOD_DIST,                r.pedHandle, 9999);
            invoke<void>(FREEZE_ENTITY_POSITION,             r.pedHandle, false);
            invoke<void>(SET_BLOCKING_OF_NON_TEMP_EVENTS,    r.pedHandle, true);
            // Try network registration - lets engine handle sync automatically
            invoke<void>(NETWORK_REGISTER_ENTITY_AS_NETWORKED, r.pedHandle);
            AddPlayerBlip(r, p.x, p.y, p.z);
        }
        // On failure: r.pedHandle stays <= 0, silently retry next sync tick
    }

    int h = r.pedHandle;
    if (h <= 0) return;

    // ── Horse mount / dismount ────────────────────────────────────────────────
    bool onHorse = p.IsOnHorse();

    // Pick the horse model: prefer the one the remote player is actually riding.
    uint32_t wantHorseModel = (p.horseModelHash != 0)
        ? p.horseModelHash
        : (uint32_t)REMOTE_HORSE_FALLBACK;

    if (onHorse)
    {
        // Delete and respawn if the horse model changed (player switched mounts).
        if (r.horseHandle > 0 && r.horseModel != wantHorseModel)
        {
            LogFmt("Horse model changed 0x%X→0x%X — respawning mount", r.horseModel, wantHorseModel);
            int hh = r.horseHandle;
            invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, hh, false, false);
            invoke<void>(DELETE_ENTITY, &hh);
            r.horseHandle          = -1;
            r.horseModel           = 0;
            r.isMounting           = false;
            r.mountTimer           = 0;
            r.horseTaskTimer       = 0;
            r.saddleTimer          = 0;
            r.horseOutfitApplied   = false;
            r.horseOutfitRequestId = -1;
        }

        if (r.horseHandle <= 0)
        {
            // Keep requesting the model every packet until it streams in.
            if (!invoke<bool>(HAS_MODEL_LOADED, wantHorseModel))
            {
                invoke<void>(REQUEST_MODEL, wantHorseModel, false);
                LogFmt("Requesting horse model 0x%X", wantHorseModel);
            }
            else
            {
                // Model ready — check if EntitySync already created this mount
                // (avoids duplication when IsHorseModel triggers EntitySync first)
                int reusedEnt = -1;
                for (auto& [nid, re] : r.entities)
                    if (re.handle > 0 && re.modelHash == wantHorseModel && invoke<bool>(DOES_ENTITY_EXIST, re.handle))
                        { reusedEnt = re.handle; break; }
                
                if (reusedEnt > 0)
                {
                    r.horseHandle = reusedEnt;
                    r.horseModel = wantHorseModel;
                    r.horseClothingPending = !r.pendingHorseClothing.empty();
                    LogFmt("Reused EntitySync horse: handle=%d for %s", reusedEnt, r.username.c_str());
                }
                else
                {
                    r.horseHandle = invoke<int>(CREATE_PED,
                    wantHorseModel, r.smoothX, r.smoothY, r.smoothZ, p.heading,
                    false, false, false, false);
                invoke<void>(SET_MODEL_AS_NO_LONGER_NEEDED, wantHorseModel);

                LogFmt("Horse spawned: handle=%d model=0x%X for %s",
                       r.horseHandle, wantHorseModel, r.username.c_str());

                if (r.horseHandle > 0)
                {
                    r.horseModel = wantHorseModel;

                    // ── Initialise entity ──────────────────────────────────────────────
                    // Step 1: _SET_RANDOM_OUTFIT_VARIATION — makes horse visible /
                    //   prevents transparent spawn (same as SET_PED_DEFAULT_OUTFIT alias).
                    invoke<void>(SET_PED_DEFAULT_OUTFIT,          r.horseHandle, true);
                    // Step 2: _EQUIP_META_PED_OUTFIT_PRESET(horse, 0, false) — preset 0
                    //   is the saddled variant for horse models; equips saddle + blanket.
                    invoke<void>(_EQUIP_META_PED_OUTFIT_PRESET,   r.horseHandle, 0, false);
                    // Step 3: _SET_HORSE_SCRIPTED_FLAG — marks this as a scripted/placed
                    //   mount so the engine sets up rider-seating state correctly.
                    invoke<void>(_SET_HORSE_SCRIPTED_FLAG,        r.horseHandle, true);
                    // Step 4: _UPDATE_PED_VARIATION — finalises the outfit/component
                    //   changes.  Required whenever outfit is changed post-creation.
                    invoke<void>(_UPDATE_PED_VARIATION,           r.horseHandle, false, true, true, true, true);
                    invoke<void>(SET_ENTITY_AS_MISSION_ENTITY,    r.horseHandle, true, true);
                    invoke<void>(SET_ENTITY_INVINCIBLE,           r.horseHandle, true);
                    invoke<void>(SET_ENTITY_VISIBLE,              r.horseHandle, true);
                    invoke<void>(SET_ENTITY_ALPHA,                r.horseHandle, 255, false);
                    invoke<void>(SET_ENTITY_LOD_DIST,             r.horseHandle, 9999);
                    invoke<void>(SET_BLOCKING_OF_NON_TEMP_EVENTS, r.horseHandle, true);

                    // ── Do NOT freeze the horse ────────────────────────────────────────
                    // Freezing prevents gravity from placing the horse on the ground,
                    // causing it to float at whatever Z it was spawned at.
                    // Without a task the engine keeps the horse in a natural idle stance;
                    // it won't wander because SET_BLOCKING_OF_NON_TEMP_EVENTS is true.
                    invoke<void>(FREEZE_ENTITY_POSITION, r.horseHandle, false);
                    r.isMounting = true;
                    r.mountTimer = 0;

                    // Try instant mount first, fall back to animated mount
                    invoke<void>(SET_PED_ONTO_MOUNT, h, r.horseHandle, -1, false);
                    if (!invoke<bool>(IS_PED_ON_MOUNT, h))
                        invoke<void>(TASK_MOUNT_ANIMAL, h, r.horseHandle, 3000, -1, 1, 1, false, false);

                    LogFmt("Horse init: handle=%d model=0x%X TASK_MOUNT_ANIMAL issued for %s",
                           r.horseHandle, wantHorseModel, r.username.c_str());
                }
                } // closes else { CREATE_PED
            }
        }
        else
        {
            // Horse already exists — check mount state but only act with a timer.
            // Re-issuing TASK_MOUNT_ANIMAL every packet (33ms) restarts the animation
            // from frame 0 each time → ped frozen at "reaching for saddle" pose.
            // mountTimer prevents re-issue for ~5 s after the last attempt.
            bool stillMounted = invoke<bool>(IS_PED_ON_MOUNT, h);
            if (stillMounted)
            {
                r.isMounting = false;   // sync: mounted, clear flag if somehow set
            }
            else if (!r.isMounting && r.mountTimer <= 0)
            {
                // Not mounted and no animation in progress → re-issue TASK_MOUNT_ANIMAL.
                r.isMounting = true;
                r.mountTimer = 300;   // ~5s at 60fps before we try again
                // timer=3000 ms: auto-warp fallback if animation never completes.
                invoke<void>(TASK_MOUNT_ANIMAL, h, r.horseHandle, 3000, -1, 1, 1, false, false);
                LogFmt("Re-mount issued for %s", r.username.c_str());
            }
        }
    }
    else if (!onHorse && r.horseHandle > 0)
    {
        // Remote player dismounted — play dismount animation then delete.
        invoke<void>(TASK_DISMOUNT_ANIMAL, h, 0, 0, 0, 0, 0);
        int hh = r.horseHandle;
        invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, hh, false, false);
        invoke<void>(DELETE_ENTITY, &hh);
        r.horseHandle     = -1;
        r.horseModel      = 0;
        r.isMounting           = false;
        r.mountTimer           = 0;
        r.horseTaskTimer       = 0;
        r.saddleTimer          = 0;
        r.horseOutfitApplied   = false;
        r.horseOutfitRequestId = -1;
    }

    r.wasOnHorse = onHorse;

    // ── Weapon sync ───────────────────────────────────────────────────────────
    if (p.weaponHash != 0 && p.weaponHash != r.lastWeapon)
    {
        invoke<void>(GIVE_WEAPON_TO_PED,
            h, (int)p.weaponHash, 9999, true, false, 0, false,
            0.5f, 0.5f, (int)0, true, 0.0f, false);
        r.lastWeapon = p.weaponHash;
    }

    // ── Aiming / Shooting ─────────────────────────────────────────────────────
    // Tasks are issued from UpdateRemotes with a timer to avoid restarting the
    // animation on every packet (which froze the ped at the "raise gun" frame).

    // ── Health / Death ────────────────────────────────────────────────────────
    int maxHp = invoke<int>(GET_ENTITY_MAX_HEALTH, h, false);
    if (maxHp > 0)
    {
        bool nowDead = p.IsDead();

        if (nowDead && !r.wasDead)
        {
            // Transition: alive → dead
            invoke<void>(SET_ENTITY_HEALTH,  h, 0, 0, 0);
            invoke<void>(SET_PED_TO_RAGDOLL, h, 500, 1500, 0, true, true, nullptr);
            LogFmt("Remote %s died", r.username.c_str());
        }
        else if (!nowDead && r.wasDead)
        {
            // Transition: dead → alive (respawned)
            invoke<void>(RESURRECT_PED, h);
            // Restore invincible + non-targetable after resurrect (engine resets these)
            invoke<void>(SET_ENTITY_INVINCIBLE,           h, true);
            invoke<void>(SET_PED_CAN_BE_TARGETTED,        h, false);
            if (g_remoteGroup != 0)
                invoke<void>(SET_PED_RELATIONSHIP_GROUP_HASH, h, g_remoteGroup);
            int fullHp = (int)(p.health * (float)maxHp);
            invoke<void>(SET_ENTITY_HEALTH, h, fullHp > 0 ? fullHp : maxHp, 0, 0);
            LogFmt("Remote %s respawned", r.username.c_str());
        }
        else if (!nowDead)
        {
            // Still alive — sync health value directly.
            // Clamp to minimum 1 so the ped can never die from local player
            // damage between syncs (authoritative death comes from the remote).
            int targetHp = (int)(p.health * (float)maxHp);
            if (targetHp < 1) targetHp = 1;

            // Big hit (>20% drop in one tick) → trigger visible flinch reaction
            if (p.health < r.lastHealth - 0.20f)
            {
                int curHp  = invoke<int>(GET_ENTITY_HEALTH, h, 0, 0);
                int damage = curHp - targetHp;
                if (damage > 0)
                    invoke<void>(APPLY_DAMAGE_TO_PED, h, damage, false, 0, 0);
            }
            invoke<void>(SET_ENTITY_HEALTH, h, targetHp, 0, 0);
        }

        r.wasDead    = nowDead;
        r.lastHealth = p.health;
    }

    g_statusBar.players = (int)g_players.size() + 1;
}
static void OnChatMessage(const ChatMessagePacket& p)
{
    // Server commands (prefixed with !cmd)
    if (p.message.rfind("!cmd ", 0) == 0)
    {
        std::string cmd = p.message.substr(5);
        if (cmd.length() >= 3 && cmd.substr(0, 3) == "tp ")
        {
            std::string t = cmd.substr(3);
            for (auto& [id, rp] : g_players)
            {
                if (rp.username.find(t) == 0 || rp.username == t)
                {
                    int pp = invoke<int>(GET_PLAYER_INDEX);
                    int pd = invoke<int>(GET_PLAYER_PED, pp);
                    invoke<void>(SET_ENTITY_COORDS_NO_OFFSET, pd, rp.x, rp.y, rp.z, false, false, false);
                    g_chat.AddMessage("Sistema", "TP a " + rp.username);
                    break;
                }
            }
        }
        return; // don't show !cmd in chat
    }
    g_chat.AddMessage(p.username, p.message);
}
static void OnClothingSync(const ClothingSyncPacket& p)
{
    if (p.senderId == g_net.localId) return;
    // Clothing often arrives before the first PlayerSync has spawned the remote
    // ped. Keep it on the player record and apply it after model/outfit streaming.
    auto& rp = g_players[p.senderId];
    rp.pendingBodyClothing.clear();
    rp.pendingHorseClothing.clear();
    for (int i = 0; i < p.count; i++)
    {
        uint32_t h = p.hashes[i];
        if (IsHorseComponent(h)) rp.pendingHorseClothing.push_back(h);
        else
            rp.pendingBodyClothing.push_back(h);
    }
    rp.bodyClothingPending = !rp.pendingBodyClothing.empty();
    rp.horseClothingPending = !rp.pendingHorseClothing.empty();
}

static void OnDisconnected()
{
    DeleteAllRemotes();
    g_ownedEntities.clear();
    g_menu.isConnected   = false;
    g_statusBar.connected = false;
    g_chat.AddMessage("Sistema","Desconectado.");
}

static void OnWeatherSync(const WeatherSyncPacket& p)
{
    if(p.weatherType != 0) {
        invoke<void>(SET_WEATHER_TYPE, p.weatherType, true, true, false, 0.0f, false);
        g_lastWeather = p.weatherType; // store for per-frame application
    }
    invoke<void>(SET_CLOCK_TIME, p.hour, p.minute, p.second);
}

// (CheckShotHit removed — damage is now resolved on the attacker side via victimId)

// Helper: check if a shot from (ax,ay,az) with heading ah hits entity handle
static bool CheckShotHit(int handle, float ax, float ay, float az, float ah, float maxDist2, int attackerPed)
{
    if (!invoke<bool>(DOES_ENTITY_EXIST, handle)) return false;
    RVector3 ep = invoke<RVector3>(GET_ENTITY_COORDS, handle, false);
    float edx = ep.x - ax, edy = ep.y - ay;
    float ed2 = edx*edx + edy*edy;
    if (ed2 > maxDist2 || ed2 < 0.01f) return false;
    float ta = atan2f(edx, edy);
    float ad = fabsf(ta - ah);
    if (ad > 3.14159265f) ad = 6.2831853f - ad;
    if (ad < 0.26f && fabsf(ep.z - az) < 2.0f)
    {
        invoke<void>(APPLY_DAMAGE_TO_PED, handle, 35, true, attackerPed, 0);
        invoke<void>(SET_PED_TO_RAGDOLL, handle, 150, 150, 0, false, false, false);
        return true;
    }
    return false;
}

static void OnSyncEvent(const SyncEventPacket& p)
{
    auto getName = [](int id)->std::string{auto it=g_players.find(id);return(it!=g_players.end())?it->second.username:"?";};
    switch(p.eventType){
        case SyncEventType::PedKilled:
            g_notify.Advanced(getName(p.attackerId)+" asesino a "+getName(p.victimId),"Asesinato","generic_textures","skull",5000); break;
        case SyncEventType::PedHit:
            if(p.victimId==g_net.localId) g_notify.Top("Te atropellaron!",getName(p.attackerId),4000); break;
        case SyncEventType::HorseStolen:
            if(p.victimId==g_net.localId){int lp=invoke<int>(GET_PLAYER_PED,invoke<int>(GET_PLAYER_INDEX));invoke<void>(0x5337B721C51883A9,lp);g_notify.Advanced("Robo de caballo!",getName(p.attackerId)+" te robo el caballo","generic_textures","skull",6000);}
            break;
        case SyncEventType::Shot:
            if (p.attackerId == g_net.localId) break;
            {
                int lp = invoke<int>(GET_PLAYER_PED, invoke<int>(GET_PLAYER_INDEX));
                auto it = g_players.find(p.attackerId);
                if (it != g_players.end() && it->second.pedHandle > 0)
                {
                    RVector3 ap = invoke<RVector3>(GET_ENTITY_COORDS, it->second.pedHandle, false);
                    RVector3 mp = invoke<RVector3>(GET_ENTITY_COORDS, lp, false);
                    float d2 = (ap.x-mp.x)*(ap.x-mp.x)+(ap.y-mp.y)*(ap.y-mp.y)+(ap.z-mp.z)*(ap.z-mp.z);
                    if (d2 < 900.0f)
                    {
                        float ah = it->second.smoothH * 3.14159265f / 180.0f;
                        // Always check entities in line of fire (animals take priority)
                        bool hitEntity = false;
                        for (auto& [handle, info] : g_ownedEntities) {
                            if (!invoke<bool>(DOES_ENTITY_EXIST, handle)) continue;
                            if (CheckShotHit(handle, ap.x, ap.y, ap.z, ah, d2, it->second.pedHandle))
                                { hitEntity = true; break; }
                        }
                        if (!hitEntity) {
                            for (auto& [oid, orp] : g_players) {
                                if (orp.horseHandle > 0 && invoke<bool>(DOES_ENTITY_EXIST, orp.horseHandle))
                                    if (CheckShotHit(orp.horseHandle, ap.x, ap.y, ap.z, ah, 3600.f, it->second.pedHandle))
                                        { hitEntity = true; break; }
                                for (auto& [nid, oe] : orp.entities) {
                                    if (oe.handle <= 0 || !invoke<bool>(DOES_ENTITY_EXIST, oe.handle)) continue;
                                    if (CheckShotHit(oe.handle, ap.x, ap.y, ap.z, ah, 3600.f, it->second.pedHandle))
                                        { hitEntity = true; break; }
                                }
                                if (hitEntity) break;
                            }
                        }
                        // Damage player: victimId match, cone check, OR very close range
                        bool shouldDamage = (p.victimId == g_net.localId);
                        if (!shouldDamage) {
                            float dx = mp.x - ap.x, dy = mp.y - ap.y;
                            float toTarget = atan2f(dx, dy);
                            float ad = fabsf(toTarget - ah);
                            if (ad > 3.14159265f) ad = 6.2831853f - ad;
                            shouldDamage = (ad < 0.70f) || (d2 < 25.f); // 40° cone or < 5m
                        }
                        if (!hitEntity && shouldDamage)
                        {
                            if (p.extraData == 0x7A8A1F57)
                                invoke<void>(SET_PED_TO_RAGDOLL, lp, 2000, 3000, 0, true, true, 0);
                            else
                            {
                                invoke<void>(APPLY_DAMAGE_TO_PED, lp, 35, true, it->second.pedHandle, 0);
                                invoke<void>(SET_PED_TO_RAGDOLL, lp, 150, 150, 0, false, false, false);
                            }
                        }
                    }
                }
            }
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sync
// ─────────────────────────────────────────────────────────────────────────────

// Called every frame — interpolates remote positions and drives ped movement.
static void UpdateRemotes()
{
    // (K is computed per-player inside the loop — see adaptive K block below)

    for (auto& [id, rp] : g_players)
    {
        int h = rp.pedHandle;
        if (h <= 0) continue;

        // If engine GC'd the ped, mark for respawn
        if (!invoke<bool>(DOES_ENTITY_EXIST, h)) { rp.pedHandle = -1; continue; }
        if (!rp.smoothInit) continue;

        // ── Targeting setup retry ─────────────────────────────────────────────
        // If g_remoteGroup was 0 when the ped was spawned (timing race: first
        // sync packet arrives before the first SendSync call creates the group),
        // we retry every frame here until the group is ready.
        if (!rp.targetingSetup && g_remoteGroup != 0)
        {
            invoke<void>(SET_PED_RELATIONSHIP_GROUP_HASH, h, g_remoteGroup);
            invoke<void>(SET_PED_CAN_BE_TARGETTED,        h, false); // no auto-aim
            invoke<void>(SET_ENTITY_INVINCIBLE,           h, true);  // SyncEvent-only damage
            rp.targetingSetup = true;
            LogFmt("Targeting setup applied (retry) for %s", rp.username.c_str());
        }

        // ── Outfit async streaming ────────────────────────────────────────────
        // Flow mirrors model streaming: request → poll → apply → release.
        // Only attempted when the remote is using their actual player model
        // (wantedModel != 0 and outfit exists for that model).
        // Skip if ped is dead — outfit will be re-applied on respawn.
        if (!rp.outfitApplied && rp.outfitHash != 0
            && rp.wantedModel != 0
            && !rp.isDead)
        {
            if (rp.outfitRequestId < 0)
            {
                // Guard: verify outfit is compatible with the spawned model.
                bool compatible = invoke<bool>(
                    _DOES_META_PED_OUTFIT_EXIST_FOR_PED_MODEL,
                    rp.outfitHash, rp.wantedModel);
                if (compatible)
                {
                    rp.outfitRequestId = invoke<int>(
                        _REQUEST_META_PED_OUTFIT, rp.wantedModel, rp.outfitHash);
                    LogFmt("Outfit request started for %s (hash=0x%X reqId=%d)",
                           rp.username.c_str(), rp.outfitHash, rp.outfitRequestId);
                }
                else
                {
                    // Outfit doesn't exist for this model (e.g. fallback NPC).
                    rp.outfitApplied = true;
                }
            }
            else if (invoke<bool>(_HAS_META_PED_OUTFIT_LOADED, rp.outfitRequestId))
            {
                // Outfit streamed in — apply and release.
                invoke<bool>(_APPLY_PED_META_PED_OUTFIT,
                             rp.outfitRequestId, h, true, false);
                invoke<void>(_UPDATE_PED_VARIATION,
                             h, false, true, true, true, true);
                invoke<void>(_RELEASE_META_PED_OUTFIT_REQUEST, rp.outfitRequestId);
                rp.outfitRequestId = -1;
                rp.outfitApplied   = true;
                LogFmt("Outfit applied for %s (hash=0x%X)",
                       rp.username.c_str(), rp.outfitHash);
            }
        }

        // Apply component-level clothing after the preset: applying the preset
        // later would otherwise overwrite custom pieces captured by ClothingSync.
        if (rp.bodyClothingPending && !rp.isDead
            && (rp.outfitApplied || rp.outfitHash == 0))
        {
            int componentCount = (int)rp.pendingBodyClothing.size();
            for (uint32_t component : rp.pendingBodyClothing)
                invoke<void>(_SET_PED_COMPONENT_ENABLED, h, (int)component, false, true, true);
            invoke<void>(_UPDATE_PED_VARIATION, h, false, true, true, true, true);
            rp.bodyClothingPending = false;
            LogFmt("Applied %d clothing components for %s",
                   componentCount, rp.username.c_str());
        }

        // If the ped died locally (from shots between syncs) but the remote
        // player hasn't reported death yet, resurrect immediately so we don't
        // show a death animation that the authoritative source didn't send.
        if (!rp.isDead && invoke<bool>(IS_PED_DEAD_OR_DYING, h, true))
        {
            invoke<void>(RESURRECT_PED, h);
            invoke<void>(SET_ENTITY_INVINCIBLE,           h, true);
            invoke<void>(SET_PED_CAN_BE_TARGETTED,        h, false);
            if (g_remoteGroup != 0)
                invoke<void>(SET_PED_RELATIONSHIP_GROUP_HASH, h, g_remoteGroup);
        }

        // Dead peds manage their own ragdoll — don't drive their position.
        if (rp.isDead)
        {
            RVector3 dp = invoke<RVector3>(GET_ENTITY_COORDS, h, false);
            if (rp.blipHandle != 0)
            {
                invoke<void>(SET_BLIP_COORDS,   rp.blipHandle, dp.x, dp.y, dp.z);
                invoke<void>(SET_BLIP_ROTATION, rp.blipHandle, (int)rp.smoothH);
            }
            Draw::WorldLabel(rp.username, dp.x, dp.y, dp.z + 1.0f);
            continue;
        }

        // ── Dead-reckoning + lerp smooth position ────────────────────────────
        // Adaptive lerp gain: at walk speed K=0.25 gives smooth follow-through.
        // At run/gallop we raise K so the clone catches up faster, but we also
        // skip velocity extrapolation (below) to avoid curved-path overshoot.
        const float K = (rp.speed >= 2) ? 0.6f : 0.25f;

        // Instead of lerping toward the stale last-packet position, we extrapolate
        // forward using velocity so the predicted target is approximately where the
        // remote player IS right now (cancels out most packet-interval lag).
        //
        // predDt = time elapsed since the last packet arrived.
        // We cap at 150 ms so a brief stall doesn't fly the ped across the map.
        float predDt = 0.f;
        if (rp.lastPacketMs > 0)
        {
            DWORD elMs = GetTickCount() - rp.lastPacketMs;
            predDt = (float)elMs / 1000.0f;
            if (predDt > 0.15f) predDt = 0.15f;
        }

        // Dead-reckoning: extrapolate position forward using velocity.
        // At walk speed (0-1) this is safe; at run/gallop (2-3) velocity changes
        // direction frequently, so extrapolating even 150 ms ahead overshoots on
        // corners and causes visible teleports.  Skip extrapolation at high speed —
        // the higher K above compensates for the lost lead.
        float predX = rp.x, predY = rp.y, predZ = rp.z;
        if (rp.speed == 1)
        {
            predX += rp.velX * predDt;
            predY += rp.velY * predDt;
            predZ += rp.velZ * predDt;
        }

        // Snap instantly on teleport (> 20 m jump between packet and prediction).
        float dx = predX - rp.smoothX, dy = predY - rp.smoothY;
        if (dx*dx + dy*dy > 400.0f)
        {
            rp.smoothX = predX; rp.smoothY = predY; rp.smoothZ = predZ;
        }
        else if (dx*dx + dy*dy > 0.01f)   // skip only <0.1m micro-jitter
        {
            rp.smoothX += (predX - rp.smoothX) * K;
            rp.smoothY += (predY - rp.smoothY) * K;
            rp.smoothZ += (predZ - rp.smoothZ) * K;
        }

        // Heading lerp — wrap-aware so 359°→1° goes the short way
        float dh = rp.heading - rp.smoothH;
        if (dh >  180.f) dh -= 360.f;
        if (dh < -180.f) dh += 360.f;
        rp.smoothH += dh * K;

        // Fetch real entity position once — used by idle drift check, label and blip
        RVector3 ep = invoke<RVector3>(GET_ENTITY_COORDS, h, false);

        // ── Horse mount state machine ─────────────────────────────────────────
        if (rp.mountTimer > 0) rp.mountTimer--;

        if (rp.isMounting && rp.horseHandle > 0)
        {
            // Check every frame whether the ped has finished mounting.
            if (invoke<bool>(IS_PED_ON_MOUNT, h))
            {
                rp.isMounting = false;
                // Start the saddle refresh timer: the engine resets the horse
                // outfit repeatedly during/after TASK_MOUNT_ANIMAL, so we
                // re-apply every 15 frames for 3 s (= 180 frames at 60 fps).
                rp.saddleTimer = 180;
                LogFmt("Mount complete for %s — saddle refresh started", rp.username.c_str());
            }
            else
            {
                // Horse and ped must follow the rider's position even while mounting.
                // If we leave the horse at spawn coords while the rider gallops away,
                // the ped can never reach the horse → mount never completes.
                invoke<void>(SET_ENTITY_COORDS_NO_OFFSET,
                    rp.horseHandle, rp.smoothX, rp.smoothY, rp.smoothZ, false, false, false);
                invoke<void>(SET_ENTITY_HEADING, rp.horseHandle, rp.smoothH);
                // IMPORTANT: do NOT override the ped's coords here.
                // Calling SET_ENTITY_COORDS_NO_OFFSET on the ped every frame resets the
                // ped's task queue, which cancels TASK_MOUNT_ANIMAL before the engine
                // can finish the mounting sequence.  The timer=3000 auto-warp handles
                // the case where the animation takes too long; we just need to keep the
                // HORSE at the right position so the ped doesn't have to travel far.
            }
        }

        // ── Horse outfit (saddle) async streaming ────────────────────────────────
        // The sender reports their mount's outfit hash (_GET_PED_META_OUTFIT_HASH on
        // the horse).  We request → poll → apply → release, same pipeline as the
        // player outfit.  This gives the remote clone the exact same saddle/blanket.
        // Only attempted when horse is fully mounted (not during mount animation).
        if (rp.horseHandle > 0 && !rp.isMounting
            && rp.horseOutfitHash != 0 && !rp.horseOutfitApplied)
        {
            if (rp.horseOutfitRequestId < 0)
            {
                // Verify the outfit is compatible with this horse model.
                uint32_t horseModel = (uint32_t)invoke<int>(GET_ENTITY_MODEL, rp.horseHandle);
                bool compat = invoke<bool>(
                    _DOES_META_PED_OUTFIT_EXIST_FOR_PED_MODEL,
                    rp.horseOutfitHash, horseModel);
                if (compat)
                {
                    rp.horseOutfitRequestId = invoke<int>(
                        _REQUEST_META_PED_OUTFIT, horseModel, rp.horseOutfitHash);
                    LogFmt("Horse outfit request for %s (hash=0x%X reqId=%d)",
                           rp.username.c_str(), rp.horseOutfitHash, rp.horseOutfitRequestId);
                }
                else
                {
                    // Outfit not in the horse model's metadata — fall back to default.
                    invoke<void>(SET_PED_DEFAULT_OUTFIT, rp.horseHandle, true);
                    invoke<void>(_UPDATE_PED_VARIATION, rp.horseHandle, false, true, true, true, true);
                    rp.horseOutfitApplied = true;
                    LogFmt("Horse outfit fallback (SET_PED_DEFAULT_OUTFIT) for %s", rp.username.c_str());
                }
            }
            else if (invoke<bool>(_HAS_META_PED_OUTFIT_LOADED, rp.horseOutfitRequestId))
            {
                invoke<bool>(_APPLY_PED_META_PED_OUTFIT,
                             rp.horseOutfitRequestId, rp.horseHandle, true, false);
                invoke<void>(_UPDATE_PED_VARIATION,
                             rp.horseHandle, false, true, true, true, true);
                invoke<void>(_RELEASE_META_PED_OUTFIT_REQUEST, rp.horseOutfitRequestId);
                rp.horseOutfitRequestId = -1;
                rp.horseOutfitApplied   = true;
                // Also kick the saddleTimer so we re-enforce the outfit for 3 s
                // in case the engine resets it after the streaming apply.
                rp.saddleTimer = 180;
                LogFmt("Horse outfit applied for %s (hash=0x%X)",
                       rp.username.c_str(), rp.horseOutfitHash);
            }
        }

        if (rp.horseClothingPending && rp.horseHandle > 0 && !rp.isMounting
            && rp.saddleTimer <= 0
            && (rp.horseOutfitApplied || rp.horseOutfitHash == 0))
        {
            for (uint32_t component : rp.pendingHorseClothing)
            {
                invoke<void>(_SET_PED_COMPONENT_ENABLED,
                             rp.horseHandle, (int)component, false, true, true);
                invoke<void>(_APPLY_SHOP_ITEM_TO_PED,
                             rp.horseHandle, (int)component, true, false, false);
            }
            invoke<void>(_UPDATE_PED_VARIATION,
                         rp.horseHandle, false, true, true, true, true);
            rp.horseClothingPending = false;
        }

        // ── Saddle refresh (post-apply) ───────────────────────────────────────────
        // After _APPLY_PED_META_PED_OUTFIT succeeds the engine can still reset the
        // horse variation during the mount animation settling. Re-apply the outfit
        // every 15 frames for 3 s.  No outfit hash needed here — just re-commit.
        if (rp.saddleTimer > 0 && rp.horseHandle > 0 && !rp.isMounting)
        {
            if (rp.saddleTimer % 15 == 0 && rp.horseOutfitApplied)
            {
                // Re-apply whichever method succeeded.
                if (rp.horseOutfitHash != 0 && rp.horseOutfitRequestId < 0)
                {
                    // Re-apply via default outfit (streaming already released).
                    invoke<void>(SET_PED_DEFAULT_OUTFIT, rp.horseHandle, true);
                    invoke<void>(_UPDATE_PED_VARIATION,  rp.horseHandle, false, true, true, true, true);
                }
            }
            rp.saddleTimer--;
        }

        // moveTarget: when fully mounted → drive the horse (ped follows automatically).
        // When isMounting or on foot → use the ped handle directly.
        int moveTarget = (rp.horseHandle > 0 && !rp.isMounting) ? rp.horseHandle : h;

        // ── Horse locomotion control (runs when fully mounted) ───────────────────
        // SET_ENTITY_VELOCITY alone moves the horse's physics body but bypasses
        // RDR2's animation state machine — the horse slides with no gait animations.
        // (This is unlike GTA V vehicles where wheel-spin is physics-driven.)
        //
        // Solution: TASK_GO_STRAIGHT_TO_COORD on the HORSE entity.  When a task is
        // issued to the horse, the engine:
        //   1. Activates the correct locomotion clip (walk / trot / canter / gallop)
        //      based on the moveBlendRatio parameter.
        //   2. Moves the horse toward the target.
        //   3. The mounted ped plays riding animations automatically.
        //
        // We rate-limit task re-issue to every ~20 frames (~0.33 s) to avoid
        // restarting the animation from frame 0 on every game tick.
        // A velocity correction term (k * error) keeps the horse glued to smoothX/Y
        // even if the task drifts slightly off course between re-issues.
        //
        //   Hard snap when gap > 10 m (network stall, teleport, respawn).
        if (rp.horseHandle > 0 && !rp.isMounting)
        {
            RVector3 hp = invoke<RVector3>(GET_ENTITY_COORDS, rp.horseHandle, false);
            float hdx = rp.smoothX - hp.x;
            float hdy = rp.smoothY - hp.y;
            float hdz = rp.smoothZ - hp.z;
            float dist2 = hdx*hdx + hdy*hdy + hdz*hdz;

            if (dist2 > 100.f)   // > 10 m — hard snap
            {
                invoke<void>(SET_ENTITY_COORDS_NO_OFFSET,
                    rp.horseHandle, rp.smoothX, rp.smoothY, rp.smoothZ, false, false, false);
                invoke<void>(SET_ENTITY_VELOCITY, rp.horseHandle, rp.velX, rp.velY, 0.f);
                rp.horseTaskTimer = 0;   // force immediate task re-issue after snap
            }
            else if (rp.speed > 0)
            {
// Moving — issue locomotion task to smoothX/Y directly (no clamping)
                if (rp.horseTaskTimer <= 0)
                {
                    float blend = (rp.speed == 3) ? 3.0f
                                : (rp.speed == 2) ? 1.5f
                                :                   1.0f;
                    invoke<void>(TASK_GO_STRAIGHT_TO_COORD,
                        rp.horseHandle,
                        rp.smoothX, rp.smoothY, rp.smoothZ,
                        blend, 9999, rp.smoothH, 1.0f, 0);
                    rp.horseTaskTimer = 10;
                }
                else rp.horseTaskTimer--;

                // Gentle velocity correction (K=3) keeps horse near target without teleporting
                constexpr float K = 3.f;
                invoke<void>(SET_ENTITY_VELOCITY, rp.horseHandle,
                    rp.velX + hdx * K, rp.velY + hdy * K, rp.velZ + hdz * K);
            }
            else
            {
                // Idle — clear locomotion task once on moving→idle transition.
                if (rp.prevSpeed > 0)
                {
                    invoke<void>(CLEAR_PED_TASKS, rp.horseHandle);
                    rp.horseTaskTimer = 0;
                }
                // Drift correction: only snap if >2m drift (was 0.5m, too aggressive)
                if (dist2 > 4.0f)
                    invoke<void>(SET_ENTITY_COORDS_NO_OFFSET,
                        rp.horseHandle, rp.smoothX, rp.smoothY, rp.smoothZ, false, false, false);
            }

            invoke<void>(SET_ENTITY_HEADING, rp.horseHandle, rp.smoothH);
        }

        if (rp.isMounting)
        {
            // Waiting for mount anim — horse + ped already positioned in state machine above.
        }
        else if (rp.speed > 0 && !rp.isAiming && !rp.isShooting)
        {
            // ── Moving on foot ────────────────────────────────────────────────
            if (rp.horseHandle <= 0)
            {
                // moveBlendRatio for on-foot TASK_GO_STRAIGHT_TO_COORD is a
                // 0–1 blend where 0.2 = walk, 0.6 = run, 1.0 = sprint.
                // Higher values = more visible movement animation
                float moveBlend = (rp.speed == 3) ? 3.0f : (rp.speed == 2) ? 1.5f : 1.0f;
                float maxReach  = (rp.speed == 3) ? 3.0f : (rp.speed == 2) ? 2.0f : 1.0f;
                float targetX = rp.smoothX, targetY = rp.smoothY, targetZ = rp.smoothZ;
                {
                    float rx = rp.smoothX - ep.x, ry = rp.smoothY - ep.y;
                    float rd = sqrtf(rx*rx + ry*ry);
                    if (rd > maxReach) { float inv = maxReach/rd; rx *= inv; ry *= inv; }
                    targetX = ep.x + rx; targetY = ep.y + ry;
                }
                invoke<void>(TASK_GO_STRAIGHT_TO_COORD,
                    h, targetX, targetY, targetZ,
                    moveBlend, 9999, rp.heading, 0.05f, 0);
                rp.taskX = targetX; rp.taskY = targetY; rp.taskZ = targetZ;
            }
            // On horse: velocity block above already handled it.
            rp.aimTaskTimer = 0;
        }
else if (rp.isAiming || rp.isShooting)
        {
            // ── Aiming / shooting ─────────────────────────────────────────────
            // Damage is resolved via SyncEvent victimId (attacker-side detection),
            // so in-engine bullets don't determine hits — they are purely visual.
            //
            // Re-issue timers: re-issuing TASK_SHOOT/AIM every frame forces the
            // engine to restart the rotation animation each tick, causing the wild
            // spinning ("looking crazy") artefact.  Shoot re-issues less often
            // to let animation finish; aim re-issues every ~0.5s to track target.
            constexpr int SHOOT_REISSUE_FRAMES = 20;
            constexpr int AIM_REISSUE_FRAMES   = 30;
            if (rp.aimTaskTimer <= 0)
            {
                float aimRad = rp.smoothH * 3.14159265f / 180.0f;
                float ax = rp.smoothX + sinf(aimRad) * 50.0f;
                float ay = rp.smoothY + cosf(aimRad) * 50.0f;
                float az = rp.smoothZ + 0.8f;
                if (rp.isShooting && rp.lastWeapon != 0)
                {
                    invoke<void>(TASK_SHOOT_AT_COORD, h, ax, ay, az, 1500, 0, 0);
                    rp.aimTaskTimer = SHOOT_REISSUE_FRAMES;
                }
                else
                {
                    invoke<void>(TASK_AIM_GUN_AT_COORD, h, ax, ay, az, -1, 0, 0);
                    rp.aimTaskTimer = AIM_REISSUE_FRAMES;
                }
            }
            else { rp.aimTaskTimer--; }
        }
        else
        {
            // ── Idle ──────────────────────────────────────────────────────────
            // If we just stopped aiming/shooting (aimTaskTimer > 0 means a
            // TASK_AIM_GUN or TASK_SHOOT was still running), cancel the secondary
            // task so the ped stops pointing the weapon immediately.
            if (rp.aimTaskTimer > 0)
            {
                invoke<void>(CLEAR_PED_SECONDARY_TASK, h);
                rp.aimTaskTimer = 0;
            }
            if (rp.horseHandle <= 0)
            {
                // On foot idle: clear task once on transition, then hold heading.
if (rp.prevSpeed > 0)
            {
                // Don't clear tasks — let the engine play deceleration animation naturally
                rp.lastSetHeading = -999.f;
            }
                float hDiff = fabsf(rp.smoothH - rp.lastSetHeading);
                if (hDiff > 180.f) hDiff = 360.f - hDiff;
                if (hDiff > 3.0f)
                {
                    invoke<void>(SET_ENTITY_HEADING, h, rp.smoothH);
                    rp.lastSetHeading = rp.smoothH;
                }
float pdx = ep.x - rp.smoothX, pdy = ep.y - rp.smoothY;
            if (pdx*pdx + pdy*pdy > 1.0f)  // snap only if >1m drift
                invoke<void>(SET_ENTITY_COORDS_NO_OFFSET,
                    h, rp.smoothX, rp.smoothY, rp.smoothZ, false, false, false);
            }
            // On horse idle: velocity block above zeroes out naturally (velX/Y/Z=0
            // when speed==0, so only the correction term runs, holding the horse still).
            rp.taskX = rp.x; rp.taskY = rp.y; rp.taskZ = rp.z;
            // aimTaskTimer already reset to 0 at the top of this else block.
        }

        rp.prevSpeed = rp.speed;

        // ── Floating name label + blip — always at the ped's actual entity pos ─
        // (ep already fetched above, before the movement branch)

        // Update coord-based blip position and rotation every frame.
        if (rp.blipHandle != 0)
        {
            invoke<void>(SET_BLIP_COORDS,   rp.blipHandle, ep.x, ep.y, ep.z);
            invoke<void>(SET_BLIP_ROTATION, rp.blipHandle, (int)rp.smoothH);
        }

        Draw::WorldLabel(rp.username, ep.x, ep.y, ep.z + 1.0f);

        // ── Remote owned-entity update ────────────────────────────────────────
        // Same lerp + dead-reckoning as the player, but applied to world entities
        // (NPCs / animals) this remote player owns.
        // predDt is already computed above for the player's own dead-reckoning.
        DWORD nowMs2 = GetTickCount();
        for (auto& [netId, re] : rp.entities)
        {
            if (re.handle <= 0 || !invoke<bool>(DOES_ENTITY_EXIST, re.handle)) continue;
            if (!re.smoothInit) continue;

            // Extrapolate target position using stored velocity.
            float eDt = (float)(nowMs2 - re.lastSeenMs) / 1000.0f;
            if (eDt > 0.15f) eDt = 0.15f;
            float tX = re.x + re.velX * eDt;
            float tY = re.y + re.velY * eDt;
            float tZ = re.z + re.velZ * eDt;

            // Entity K — higher than player K for fast animals
            constexpr float EK_LERP = 0.85f;
            // Snap on teleport (> 50 m), else lerp.
            float edx = tX - re.smoothX, edy = tY - re.smoothY;
            if (edx*edx + edy*edy > 2500.f) {
                re.smoothX = tX; re.smoothY = tY; re.smoothZ = tZ;
            } else {
                re.smoothX += (tX - re.smoothX) * EK_LERP;
                re.smoothY += (tY - re.smoothY) * EK_LERP;
                re.smoothZ += (tZ - re.smoothZ) * EK_LERP;
            }

            // Heading lerp (wrap-aware).
            float edh = re.heading - re.smoothH;
            if (edh >  180.f) edh -= 360.f;
            if (edh < -180.f) edh += 360.f;
            re.smoothH += edh * EK_LERP;

            // Position the entity: velocity + proportional correction (same as horse).
            RVector3 ePos = invoke<RVector3>(GET_ENTITY_COORDS, re.handle, false);
            float cx = re.smoothX - ePos.x, cy = re.smoothY - ePos.y;
            float dist2e = cx*cx + cy*cy;
            if (dist2e > 100.f) {
                invoke<void>(SET_ENTITY_COORDS_NO_OFFSET,
                    re.handle, re.smoothX, re.smoothY, re.smoothZ, false, false, false);
            } else {
                constexpr float EK = 8.f;
                invoke<void>(SET_ENTITY_VELOCITY, re.handle,
                    re.velX + cx*EK, re.velY + cy*EK, 0.f);
            }
            invoke<void>(SET_ENTITY_HEADING, re.handle, re.smoothH);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Entity sync — receive side
// ─────────────────────────────────────────────────────────────────────────────
static void OnEntitySync(const EntitySyncPacket& pk)
{
    // Ignore our own echo (server only broadcasts to others, but be defensive).
    if (pk.senderId == g_net.localId) return;

    auto it = g_players.find(pk.senderId);
    if (it == g_players.end()) return;
    auto& rp = it->second;

    DWORD nowMs = GetTickCount();

    for (int i = 0; i < pk.count; i++)
    {
        const auto& e = pk.entries[i];
        if (e.modelHash == 0) continue;

        auto& re = rp.entities[e.netId];
        re.lastSeenMs  = nowMs;
        re.modelHash   = e.modelHash;
        re.entityType  = e.entityType;

        // Dead-reckoning: store velocity for inter-packet extrapolation.
        re.velX = e.velX; re.velY = e.velY; re.velZ = e.velZ;

        if (!re.smoothInit)
        {
            re.smoothX = e.x; re.smoothY = e.y; re.smoothZ = e.z;
            re.smoothH = e.heading;
            re.smoothInit = true;
        }
        re.x = e.x; re.y = e.y; re.z = e.z;
        re.heading = e.heading;

        // ── Spawn entity if not yet created or was deleted by engine ─────────
        bool exists = (re.handle > 0 && invoke<bool>(DOES_ENTITY_EXIST, re.handle));
        if (!exists)
        {
            re.handle = -1;
            if (invoke<bool>(HAS_MODEL_LOADED, e.modelHash))
            {
                int newEnt = -1;
                int etype = e.entityType;
                LogFmt("EntitySync spawning: netId=%d model=0x%X type=%d", e.netId, e.modelHash, etype);

                if (etype == 3)
                {
                    newEnt = invoke<int>(CREATE_OBJECT,
                        (int)e.modelHash, e.x, e.y, e.z, false, false, true);
                    invoke<void>(SET_MODEL_AS_NO_LONGER_NEEDED, e.modelHash);
                    if (newEnt > 0)
                    {
                        re.handle = newEnt;
                        invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, newEnt, true, true);
                        invoke<void>(SET_ENTITY_VISIBLE,           newEnt, true);
                        invoke<void>(SET_ENTITY_ALPHA,             newEnt, 255, false);
                        invoke<void>(SET_ENTITY_LOD_DIST,          newEnt, 9999);
                        invoke<void>(FREEZE_ENTITY_POSITION,       newEnt, false);
                        LogFmt("EntitySync spawn object: netId=%d model=0x%X handle=%d owner=%d",
                               e.netId, e.modelHash, newEnt, pk.senderId);
                    }
                }
                else if (e.entityType == 2)
                {
                    newEnt = invoke<int>(CREATE_VEHICLE,
                        (int)e.modelHash, e.x, e.y, e.z, e.heading, false, false, false, false);
                    invoke<void>(SET_MODEL_AS_NO_LONGER_NEEDED, e.modelHash);
                    if (newEnt > 0)
                    {
                        re.handle = newEnt;
                        invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, newEnt, true, true);
                        invoke<void>(SET_ENTITY_VISIBLE,           newEnt, true);
                        invoke<void>(SET_ENTITY_LOD_DIST,          newEnt, 9999);
                        invoke<void>(FREEZE_ENTITY_POSITION,       newEnt, false);
                        LogFmt("EntitySync spawn vehicle: netId=%d model=0x%X handle=%d owner=%d",
                               e.netId, e.modelHash, newEnt, pk.senderId);
                    }
                }
                else
                {
                    // ── Ped (human NPC or animal) ──────────────────────────────
                    newEnt = invoke<int>(CREATE_PED,
                        e.modelHash, e.x, e.y, e.z + 0.5f, e.heading,
                        false, false, false, false);
                    invoke<void>(SET_MODEL_AS_NO_LONGER_NEEDED, e.modelHash);

                    if (newEnt > 0)
                    {
                        re.handle = newEnt;

                        // SET_PED_RANDOM_COMPONENT_VARIATION → human NPCs visible.
                        // SET_PED_DEFAULT_OUTFIT             → animals/horses visible.
                        // Do NOT call _EQUIP_META_PED_OUTFIT_PRESET here — preset 0
                        // is undefined for most human models and clears their outfit,
                        // making them fully transparent (invisible ped bug).
                        invoke<void>(SET_PED_RANDOM_COMPONENT_VARIATION, newEnt, 0);
                        invoke<void>(SET_PED_DEFAULT_OUTFIT,             newEnt, true);
                        invoke<void>(_UPDATE_PED_VARIATION,              newEnt, false, true, true, true, true);
                        invoke<void>(SET_ENTITY_AS_MISSION_ENTITY,       newEnt, true, true);
                        invoke<void>(SET_ENTITY_VISIBLE,                 newEnt, true);
                        invoke<void>(SET_ENTITY_ALPHA,                   newEnt, 255, false);
                        invoke<void>(SET_ENTITY_LOD_DIST,                newEnt, 9999);
                        if (IsHorseModel(e.modelHash))
                        {
                            invoke<void>(SET_ENTITY_INVINCIBLE,  newEnt, false);
                            invoke<void>(FREEZE_ENTITY_POSITION, newEnt, false);
                            invoke<void>(SET_BLOCKING_OF_NON_TEMP_EVENTS, newEnt, false);
                            invoke<void>(_SET_HORSE_SCRIPTED_FLAG, newEnt, true);
                            invoke<void>(SET_PED_CAN_RAGDOLL, newEnt, true);
                        }
                        else if (!invoke<bool>(IS_PED_HUMAN, newEnt))
                        {
                            // Animal — block ambient events but let AI run
                            invoke<void>(SET_ENTITY_INVINCIBLE,  newEnt, false);
                            invoke<void>(FREEZE_ENTITY_POSITION, newEnt, false);
                            invoke<void>(SET_BLOCKING_OF_NON_TEMP_EVENTS, newEnt, true);
                            invoke<void>(SET_PED_CAN_BE_TARGETTED,           newEnt, true);
                            invoke<void>(SET_PED_CAN_BE_TARGETTED_BY_PLAYER, newEnt,
                                         invoke<int>(GET_PLAYER_INDEX), true);
                            if (g_animalGroup != 0)
                                invoke<void>(SET_PED_RELATIONSHIP_GROUP_HASH, newEnt, g_animalGroup);
                        }
                        else
                        {
                            // Human NPC — let AI run freely, can ragdoll on death
                            invoke<void>(SET_ENTITY_INVINCIBLE,  newEnt, false);
                            invoke<void>(FREEZE_ENTITY_POSITION, newEnt, false);
                            invoke<void>(SET_BLOCKING_OF_NON_TEMP_EVENTS, newEnt, false);
                            invoke<void>(SET_PED_CAN_RAGDOLL, newEnt, true);
                        }
                        if (!IsHorseModel(e.modelHash) && rp.pedHandle > 0)
                            invoke<void>(TASK_COMBAT_PED, newEnt, rp.pedHandle, 0, 16);
                        LogFmt("EntitySync spawn ped: netId=%d model=0x%X handle=%d owner=%d",
                               e.netId, e.modelHash, newEnt, pk.senderId);
                    }
                }
            }
            else
            {
                invoke<void>(REQUEST_MODEL, e.modelHash, false);
            }
        }
    }

    // ── Delete entities absent from packets for > 500 ms ─────────────────────
    int localMount = 0;
    {
        int lp = invoke<int>(GET_PLAYER_PED, invoke<int>(GET_PLAYER_INDEX));
        localMount = invoke<bool>(IS_PED_ON_MOUNT, lp) ? invoke<int>(GET_MOUNT, lp) : 0;
    }
    for (auto eit = rp.entities.begin(); eit != rp.entities.end();)
    {
        if (nowMs - eit->second.lastSeenMs > 500)
        {
            if (eit->second.handle > 0 && eit->second.handle == localMount)
                { ++eit; continue; }
            if (eit->second.blipHandle != 0)
                invoke<void>(BLIP_REMOVE, eit->second.blipHandle);
            if (eit->second.handle > 0)
            {
                int h = eit->second.handle;
                invoke<void>(SET_ENTITY_AS_MISSION_ENTITY, h, false, false);
                invoke<void>(DELETE_ENTITY, &h);
                LogFmt("EntitySync delete: netId=%d (timeout)", eit->first);
            }
            eit = rp.entities.erase(eit);
        }
        else ++eit;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Entity sync — send side: scan + send
// ─────────────────────────────────────────────────────────────────────────────

// ScanOwnedEntities — called every ENTITY_SCAN_INTERVAL frames.
// ADDs newly visible peds/objects to g_ownedEntities.
// REMOVEs only when the entity no longer exists OR moves > 200 m away.
// This 200 m keepalive lets trainer-spawned horses stay synced even after the
// local player walks to the other side of town.
static void ScanOwnedEntities()
{
    int player    = invoke<int>(GET_PLAYER_INDEX);
    int playerPed = invoke<int>(GET_PLAYER_PED, player);
    int myHorse   = invoke<bool>(IS_PED_ON_MOUNT, playerPed)
                  ? invoke<int>(GET_MOUNT, playerPed) : 0;

    RVector3 myPos = invoke<RVector3>(GET_ENTITY_COORDS, playerPed, false);

    // Build exclusion set: remote player peds, their horses, their entities.
    std::set<int> remoteHandles;
    for (auto& [id, rp] : g_players)
    {
        if (rp.pedHandle   > 0) remoteHandles.insert(rp.pedHandle);
        if (rp.horseHandle > 0) remoteHandles.insert(rp.horseHandle);
        for (auto& [nid, re] : rp.entities)
            if (re.handle > 0) remoteHandles.insert(re.handle);
    }

    // ── ADD: nearby mission-entity peds (trainer horses, police, NPCs) ──────────
    // IS_ENTITY_A_MISSION_ENTITY is checked to avoid cloning every ambient NPC
    // in towns — without it every townsperson would appear doubled on the remote.
    // Trainer-spawned horses ARE flagged as mission entities by RampageUI.
    // Pursuit police are also flagged by the game's internal pursuit scripts.
    //
    // IMPORTANT: pedBuf is heap-allocated to avoid stack corruption.
    // GET_PED_NEARBY_PEDS may ignore the capacity hint and write more handles
    // than requested — same issue as worldGetAllObjects with a stack buffer.
    // pedBuf[0] is pre-set to capacity (some RDR2 builds use it as a hint).
    const int PED_BUF = 256;
    std::vector<int> pedBuf(PED_BUF + 1, 0);
    pedBuf[0] = PED_BUF;
    int pedCount = invoke<int>(GET_PED_NEARBY_PEDS, playerPed, pedBuf.data(), -1, 0);
    if (pedCount > PED_BUF) pedCount = PED_BUF;

    for (int i = 0; i < pedCount; i++)
    {
        int ped = pedBuf[i + 1];
        if (ped <= 0)                                       continue;
        if (ped == playerPed)                               continue;
        if (ped == myHorse)                                 continue;
        if (remoteHandles.count(ped))                       continue;
        if (invoke<bool>(IS_PED_A_PLAYER, ped))             continue;
        bool isHorse  = IsHorseModel((uint32_t)invoke<int>(GET_ENTITY_MODEL, ped));
        bool isAnimal = !invoke<bool>(IS_PED_HUMAN, ped);
        bool isCombat = invoke<bool>(IS_PED_IN_COMBAT, ped, playerPed) ||
                        invoke<bool>(IS_PED_SHOOTING, ped);
        bool isMissionHuman = invoke<bool>(IS_PED_HUMAN, ped) && invoke<bool>(IS_ENTITY_A_MISSION_ENTITY, ped);
        if (!isHorse && !isAnimal && !isCombat && !isMissionHuman) continue;

        if (g_ownedEntities.find(ped) == g_ownedEntities.end())
        {
            OwnedEntityInfo info;
            info.netId = g_nextEntityNetId++;
            if (g_nextEntityNetId == 0) g_nextEntityNetId = 1;
            g_ownedEntities[ped] = info;
            LogFmt("EntitySync track ped: handle=%d netId=%d model=0x%X",
                   ped, info.netId, (uint32_t)invoke<int>(GET_ENTITY_MODEL, ped));
        }
    }

    // ── ADD: world objects within 80 m (cannons, crates, props…) ─────────────
    // worldGetAllObjects buffer is heap-allocated to prevent stack corruption —
    // the ScriptHookRDR2 implementation may write all pool objects regardless of
    // the arrSize parameter, overflowing a stack array.
    // Buffer is 8192 entries (32 KB on heap) to handle large object pools;
    // IS_ENTITY_A_MISSION_ENTITY then filters out static world props (rocks, fences…).
    {
        const int OBJ_BUF = 8192;
        std::vector<int> objBuf(OBJ_BUF, 0);
        int objCount = worldGetAllObjects(objBuf.data(), OBJ_BUF);
        if (objCount > OBJ_BUF) objCount = OBJ_BUF;

        for (int i = 0; i < objCount; i++)
        {
            int obj = objBuf[i];
            if (obj <= 0)                                        continue;
            if (remoteHandles.count(obj))                        continue;
            if (!invoke<bool>(DOES_ENTITY_EXIST, obj))           continue;   // stale handle guard

            RVector3 op = invoke<RVector3>(GET_ENTITY_COORDS, obj, false);
            float dx = op.x - myPos.x, dy = op.y - myPos.y;
            if (dx*dx + dy*dy > 6400.f) continue;   // 80 m radius

            if (g_ownedEntities.find(obj) == g_ownedEntities.end())
            {
                OwnedEntityInfo info;
                info.netId = g_nextEntityNetId++;
                if (g_nextEntityNetId == 0) g_nextEntityNetId = 1;
                g_ownedEntities[obj] = info;
                LogFmt("EntitySync track object: handle=%d netId=%d model=0x%X",
                       obj, info.netId, (uint32_t)invoke<int>(GET_ENTITY_MODEL, obj));
            }
        }
    }

    // ── ADD: vehicles + objects together (worldGetAllVehicles may not work in all builds)
    {
        const int VEH_BUF = 512;
        std::vector<int> vehBuf(VEH_BUF, 0);
        int vehCount = worldGetAllVehicles(vehBuf.data(), VEH_BUF);
        LogFmt("EntitySync vehicle scan: found %d", vehCount);
        if (vehCount > VEH_BUF) vehCount = VEH_BUF;
        for (int i = 0; i < vehCount; i++)
        {
            int veh = vehBuf[i];
            if (veh <= 0) continue;
            if (remoteHandles.count(veh)) continue;
            if (!invoke<bool>(DOES_ENTITY_EXIST, veh)) continue;
            RVector3 vp = invoke<RVector3>(GET_ENTITY_COORDS, veh, false);
            float dx = vp.x - myPos.x, dy = vp.y - myPos.y;
            if (dx*dx + dy*dy > 6400.f) continue;
            if (g_ownedEntities.find(veh) == g_ownedEntities.end())
            {
                OwnedEntityInfo info;
                info.netId = g_nextEntityNetId++;
                if (g_nextEntityNetId == 0) g_nextEntityNetId = 1;
                g_ownedEntities[veh] = info;
                int vtype = invoke<int>(GET_ENTITY_TYPE, veh);
                LogFmt("EntitySync track vehicle: handle=%d netId=%d model=0x%X type=%d",
                       veh, info.netId, (uint32_t)invoke<int>(GET_ENTITY_MODEL, veh), vtype);
            }
        }
    }

    // ── REMOVE: entity deleted OR > 200 m away ────────────────────────────────
    // Previous behaviour removed entities that left GET_PED_NEARBY_PEDS range —
    // that caused all but the closest horse to vanish as the player walked away.
    // Now entities stay tracked until they actually disappear or go beyond 200 m.
    constexpr float MAX_TRACK_DIST2 = 200.f * 200.f;
    for (auto eit = g_ownedEntities.begin(); eit != g_ownedEntities.end();)
    {
        if (!invoke<bool>(DOES_ENTITY_EXIST, eit->first))
        {
            LogFmt("EntitySync untrack (deleted): handle=%d netId=%d", eit->first, eit->second.netId);
            eit = g_ownedEntities.erase(eit);
            continue;
        }
        RVector3 ep2 = invoke<RVector3>(GET_ENTITY_COORDS, eit->first, false);
        float edx = ep2.x - myPos.x, edy = ep2.y - myPos.y;
        if (edx*edx + edy*edy > MAX_TRACK_DIST2)
        {
            LogFmt("EntitySync untrack (distance): handle=%d netId=%d", eit->first, eit->second.netId);
            eit = g_ownedEntities.erase(eit);
            continue;
        }
        ++eit;
    }
}

// SendEntitySync — called every ENTITY_SYNC_INTERVAL_MS milliseconds.
// Reads current state (position, velocity) for all tracked owned entities
// and sends one EntitySyncPacket to the server (server broadcasts to others).
static void SendEntitySync()
{
    if (g_ownedEntities.empty()) return;

    EntitySyncPacket pk;
    pk.senderId = g_net.localId;
    pk.count    = 0;

    for (auto& [handle, info] : g_ownedEntities)
    {
        if (pk.count >= ENTITY_SYNC_MAX) break;
        if (!invoke<bool>(DOES_ENTITY_EXIST, handle)) continue;

        RVector3 pos = invoke<RVector3>(GET_ENTITY_COORDS,  handle, false);
        RVector3 vel = invoke<RVector3>(GET_ENTITY_VELOCITY, handle, 0);

        auto& e    = pk.entries[pk.count++];
        e.netId     = info.netId;
        e.modelHash = (uint32_t)invoke<int>(GET_ENTITY_MODEL, handle);
        e.entityType= (uint8_t) invoke<int>(GET_ENTITY_TYPE,  handle);
        e.x = pos.x; e.y = pos.y; e.z = pos.z;
        e.heading   = invoke<float>(GET_ENTITY_HEADING, handle);
        e.velX = vel.x; e.velY = vel.y; e.velZ = vel.z;
    }

    if (pk.count > 0)
        g_net.SendEntitySync(pk);
}

static void SendSync()
{
    int player=invoke<int>(GET_PLAYER_INDEX);
    int ped=invoke<int>(GET_PLAYER_PED,player);
    RVector3 pos=invoke<RVector3>(GET_ENTITY_COORDS,ped,false);
    PlayerSyncPacket pk;
    pk.playerId=g_net.localId; pk.username=g_username;
    pk.x=pos.x; pk.y=pos.y; pk.z=pos.z;
    pk.heading=invoke<float>(GET_ENTITY_HEADING,ped);
    int maxHp=invoke<int>(GET_ENTITY_MAX_HEALTH,ped,false);
    int curHp=invoke<int>(GET_ENTITY_HEALTH,ped,0,0);
    pk.health=maxHp>0?(float)curHp/maxHp:1.f;
    bool onMount = invoke<bool>(IS_PED_ON_MOUNT, ped);
    pk.SetOnHorse(onMount);
    if (onMount)
    {
        // GET_MOUNT(ped) → confirmed hash 0xE7E11B8DCBED1058 from natives.h
        int mountEnt = invoke<int>(GET_MOUNT, ped);
        if (mountEnt > 0)
        {
            pk.horseModelHash = (uint32_t)invoke<int>(GET_ENTITY_MODEL, mountEnt);
            // IS_PED_RUNNING/SPRINTING/WALKING are foot-only natives — on a horse they
            // always return false.  Use physics velocity of the mount for speed detection.
            RVector3 horseVel = invoke<RVector3>(GET_ENTITY_VELOCITY, mountEnt, 0);
            float spd2d = sqrtf(horseVel.x*horseVel.x + horseVel.y*horseVel.y);
            pk.speed = (spd2d > 8.f) ? 3   // gallop  ≈ 29 km/h
                     : (spd2d > 3.f) ? 2   // canter  ≈ 11 km/h
                     : (spd2d > 0.5f) ? 1  // walk/trot
                     : 0;
        }
    }
    else
    {
        bool running  = invoke<bool>(IS_PED_RUNNING,   ped);
        bool sprinting= invoke<bool>(IS_PED_SPRINTING, ped);
        bool walking  = invoke<bool>(IS_PED_WALKING,   ped);
        pk.speed = sprinting ? 3 : running ? 2 : walking ? 1 : 0;
    }
    pk.SetDead(invoke<bool>(IS_PED_DEAD_OR_DYING,ped,true));
    // Read current weapon hash (0 if unarmed / fists) — must be before mounted checks.
    uint32_t weaponHash = 0;
    invoke<bool>(GET_CURRENT_PED_WEAPON, ped, &weaponHash, true, 0, false);
    pk.weaponHash = weaponHash;
    // IS_PED_SHOOTING / IS_PLAYER_FREE_AIMING return false when mounted on a horse
    // in RDR2 v1491.  Fall back to mouse buttons + free-aim check so the aiming /
    // shooting flags are still broadcast correctly from horseback.
    bool isMountedFiring = onMount && weaponHash != 0 &&
                           (GetAsyncKeyState(VK_LBUTTON) & 0x8000) &&
                           invoke<bool>(IS_PLAYER_FREE_AIMING, player);
    bool isMountedAiming = onMount && weaponHash != 0 &&
                           (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
    pk.SetAiming  (invoke<bool>(IS_PLAYER_FREE_AIMING, player) || isMountedAiming);
    pk.SetShooting(invoke<bool>(IS_PED_SHOOTING, ped)          || isMountedFiring);
    // Read actual player ped model (player_zero=Arthur, player_one=John, etc.)
    pk.modelHash  = (uint32_t)invoke<int>(GET_ENTITY_MODEL, ped);
    // _GET_PED_META_OUTFIT_HASH — current clothing preset (Arthur/John outfit).
    // Remote clients use this hash to stream and apply the exact same outfit.
    pk.outfitHash = (uint32_t)invoke<int>(_GET_PED_META_OUTFIT_HASH, ped);
    // Clothing sync: scan on outfit change OR every 150 player syncs (~5s at 30 Hz)
    static uint32_t lastOutfitScan = 0xFFFFFFFF;
    static int scanTimer = 0;
    if (pk.outfitHash != lastOutfitScan || ++scanTimer >= 150)
    {
        if (pk.outfitHash != lastOutfitScan) scanTimer = 0;
        lastOutfitScan = pk.outfitHash;
        ClothingSyncPacket cp;
        cp.senderId = g_net.localId;
        cp.count = ScanActiveComponents(ped, cp.hashes, 25);
        g_net.SendClothing(cp);
    }
    // Horse outfit hash: read the saddle/blanket preset from the actual mount so
    // remote clients can apply the identical outfit on their CREATE_PED clone.
    // 0 when not mounted — remote ignores it in that case.
    if (onMount)
    {
        int mountEnt2 = invoke<int>(GET_MOUNT, ped);
        if (mountEnt2 > 0)
            pk.horseOutfitHash = (uint32_t)invoke<int>(_GET_PED_META_OUTFIT_HASH, mountEnt2);
    }
    g_net.SendSync(pk);
    // Shot event detection
    // On horseback IS_PED_SHOOTING is always false → use the same isMountedFiring
    // flag (LMB held + free-aiming) as the rising-edge trigger.
    static bool wasShooting = false;
    bool isShootingNow = invoke<bool>(IS_PED_SHOOTING, ped) || isMountedFiring;
    if (isShootingNow && !wasShooting)
    {
        SyncEventPacket se;
        se.eventType  = SyncEventType::Shot;
        se.attackerId = g_net.localId;
        se.extraData  = weaponHash;
        se.victimId   = 0;

        // Use camera yaw when available (works on foot and horseback)
        // Falls back to ped heading if camera returns invalid value
        float myH;
        float camYaw = GetCameraYaw();
        if (camYaw > -900.f)
            myH = camYaw * 3.14159265f / 180.0f;
        else
            myH = pk.heading * 3.14159265f / 180.0f;

        // Find the remote player inside the aim cone (attacker-side = no net lag).
        float bestAngle = 0.44f;  // ~25°
        for (auto& [rid, rp2] : g_players)
        {
            if (rp2.pedHandle <= 0) continue;
            RVector3 rpos = invoke<RVector3>(GET_ENTITY_COORDS, rp2.pedHandle, false);
            float rdx = rpos.x - pos.x, rdy = rpos.y - pos.y;
            float rdist2 = rdx*rdx + rdy*rdy;
            if (rdist2 > 10000.f || rdist2 < 0.01f) continue; // > 100 m
            float toTarget = atan2f(rdx, rdy);
            float ad = fabsf(toTarget - myH);
            if (ad > 3.14159265f) ad = 6.2831853f - ad;
            if (ad < bestAngle) { bestAngle = ad; se.victimId = rid; }
        }

        g_net.SendEvent(se);
    }
    wasShooting = isShootingNow;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main script loop
// ─────────────────────────────────────────────────────────────────────────────
static void ScriptMain()
{
    Log("ScriptMain started");

    g_weatherCtrl.Init();
    g_plugins.events.onChatCommand = [](const char* cmd, const char* args) {
        g_weatherCtrl.OnChatCommand(cmd, args);
    };
    g_plugins.events.onTick = []() {
        g_weatherLocked = g_weatherCtrl.weatherLocked;
        g_weatherTarget = g_weatherCtrl.targetHash;
        g_timeLocked    = g_weatherCtrl.timeLocked;
        g_timeHour      = g_weatherCtrl.timeHour;
        g_timeMinute    = g_weatherCtrl.timeMinute;
    };

    g_net.onServerInfo       = OnServerInfo;
    g_net.onPlayerConnect    = OnPlayerConnect;
    g_net.onPlayerDisconnect = OnPlayerDisconnect;
    g_net.onPlayerSync       = OnPlayerSync;
    g_net.onEntitySync       = OnEntitySync;
    g_net.onChatMessage      = OnChatMessage;
    g_net.onWeatherSync      = OnWeatherSync;
    g_net.onSyncEvent        = OnSyncEvent;
    g_net.onClothingSync     = OnClothingSync;
    g_net.onDisconnected     = OnDisconnected;
    g_net.onPluginCommand    = [](const PluginCommandPacket& p) {
        if (g_plugins.events.onChatCommand) g_plugins.events.onChatCommand(p.cmd, p.args);
        g_plugins.DispatchChat(p.cmd, p.args);
    };

    g_menu.onConnect = [&]{
        g_username=g_menu.username; g_serverIp=g_menu.serverIp; g_serverPort=g_menu.serverPort;
        SaveConfig();

        // Always clean up any previous socket (failed attempt, wrong IP, etc.)
        g_net.Disconnect();
        DeleteAllRemotes();
        g_menu.isConnected  = false;
        g_statusBar.connected = false;

        g_chat.AddMessage("Sistema","Conectando a "+g_serverIp+":"+std::to_string(g_serverPort)+"...");
        g_net.Connect(g_serverIp,g_serverPort,g_username);
        g_connectingTimer = 0;   // start timeout countdown
    };
    g_menu.onDisconnect = [&]{
        g_net.Disconnect(); DeleteAllRemotes();
        g_menu.isConnected=false; g_statusBar.connected=false;
        g_chat.AddMessage("Sistema","Desconectado.");
    };

    // Pre-load the remote ped model silently so it's ready before any player connects.
    // Horse models are loaded on-demand when the first horse packet arrives.
    invoke<void>(REQUEST_MODEL, REMOTE_PED_MODEL, false);
    invoke<void>(REQUEST_MODEL, REMOTE_HORSE_FALLBACK, false);
    // Pre-load common horse breeds (avoid freeze on different horse models)
    static const UINT64 horseModels[] = {
        0xB57D0193, // americanstandardbred_black
        0xA1A5E5C5, 0x056E0529, 0x9B77D6E1, 0xA49E1F56,
        0x4701C65C, 0x1B7F3F6A, 0x5E2B131F, 0x6A6F2C53,
    };
    for (auto m : horseModels) invoke<void>(REQUEST_MODEL, m, false);
    // Pre-load common player models to avoid freeze on model change
    invoke<void>(REQUEST_MODEL, 0x39D8E4FA, false); // mp_male
    invoke<void>(REQUEST_MODEL, 0x9E16A7D2, false); // mp_female
    invoke<void>(REQUEST_MODEL, 0x0D7114C9, false); // player_zero (Arthur)
    invoke<void>(REQUEST_MODEL, 0xDB096B10, false); // player_one (John)

    g_chat.AddMessage("Sistema","RDR2Coop v0.1 — F10=menu  T=chat  O=jugadores");
    g_lan.Start(g_serverPort);
    g_lang.Load(g_dllDir+"RDR2Coop\\language\\es.lang");
    int lp=invoke<int>(GET_PLAYER_INDEX);int lped=invoke<int>(GET_PLAYER_PED,lp);
    if(lped>0)g_plugins.LoadAll(g_dllDir+"RDR2Coop\\plugins\\",lped,g_username.c_str(),
        &g_weatherLocked, &g_weatherTarget, &g_lastWeather);
    Log("Entering main loop");

    int frame=0;
    int autoConnectFrame = g_autoConnect ? 180 : -1;
    while (true)
    {
        frame++;
        // ── Auto-connect ─────────────────────────────────────────────────
        if (autoConnectFrame >= 0 && frame >= autoConnectFrame && !g_net.connected)
        {
            autoConnectFrame = -1;
            g_net.Disconnect(); DeleteAllRemotes();
            g_chat.AddMessage("Sistema","Auto-conectando...");
            g_net.Connect(g_serverIp,g_serverPort,g_username);
        }
        if (frame<=3 || frame%600==0)
            LogFmt("Frame %d menuOpen=%d connected=%d",frame,(int)g_menu.isOpen,(int)g_net.connected);

        PollInput();   // GetAsyncKeyState — no keyboard handler needed

        // Ensure the remote-player relationship group exists BEFORE processing any
        // incoming network packets.  Previously this was called only inside SendSync
        // which runs AFTER g_net.Poll — meaning the very first sync packet could
        // spawn a ped before the group was created, leaving it un-targetable forever.
        if (g_net.connected) { InitRemoteGroup(); InitAnimalGroup(); }

        g_net.Poll();

        // ── Connection timeout ────────────────────────────────────────────────
        if (g_connectingTimer >= 0 && !g_net.connected)
        {
            if (++g_connectingTimer >= CONNECT_TIMEOUT_FRAMES)
            {
                g_connectingTimer = -1;
                g_net.Disconnect();
                g_chat.AddMessage("Sistema","Sin respuesta del servidor. Verifica la IP y el puerto.");
            }
        }

        g_lan.Tick();
        if (g_net.connected) UpdateRemotes();
        g_menu.SetLan(g_lan.GetServers());
        g_menu.SetLanDbg(g_lan.GetDebugStr());
        g_menu.Tick();
        g_chat.Tick();
        g_statusBar.Tick();
        g_notify.Tick();
        g_plugins.TickAll(invoke<int>(GET_PLAYER_PED, invoke<int>(GET_PLAYER_INDEX)));

std::vector<PlayerListPanel::Entry> remotes;
        for (auto& [id,p]:g_players) remotes.push_back({p.username,0,false});
        g_playerList.Tick(g_username,g_net.ping,remotes);

        // ── Server timeout ────────────────────────────────────────────────────
        // If connected but no packet received for 10 s → server is gone.
        if (g_net.connected && g_net.lastPacketMs != 0)
        {
            DWORD elapsed = GetTickCount() - g_net.lastPacketMs;
            if (elapsed > 10000)
            {
                Log("Server timeout — no packet for 10s, disconnecting");
                g_net.Disconnect();
                DeleteAllRemotes();
                g_menu.isConnected    = false;
                g_statusBar.connected = false;
                g_chat.AddMessage("Sistema", "Conexion perdida con el servidor.");
            }
        }

        if (g_net.connected)
        {
            // ── Player sync (30 Hz) ───────────────────────────────────────────
            DWORD nowMs = GetTickCount();
            if (nowMs - g_lastPlayerSyncMs >= PLAYER_SYNC_INTERVAL_MS)
            {
                g_lastPlayerSyncMs = nowMs;
                SendSync();
            }

            // ── Entity scan (2 Hz) — discover new/removed mission entities ────
            if (++g_entityScanTimer >= ENTITY_SCAN_INTERVAL)
            {
                g_entityScanTimer = 0;
                ScanOwnedEntities();
            }

            // ── Entity send (15 Hz) — push positions for tracked entities ─────
            if (nowMs - g_lastEntitySyncMs >= ENTITY_SYNC_INTERVAL_MS)
            {
                g_lastEntitySyncMs = nowMs;
                SendEntitySync();
            }
        }

        if (g_chat.isOpen || g_menu.isOpen)
            invoke<void>(DISABLE_ALL_CONTROL_ACTIONS, 0);

        if (g_lastWeather != 0)
        {
            uint32_t w = g_weatherLocked ? g_weatherTarget : g_lastWeather;
            invoke<void>(SET_WEATHER_TYPE, w, true, true, true, 0.0f, true);
        }
        if (g_timeLocked)
            invoke<void>(SET_CLOCK_TIME, g_timeHour, g_timeMinute, 0);

        scriptWait(0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DLL entry point
// ─────────────────────────────────────────────────────────────────────────────
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        char path[MAX_PATH];
        GetModuleFileNameA(hModule,path,MAX_PATH);
        std::string s(path);
        auto pos=s.rfind('\\');
        std::string dir=(pos!=std::string::npos?s.substr(0,pos+1):"");
        g_dllDir = dir;
        std::string modDir = dir + "RDR2Coop\\";
        CreateDirectoryA(modDir.c_str(), NULL);
        CreateDirectoryA((modDir+"language").c_str(), NULL);
        CreateDirectoryA((modDir+"plugins").c_str(), NULL);
        CreateDirectoryA((modDir+"scripts").c_str(), NULL);
        g_configPath = modDir + "RDR2Coop.ini";
        g_log = fopen((modDir+"logs\\RDR2Coop_debug.log").c_str(),"w");
        LogFmt("DLL loaded: %s",path);

        // Single WSAStartup for the whole process lifetime.
        WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);

        LoadConfig(); Log("Config loaded");

        // NOTE: LAN scanner and plugins start from ScriptMain

        scriptRegister(hModule, ScriptMain);
        LogFmt("hDll=%p  scriptRegister=%p  scriptWait=%p  nativeInit=%p  kbRegister=%p",
            (void*)_shook::hDll,
            (void*)_shook::_scriptRegister,
            (void*)_shook::_scriptWait,
            (void*)_shook::_nativeInit,
            (void*)_shook::_kbRegister);
        // Input also handled via GetAsyncKeyState (belt-and-suspenders).
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Log("DLL detach");
        g_net.Disconnect();
        g_lan.Stop();
        DeleteAllRemotes();
        scriptUnregister(hModule);
        WSACleanup(); // process-level cleanup
        if (g_log) { fclose(g_log); g_log=nullptr; }
    }
    return TRUE;
}
