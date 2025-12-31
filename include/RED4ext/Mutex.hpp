#pragma once

#include <RED4ext/Common.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace RED4ext
{
struct Mutex
{
    Mutex();
    Mutex(const Mutex&) = delete;
    Mutex(Mutex&&) = delete;
    Mutex& operator=(const Mutex&) = delete;
    Mutex& operator=(Mutex&&) = delete;

    void Lock();
    void Unlock();

    // --------------------------------------------
    // -- support for lock_guard --
    // --------------------------------------------

    void lock();
    void unlock();

private:
#if defined(_WIN32) || defined(_WIN64)
    CRITICAL_SECTION m_cs;
};
RED4EXT_ASSERT_SIZE(Mutex, 40);
#else
    pthread_mutex_t m_mutex;
};
// Note: pthread_mutex_t is 64 bytes on macOS, larger than Windows CRITICAL_SECTION (40 bytes)
// We cannot statically assert the same size on macOS
#endif
} // namespace RED4ext

#ifdef RED4EXT_HEADER_ONLY
#include <RED4ext/Mutex-inl.hpp>
#endif
