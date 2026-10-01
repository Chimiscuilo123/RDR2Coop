#pragma once
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include <type_traits>

// ─────────────────────────────────────────────────────────────────────────────
// ScriptHookRDR2 V2 API — resolved at runtime via GetProcAddress.
// No ScriptHookRDR2.lib needed; only ScriptHookRDR2.dll at runtime.
// ─────────────────────────────────────────────────────────────────────────────

typedef void (*LP_SCRIPT_MAIN)();
typedef void (*LP_KEYBOARD_HANDLER)(DWORD key, WORD repeats, BYTE scanCode,
    BOOL isExtended, BOOL isWithAlt, BOOL wasDownBefore, BOOL isUpNow);

// ── Function pointer types ────────────────────────────────────────────────────
typedef void    (*fn_scriptRegister)  (HMODULE, LP_SCRIPT_MAIN);
typedef void    (*fn_scriptUnregister)(HMODULE);
typedef void    (*fn_scriptWait)      (DWORD);
typedef void    (*fn_keyboardHandlerRegister)  (LP_KEYBOARD_HANDLER);
typedef void    (*fn_keyboardHandlerUnregister)(LP_KEYBOARD_HANDLER);
typedef void    (*fn_nativeInit)      (UINT64);
typedef void    (*fn_nativePush64)    (UINT64);
typedef PUINT64 (*fn_nativeCall)      ();

// World-entity pool helpers exported by ScriptHookRDR2.dll (C linkage, no mangling).
// These iterate the game's entity pool and fill an int array with handles.
typedef int (*fn_worldGetAllPeds)    (int*, int);
typedef int (*fn_worldGetAllVehicles)(int*, int);
typedef int (*fn_worldGetAllObjects) (int*, int);

// ── Resolved pointers ─────────────────────────────────────────────────────────
namespace _shook
{
    inline HMODULE hDll = nullptr;

    inline fn_scriptRegister          _scriptRegister          = nullptr;
    inline fn_scriptUnregister        _scriptUnregister        = nullptr;
    inline fn_scriptWait              _scriptWait              = nullptr;
    inline fn_keyboardHandlerRegister _kbRegister              = nullptr;
    inline fn_keyboardHandlerUnregister _kbUnregister          = nullptr;
    inline fn_nativeInit              _nativeInit              = nullptr;
    inline fn_nativePush64            _nativePush64            = nullptr;
    inline fn_nativeCall              _nativeCall              = nullptr;

    inline fn_worldGetAllPeds         _worldGetAllPeds         = nullptr;
    inline fn_worldGetAllVehicles     _worldGetAllVehicles     = nullptr;
    inline fn_worldGetAllObjects      _worldGetAllObjects      = nullptr;

