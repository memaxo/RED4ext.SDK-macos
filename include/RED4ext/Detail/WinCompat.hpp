#pragma once

#include <cstdint>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else

#ifndef RED4EXT_PLATFORM_MACOS
#define RED4EXT_PLATFORM_MACOS
#endif

// Minimal Windows types for SDK compatibility
using HMODULE = void*;
using HANDLE = void*;
using BOOL = int;
using DWORD = uint32_t;
using WORD = uint16_t;
using BYTE = uint8_t;
using UINT = uint32_t;
using UINT64 = uint64_t;
using WCHAR = wchar_t;
using LPWSTR = wchar_t*;
using LPCWSTR = const wchar_t*;
using LPVOID = void*;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#include <atomic>
#include <thread>

namespace Platform
{
inline void SwitchToThread()
{
    std::this_thread::yield();
}
}

using namespace Platform;

// Interlocked functions - standard sizes
#define InterlockedIncrement(ptr) __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define InterlockedDecrement(ptr) __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST)
#define InterlockedExchangeAdd(ptr, val) __atomic_fetch_add(ptr, val, __ATOMIC_SEQ_CST)
#define InterlockedCompareExchange(ptr, val, comp) ({ \
    auto _comp = comp; \
    __atomic_compare_exchange_n(ptr, &_comp, val, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    _comp; \
})
#define InterlockedExchange(ptr, val) __atomic_exchange_n(ptr, val, __ATOMIC_SEQ_CST)

// 64-bit Interlocked functions
#define InterlockedIncrement64(ptr) __atomic_add_fetch((int64_t*)(ptr), 1, __ATOMIC_SEQ_CST)
#define InterlockedDecrement64(ptr) __atomic_sub_fetch((int64_t*)(ptr), 1, __ATOMIC_SEQ_CST)
#define InterlockedExchangeAdd64(ptr, val) __atomic_fetch_add((int64_t*)(ptr), (int64_t)(val), __ATOMIC_SEQ_CST)
#define InterlockedCompareExchange64(ptr, val, comp) ({ \
    int64_t _comp64 = (int64_t)(comp); \
    __atomic_compare_exchange_n((int64_t*)(ptr), &_comp64, (int64_t)(val), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    _comp64; \
})
#define InterlockedExchange64(ptr, val) __atomic_exchange_n((int64_t*)(ptr), (int64_t)(val), __ATOMIC_SEQ_CST)

// 8-bit Interlocked functions
#define _InterlockedCompareExchange8(ptr, val, comp) ({ \
    auto _comp = (int8_t)comp; \
    __atomic_compare_exchange_n((int8_t*)ptr, &_comp, (int8_t)val, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    _comp; \
})
#define InterlockedExchange8(ptr, val) __atomic_exchange_n((int8_t*)ptr, (int8_t)val, __ATOMIC_SEQ_CST)
#define _InterlockedExchangeAdd8(ptr, val) __atomic_fetch_add((int8_t*)ptr, (int8_t)val, __ATOMIC_SEQ_CST)

// MSVC __declspec compatibility
// We need to handle multiple __declspec specifiers:
// - __declspec(align(x)) -> __attribute__((aligned(x)))
// - __declspec(noinline) -> __attribute__((noinline))
// - __declspec(dllexport) -> __attribute__((visibility("default")))
// - __declspec(dllimport) -> (nothing needed for macOS)

// Helper macros to extract alignment value
#define RED4EXT_DECLSPEC_align(x) __attribute__((aligned(x)))
#define RED4EXT_DECLSPEC_noinline __attribute__((noinline))
#define RED4EXT_DECLSPEC_dllexport __attribute__((visibility("default")))
#define RED4EXT_DECLSPEC_dllimport /* nothing */
#define RED4EXT_DECLSPEC_novtable /* nothing */

// Main __declspec macro - concatenates with the specifier to pick the right handler
#define __declspec(x) RED4EXT_DECLSPEC_##x

// Windows module functions
#include <dlfcn.h>
#include <mach-o/dyld.h>

inline HMODULE GetModuleHandle(const char* lpModuleName)
{
    if (lpModuleName == nullptr)
    {
        // Return handle to main executable
        return (HMODULE)_dyld_get_image_header(0);
    }
    return dlopen(lpModuleName, RTLD_LAZY | RTLD_NOLOAD);
}

inline HMODULE GetModuleHandleW(const wchar_t* lpModuleName)
{
    if (lpModuleName == nullptr)
    {
        return GetModuleHandle(nullptr);
    }
    // Convert wchar_t to char (simplified)
    std::string narrow;
    for (const wchar_t* p = lpModuleName; *p; ++p)
    {
        narrow += static_cast<char>(*p & 0xFF);
    }
    return GetModuleHandle(narrow.c_str());
}

// Memory alignment functions
inline void* _aligned_malloc(size_t size, size_t alignment)
{
    void* ptr = nullptr;
    posix_memalign(&ptr, alignment, size);
    return ptr;
}

inline void _aligned_free(void* ptr)
{
    free(ptr);
}

// Note: GetLastError and SetLastError should be provided by the application
// (e.g., via Platform namespace) to avoid conflicts.

#define MAX_PATH 1024
#define ERROR_INSUFFICIENT_BUFFER 122L

#endif
