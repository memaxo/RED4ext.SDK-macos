#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/SpinLock.hpp>
#endif

#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <RED4ext/Detail/WinCompat.hpp>
#endif

RED4EXT_INLINE RED4ext::SpinLock::SpinLock()
    : state(0)
{
}

RED4EXT_INLINE bool RED4ext::SpinLock::TryLock()
{
    return InterlockedExchange8(&state, 1) == 0;
}

RED4EXT_INLINE void RED4ext::SpinLock::Lock()
{
    uint32_t loopCount = 0;
    while (true)
    {
        if (TryLock())
            break;

        if (loopCount >= 16)
            SwitchToThread();
        ++loopCount;
    }
}

RED4EXT_INLINE void RED4ext::SpinLock::Unlock()
{
    InterlockedExchange8(&state, 0);
}

// ----------------------------
// -- support for lock_guard --
// ----------------------------

RED4EXT_INLINE bool RED4ext::SpinLock::try_lock()
{
    return TryLock();
}

RED4EXT_INLINE void RED4ext::SpinLock::lock()
{
    Lock();
}

RED4EXT_INLINE void RED4ext::SpinLock::unlock()
{
    Unlock();
}
