#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/Mutex.hpp>
#endif

#if defined(_WIN32) || defined(_WIN64)

RED4EXT_INLINE RED4ext::Mutex::Mutex()
{
    InitializeCriticalSection(&m_cs);
}

RED4EXT_INLINE void RED4ext::Mutex::Lock()
{
    EnterCriticalSection(&m_cs);
}

RED4EXT_INLINE void RED4ext::Mutex::Unlock()
{
    LeaveCriticalSection(&m_cs);
}

#else

RED4EXT_INLINE RED4ext::Mutex::Mutex()
{
    pthread_mutex_init(&m_mutex, nullptr);
}

RED4EXT_INLINE void RED4ext::Mutex::Lock()
{
    pthread_mutex_lock(&m_mutex);
}

RED4EXT_INLINE void RED4ext::Mutex::Unlock()
{
    pthread_mutex_unlock(&m_mutex);
}

#endif

RED4EXT_INLINE void RED4ext::Mutex::lock()
{
    Lock();
}

RED4EXT_INLINE void RED4ext::Mutex::unlock()
{
    Unlock();
}