    inline void Resolve()
    {
        hDll = GetModuleHandleA("ScriptHookRDR2.dll");
        if (!hDll) return;

        // ScriptHookRDR2 exports use C++ mangled names (MSVC compiled).
        // These are the exact decorated names from the DLL export table.
        _scriptRegister   = (fn_scriptRegister)  GetProcAddress(hDll, "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
        _scriptUnregister = (fn_scriptUnregister)GetProcAddress(hDll, "?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
        _scriptWait       = (fn_scriptWait)      GetProcAddress(hDll, "?scriptWait@@YAXK@Z");
        _nativeInit       = (fn_nativeInit)      GetProcAddress(hDll, "?nativeInit@@YAX_K@Z");
        _nativePush64     = (fn_nativePush64)    GetProcAddress(hDll, "?nativePush64@@YAX_K@Z");
        _nativeCall       = (fn_nativeCall)      GetProcAddress(hDll, "?nativeCall@@YAPEA_KXZ");
        _kbRegister       = (fn_keyboardHandlerRegister)  GetProcAddress(hDll, "?keyboardHandlerRegister@@YAXP6AXKGEHHHH@Z@Z");
        _kbUnregister     = (fn_keyboardHandlerUnregister)GetProcAddress(hDll, "?keyboardHandlerUnregister@@YAXP6AXKGEHHHH@Z@Z");
        // World-pool helpers — plain C exports, no name mangling.
        _worldGetAllPeds     = (fn_worldGetAllPeds)    GetProcAddress(hDll, "worldGetAllPeds");
        _worldGetAllVehicles = (fn_worldGetAllVehicles)GetProcAddress(hDll, "worldGetAllVehicles");
        _worldGetAllObjects  = (fn_worldGetAllObjects) GetProcAddress(hDll, "worldGetAllObjects");
    }
}

// ── Public API wrappers ───────────────────────────────────────────────────────

inline void scriptRegister(HMODULE mod, LP_SCRIPT_MAIN fn)
{
    _shook::Resolve();
    if (_shook::_scriptRegister) _shook::_scriptRegister(mod, fn);
}

inline void scriptUnregister(HMODULE mod)
{
    if (_shook::_scriptUnregister) _shook::_scriptUnregister(mod);
}

inline void scriptWait(DWORD ms)
{
    if (_shook::_scriptWait) _shook::_scriptWait(ms);
}

inline void keyboardHandlerRegister(LP_KEYBOARD_HANDLER fn)
{
    if (_shook::_kbRegister) _shook::_kbRegister(fn);
}

inline void keyboardHandlerUnregister(LP_KEYBOARD_HANDLER fn)
{
    if (_shook::_kbUnregister) _shook::_kbUnregister(fn);
}

// ── Native call helpers ───────────────────────────────────────────────────────

inline void nativeInit(UINT64 hash)
{
    if (_shook::_nativeInit) _shook::_nativeInit(hash);
}

inline void nativePush64(UINT64 val)
{
    if (_shook::_nativePush64) _shook::_nativePush64(val);
}

inline PUINT64 nativeCall()
{
    return _shook::_nativeCall ? _shook::_nativeCall() : nullptr;
}

// ── Argument push specializations ────────────────────────────────────────────

template<typename T>
inline void push(T val)
{
    UINT64 buf = 0;
    memcpy(&buf, &val, sizeof(T));
    nativePush64(buf);
}

template<>
inline void push<bool>(bool val)   { nativePush64((UINT64)val); }
template<>
inline void push<int>(int val)     { nativePush64((UINT64)val); }
template<>
inline void push<float>(float val) { UINT64 b=0; memcpy(&b,&val,4); nativePush64(b); }
template<>
inline void push<const char*>(const char* val) { nativePush64((UINT64)val); }
template<>
inline void push<UINT64>(UINT64 val) { nativePush64(val); }

// ── invoke<ReturnType>(hash, args...) ────────────────────────────────────────

template<typename Ret, typename... Args>
inline typename std::enable_if<!std::is_void<Ret>::value, Ret>::type
invoke(UINT64 hash, Args... args)
{
    nativeInit(hash);
    (push(args), ...);
    PUINT64 res = nativeCall();
    Ret out{};
    if (res) memcpy(&out, res, sizeof(Ret));
    return out;
}

template<typename Ret = void, typename... Args>
inline typename std::enable_if<std::is_void<Ret>::value>::type
invoke(UINT64 hash, Args... args)
{
    nativeInit(hash);
    (push(args), ...);
    nativeCall();
}

// ── World-pool wrappers ───────────────────────────────────────────────────────

inline int worldGetAllPeds(int* arr, int arrSize)
{
    return _shook::_worldGetAllPeds ? _shook::_worldGetAllPeds(arr, arrSize) : 0;
}

inline int worldGetAllObjects(int* arr, int arrSize)
{
    return _shook::_worldGetAllObjects ? _shook::_worldGetAllObjects(arr, arrSize) : 0;
}

inline int worldGetAllVehicles(int* arr, int arrSize)
{
    return _shook::_worldGetAllVehicles ? _shook::_worldGetAllVehicles(arr, arrSize) : 0;
}
