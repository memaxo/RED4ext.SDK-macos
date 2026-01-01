#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>

#include <RED4ext/Detail/WinCompat.hpp>

#include <RED4ext/Api/Sdk.hpp>

namespace RED4ext
{
class RelocBase
{
public:
    static uintptr_t GetImageBase();
};

/**
 * @brief Represent a native function, use this to relocate its address at runtime.
 * @tparam T The type.
 */
template<typename T>
class RelocFunc : private RelocBase
{
public:
    RelocFunc(uintptr_t aOffset)
        : m_address(reinterpret_cast<T>(aOffset + GetImageBase()))
    {
    }

    inline operator T() const
    {
        return m_address;
    }

private:
    T m_address;
};

/**
 * @brief Represent a native pointer, use this to relocate its address at runtime.
 * @tparam T The type.
 */
template<typename T>
class RelocPtr : private RelocBase
{
public:
    RelocPtr(uintptr_t aOffset)
        : m_address(reinterpret_cast<T*>(aOffset + GetImageBase()))
    {
    }

    inline operator T() const
    {
        return *m_address;
    }

    inline T* GetAddr() const
    {
        return m_address;
    }

private:
    T* m_address;
};

/**
 * @brief Represent a native virtual table, use this to relocate its address at runtime.
 * @tparam T The type.
 */
class RelocVtbl : private RelocBase
{
public:
    RelocVtbl(uintptr_t aOffset)
        : m_address(reinterpret_cast<uintptr_t*>(aOffset + GetImageBase()))
    {
    }

    inline operator uintptr_t*() const
    {
        return m_address;
    }

private:
    uintptr_t* m_address;
};

class UniversalRelocBase
{
public:
    static uintptr_t Resolve(uint32_t aHash);

private:
    using QueryFunc_t = void (*)(PluginInfo*);
    using ResolveFunc_t = std::uintptr_t (*)(std::uint32_t);

    static HMODULE GetRED4extModule();

    static ResolveFunc_t InitializeAddressResolverFunction();
    static ResolveFunc_t GetAddressResolverFunction();

    static HMODULE GetCurrentModuleHandle();
    static std::filesystem::path GetCurrentModulePath();

    static QueryFunc_t GetCurrentPluginQueryFunction();
    static bool QueryCurrentPlugin(PluginInfo& aPluginInfo);

    static void ShowErrorAndTerminateProcess(std::wstring_view aMsg, std::uint32_t aLastError,
                                             bool aQueryPluginInfo = true);
};

/**
 * @brief Represent a native function, use this to relocate its address at runtime.
 * @tparam T The type.
 */
template<typename T>
class UniversalRelocFunc : private UniversalRelocBase
{
public:
    UniversalRelocFunc(uint32_t aHash)
#if !defined(_WIN32) && !defined(_WIN64)
        // macOS: Defer address resolution until first use to avoid issues during global init
        : m_hash(aHash)
        , m_address(nullptr)
        , m_resolved(false)
#else
        : m_address(reinterpret_cast<T>(Resolve(aHash)))
#endif
    {
    }

    inline operator T() const
    {
#if !defined(_WIN32) && !defined(_WIN64)
        if (!m_resolved)
        {
            const_cast<UniversalRelocFunc*>(this)->m_address = reinterpret_cast<T>(Resolve(m_hash));
            const_cast<UniversalRelocFunc*>(this)->m_resolved = true;
        }
#endif
        return m_address;
    }
    
    inline bool IsValid() const
    {
#if !defined(_WIN32) && !defined(_WIN64)
        // Trigger resolution if needed
        static_cast<void>(static_cast<T>(*this));
#endif
        return m_address != nullptr;
    }

private:
#if !defined(_WIN32) && !defined(_WIN64)
    uint32_t m_hash;
    mutable T m_address;
    mutable bool m_resolved;
#else
    T m_address;
#endif
};

/**
 * @brief Represent a native pointer, use this to relocate its address at runtime.
 * @tparam T The type.
 */
template<typename T>
class UniversalRelocPtr : private UniversalRelocBase
{
public:
    UniversalRelocPtr(uint32_t aHash)
#if !defined(_WIN32) && !defined(_WIN64)
        // macOS: Defer address resolution until first use
        : m_hash(aHash)
        , m_address(nullptr)
        , m_resolved(false)
#else
        : m_address(reinterpret_cast<T*>(Resolve(aHash)))
#endif
    {
    }

    inline operator T() const
    {
#if !defined(_WIN32) && !defined(_WIN64)
        if (!m_resolved)
        {
            const_cast<UniversalRelocPtr*>(this)->m_address = reinterpret_cast<T*>(Resolve(m_hash));
            const_cast<UniversalRelocPtr*>(this)->m_resolved = true;
        }
        if (!m_address) return T{};  // Return default for unresolved addresses
#endif
        return *m_address;
    }

    inline T* GetAddr() const
    {
#if !defined(_WIN32) && !defined(_WIN64)
        if (!m_resolved)
        {
            const_cast<UniversalRelocPtr*>(this)->m_address = reinterpret_cast<T*>(Resolve(m_hash));
            const_cast<UniversalRelocPtr*>(this)->m_resolved = true;
        }
#endif
        return m_address;
    }

private:
#if !defined(_WIN32) && !defined(_WIN64)
    uint32_t m_hash;
    mutable T* m_address;
    mutable bool m_resolved;
#else
    T* m_address;
#endif
};

/**
 * @brief Represent a native virtual table, use this to relocate its address at runtime.
 * @tparam T The type.
 */
class UniversalRelocVtbl : private UniversalRelocBase
{
public:
    UniversalRelocVtbl(uint32_t aHash)
#if !defined(_WIN32) && !defined(_WIN64)
        // macOS: Defer address resolution until first use
        : m_hash(aHash)
        , m_address(nullptr)
        , m_resolved(false)
#else
        : m_address(reinterpret_cast<uintptr_t*>(Resolve(aHash)))
#endif
    {
    }

    inline operator uintptr_t*() const
    {
#if !defined(_WIN32) && !defined(_WIN64)
        if (!m_resolved)
        {
            const_cast<UniversalRelocVtbl*>(this)->m_address = reinterpret_cast<uintptr_t*>(Resolve(m_hash));
            const_cast<UniversalRelocVtbl*>(this)->m_resolved = true;
        }
#endif
        return m_address;
    }

private:
#if !defined(_WIN32) && !defined(_WIN64)
    uint32_t m_hash;
    mutable uintptr_t* m_address;
    mutable bool m_resolved;
#else
    uintptr_t* m_address;
#endif
};

} // namespace RED4ext

#ifdef RED4EXT_HEADER_ONLY
#include <RED4ext/Relocation-inl.hpp>
#endif
