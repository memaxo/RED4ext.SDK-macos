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

RED4EXT_INLINE bool RED4ext::TLS::IsInitialized()
{
    return Get() != nullptr;
}

#elif defined(__APPLE__)
// macOS implementation
// On macOS we can't read a fixed segment offset like Windows; instead we attempt:
//  1) An exported resolver from the RED4ext runtime (preferred)
//  2) Discovery via pthread TSD (fallback)

#include <atomic>
#include <cstddef>
#include <dlfcn.h>
#include <mutex>
#include <pthread.h>

#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace
{
using GetTlsFunc_t = RED4ext::TLS* (*)();

static std::once_flag g_tlsOnce;
static GetTlsFunc_t g_getTlsFunc = nullptr;

// Optional explicit TLS pointer (set by hooks if available)
static thread_local RED4ext::TLS* g_gameTLS = nullptr;

static std::atomic<int> g_tlsPthreadKey{-1};

struct VmRegion
{
    mach_vm_address_t start = 0;
    mach_vm_address_t end = 0;
    vm_prot_t protection = 0;
};

static bool QueryRegion(const void* aPtr, VmRegion& aOut)
{
    if (!aPtr)
        return false;

    const auto addr = static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(aPtr));
    if (addr == 0)
        return false;

    mach_vm_address_t query = addr;
    mach_vm_size_t regionSize = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t objectName = MACH_PORT_NULL;

    const auto kr = mach_vm_region(mach_task_self(), &query, &regionSize, VM_REGION_BASIC_INFO_64,
                                   reinterpret_cast<vm_region_info_t>(&info), &infoCount, &objectName);
    if (kr != KERN_SUCCESS)
        return false;

    aOut.start = query;
    aOut.end = query + regionSize;
    aOut.protection = info.protection;
    return true;
}

static bool IsReadableRange(const void* aPtr, std::size_t aSize)
{
    if (!aPtr || aSize == 0)
        return false;

    VmRegion region{};
    if (!QueryRegion(aPtr, region))
        return false;

    if ((region.protection & VM_PROT_READ) == 0)
        return false;

    const auto start = static_cast<mach_vm_address_t>(reinterpret_cast<uintptr_t>(aPtr));
    const auto end = start + static_cast<mach_vm_address_t>(aSize);
    if (end < start)
        return false;

    return start >= region.start && end <= region.end;
}

static bool IsExecutableAddress(const void* aPtr)
{
    VmRegion region{};
    if (!QueryRegion(aPtr, region))
        return false;

    return (region.protection & VM_PROT_EXECUTE) != 0;
}

static bool LooksLikeGameTls(RED4ext::TLS* aPtr)
{
    if (!aPtr)
        return false;

    VmRegion region{};
    if (!QueryRegion(aPtr, region))
        return false;

    // TLS is expected to be writable thread-local state.
    if ((region.protection & (VM_PROT_READ | VM_PROT_WRITE)) != (VM_PROT_READ | VM_PROT_WRITE))
        return false;

    if ((region.protection & VM_PROT_EXECUTE) != 0)
        return false;

    if (!IsReadableRange(aPtr, sizeof(RED4ext::TLS)))
        return false;

    // Heuristic: many internal engine structs are polymorphic; if the first word looks like a vtable pointer,
    // require it to point into executable memory.
    const auto firstWord = *reinterpret_cast<const uintptr_t*>(aPtr);
    if (firstWord != 0 && IsExecutableAddress(reinterpret_cast<const void*>(firstWord)))
        return true;

    // Fallback heuristic: check that the first few words contain a reasonable density of pointer-like values.
    const auto* words = reinterpret_cast<const uintptr_t*>(aPtr);
    constexpr std::size_t kSampleWords = 16;
    std::size_t pointerLike = 0;

    for (std::size_t i = 0; i < kSampleWords; ++i)
    {
        const uintptr_t v = words[i];
        if (v == 0)
            continue;

        if ((v & 0x7) != 0)
            continue;

        if (v < 0x10000)
            continue;

        if (IsReadableRange(reinterpret_cast<const void*>(v), sizeof(uintptr_t)))
            ++pointerLike;
    }

    return pointerLike >= 3;
}

static RED4ext::TLS* GetFromPthreadKey(int aKey)
{
    if (aKey < 0)
        return nullptr;

    return static_cast<RED4ext::TLS*>(pthread_getspecific(static_cast<pthread_key_t>(aKey)));
}

static RED4ext::TLS* DiscoverTlsViaPthreadTsd()
{
    for (int key = 0; key < PTHREAD_KEYS_MAX; ++key)
    {
        auto* ptr = GetFromPthreadKey(key);
        if (!ptr)
            continue;

        if (!LooksLikeGameTls(ptr))
            continue;

        // Persist the discovered key for subsequent calls.
        int expected = -1;
        (void)g_tlsPthreadKey.compare_exchange_strong(expected, key);
        return ptr;
    }

    return nullptr;
}

static void InitTlsResolver()
{
    // Optional integration point: if the macOS RED4ext runtime exports a TLS getter,
    // use it to make TLS::Get() self-contained for plugins.
    static constexpr auto moduleName = "RED4ext.dylib";
    const auto handle = dlopen(moduleName, RTLD_LAZY | RTLD_NOLOAD);
    if (!handle)
        return;

    if (auto sym = dlsym(handle, "RED4ext_GetTLS"))
    {
        g_getTlsFunc = reinterpret_cast<GetTlsFunc_t>(sym);
        return;
    }

    if (auto sym = dlsym(handle, "RED4ext_GetTls"))
    {
        g_getTlsFunc = reinterpret_cast<GetTlsFunc_t>(sym);
        return;
    }
}
}

RED4EXT_INLINE RED4ext::TLS* RED4ext::TLS::Get()
{
    if (g_gameTLS)
        return g_gameTLS;

    std::call_once(g_tlsOnce, InitTlsResolver);
    if (g_getTlsFunc)
        return g_getTlsFunc();

    if (const int key = g_tlsPthreadKey.load(std::memory_order_relaxed); key >= 0)
    {
        if (auto* ptr = GetFromPthreadKey(key))
            return ptr;

        // Key was discovered, but this thread doesn't have a value yet.
        return nullptr;
    }

    return DiscoverTlsViaPthreadTsd();
    
}

RED4EXT_INLINE bool RED4ext::TLS::IsInitialized()
{
    return Get() != nullptr;
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

#else

RED4EXT_INLINE RED4ext::TLS* RED4ext::TLS::Get()
{
    return nullptr;
}

RED4EXT_INLINE bool RED4ext::TLS::IsInitialized()
{
    return false;
}

#endif
