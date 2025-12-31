#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/TLS.hpp>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <intrin.h>

RED4EXT_INLINE RED4ext::TLS* RED4ext::TLS::Get()
{
    return *reinterpret_cast<TLS**>(__readgsqword(0x58));
}

#else
// macOS/ARM64 implementation
// On macOS, we need to find the TLS structure differently.
// The game uses a specific TLS slot that we need to discover at runtime.

#include <pthread.h>
#include <cstdint>

namespace
{
// Thread-local storage key for the game's TLS structure
static pthread_key_t g_tlsKey = 0;
static bool g_tlsKeyInitialized = false;

// The game's TLS pointer - discovered at runtime
static thread_local RED4ext::TLS* g_gameTLS = nullptr;
}

RED4EXT_INLINE RED4ext::TLS* RED4ext::TLS::Get()
{
    // On macOS, we need to discover the game's TLS structure differently
    // than on Windows where it's at a fixed GS segment offset.
    // 
    // For now, return the cached pointer if available.
    // The game initialization hooks will set this up properly.
    //
    // Note: This is a placeholder implementation. The actual TLS discovery
    // will need to be done by hooking game initialization and finding
    // where the TLS structure is allocated.
    
    if (g_gameTLS)
    {
        return g_gameTLS;
    }
    
    // Fallback: return nullptr if TLS not yet initialized
    // Callers should check for nullptr
    return nullptr;
}

// Helper to set the TLS pointer from game hooks
namespace RED4ext
{
namespace Detail
{
inline void SetGameTLS(TLS* aTLS)
{
    g_gameTLS = aTLS;
}
}
}

#endif
