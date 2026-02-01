#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/Relocation.hpp>
#endif

#include <mutex>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <charconv>
#include <cstdlib>
#include <utility>
#include <system_error>
#include <atomic>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <limits.h>
#include <codecvt>
#include <locale>
#endif

#include <RED4ext/Api/SemVer.hpp>
#include <RED4ext/Common.hpp>
#include <RED4ext/Detail/Memory.hpp>

RED4EXT_INLINE uintptr_t RED4ext::RelocBase::GetImageBase()
{
#if defined(_WIN32) || defined(_WIN64)
    static const auto base = std::bit_cast<uintptr_t>(GetModuleHandle(nullptr));
#else
    static const auto base = std::bit_cast<uintptr_t>(_dyld_get_image_header(0));
#endif
    return base;
}

RED4EXT_INLINE
uintptr_t RED4ext::UniversalRelocBase::Resolve(uint32_t aHash)
{
    if constexpr (Detail::AddressResolverOverride<uint32_t>::value)
    {
        return Detail::AddressResolverOverride<uint32_t>::Resolve(aHash);
    }

// ============================================================================
// macOS Address Resolution
// ============================================================================
// On macOS, addresses are resolved from a JSON database file instead of using
// the Windows Address Library. The database maps FNV1a hash values to offsets
// within the game's __TEXT segment.
//
// Database Format (cyberpunk2077_addresses.json):
// {
//   "version": "1.0",
//   "game_version": "2.3.1",
//   "stats": { "total": 126, "resolved": 126, "unresolved": 0 },
//   "Addresses": [
//     { "hash": "1234567890", "offset": "1:0xABCDEF00" }
//   ]
// }
//
// Offset format: "segment:0xOFFSET" where segment is always 1 (__TEXT)
//
// Search paths (in order):
//   1. $RED4EXT_SDK_ADDRESS_DB environment variable
//   2. Same directory as the plugin (.dylib)
//   3. red4ext/ or red4ext/bin/x64/ relative to plugin
//   4. Same directory as the game executable
//   5. red4ext/ or red4ext/bin/x64/ relative to executable
//
// Validation: Run scripts/check_addresses.py --strict
// ============================================================================

#if !defined(_WIN32) && !defined(_WIN64)
    struct AddressDb
    {
        std::once_flag initOnce;
        std::unordered_map<std::uint32_t, std::uintptr_t> offsets;
        bool loaded{false};
        std::filesystem::path path;
    };

    /// Parse a decimal string to uint32_t. Returns false on parse failure.
    auto tryParseU32 = [](std::string_view aStr, std::uint32_t& aOut) -> bool
    {
        aOut = 0;
        const char* begin = aStr.data();
        const char* end = aStr.data() + aStr.size();
        auto res = std::from_chars(begin, end, aOut, 10);
        return res.ec == std::errc{} && res.ptr == end;
    };

    /// Parse offset string in format "segment:0xHEX" (segment must be 1 for __TEXT).
    /// Returns false if format is invalid or segment != 1.
    auto tryParseOffset = [&tryParseU32](std::string_view aStr, std::uintptr_t& aOut) -> bool
    {
        // Format: segment:0xHEX (segment 1 is __TEXT)
        aOut = 0;
        const auto colon = aStr.find(':');
        if (colon == std::string_view::npos)
            return false;

        std::uint32_t segment = 0;
        if (!tryParseU32(aStr.substr(0, colon), segment))
            return false;

        auto rest = aStr.substr(colon + 1);
        if (rest.size() < 3 || rest[0] != '0' || (rest[1] != 'x' && rest[1] != 'X'))
            return false;

        rest.remove_prefix(2);
        std::uintptr_t value = 0;
        auto res = std::from_chars(rest.data(), rest.data() + rest.size(), value, 16);
        if (res.ec != std::errc{} || res.ptr != rest.data() + rest.size())
            return false;

        // Only segment 1 (__TEXT) is supported for macOS game executables
        if (segment != 1)
            return false;

        aOut = value;
        return true;
    };

    /// Extract a quoted string value that follows a given key in JSON.
    /// Example: Given text '{"hash":"123"}' and key '"hash"', returns "123".
    /// Returns empty string_view and npos if not found or format is invalid.
    auto extractQuotedValueAfterKey = [](std::string_view aText, std::string_view aKey, size_t aFrom) ->
        std::pair<std::string_view, size_t>
    {
        const auto keyPos = aText.find(aKey, aFrom);
        if (keyPos == std::string_view::npos)
            return {{}, std::string_view::npos};

        auto pos = aText.find(':', keyPos + aKey.size());
        if (pos == std::string_view::npos)
            return {{}, std::string_view::npos};

        pos = aText.find('"', pos);
        if (pos == std::string_view::npos)
            return {{}, std::string_view::npos};

        const auto start = pos + 1;
        const auto end = aText.find('"', start);
        if (end == std::string_view::npos)
            return {{}, std::string_view::npos};

        return {aText.substr(start, end - start), end + 1};
    };

    auto loadAddressDb = [&]() -> AddressDb&
    {
        static AddressDb db;
        std::call_once(db.initOnce,
                       [&]()
                       {
                           auto tryLoadFromPath = [&](const std::filesystem::path& aPath) -> bool
                           {
                               std::ifstream file(aPath);
                               if (!file)
                                   return false;

                               std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                               if (contents.empty())
                                   return false;

                               std::string_view text(contents);
                               size_t pos = 0;
                               while (true)
                               {
                                   auto [hashStr, hashNext] = extractQuotedValueAfterKey(text, "\"hash\"", pos);
                                   if (hashNext == std::string_view::npos)
                                       break;
                                   auto [offStr, offNext] = extractQuotedValueAfterKey(text, "\"offset\"", hashNext);
                                   if (offNext == std::string_view::npos)
                                       break;

                                   std::uint32_t hash = 0;
                                   std::uintptr_t offset = 0;
                                   if (tryParseU32(hashStr, hash) && tryParseOffset(offStr, offset))
                                   {
                                       db.offsets.emplace(hash, offset);
                                   }

                                   pos = offNext;
                               }

                               if (!db.offsets.empty())
                               {
                                   db.loaded = true;
                                   db.path = aPath;
                                   return true;
                               }

                               return false;
                           };

                           const char* envPath = std::getenv("RED4EXT_SDK_ADDRESS_DB");
                           if (envPath && *envPath)
                           {
                               if (tryLoadFromPath(envPath))
                                   return;
                           }

                           static constexpr auto fileName = "cyberpunk2077_addresses.json";

                           auto tryLoadFromRed4extRootNear = [&](std::filesystem::path aStart) -> bool
                           {
                               for (int i = 0; i < 10 && !aStart.empty(); ++i)
                               {
                                   std::filesystem::path red4extRoot;
                                   if (aStart.filename() == "red4ext")
                                   {
                                       red4extRoot = aStart;
                                   }
                                   else
                                   {
                                       auto candidate = aStart / "red4ext";
                                       if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate))
                                       {
                                           red4extRoot = candidate;
                                       }
                                   }

                                   if (!red4extRoot.empty())
                                   {
                                       if (tryLoadFromPath(red4extRoot / fileName))
                                           return true;
                                       if (tryLoadFromPath(red4extRoot / "bin" / "x64" / fileName))
                                           return true;
                                   }

                                   aStart = aStart.parent_path();
                               }
                               return false;
                           };

                           const auto modulePath = GetCurrentModulePath();
                           if (!modulePath.empty())
                           {
                               if (tryLoadFromPath(modulePath.parent_path() / fileName))
                                   return;

                               // Common layout: <game>/red4ext/plugins/<PluginName>/<plugin>.dylib
                               if (tryLoadFromRed4extRootNear(modulePath.parent_path()))
                                   return;
                           }

                           char exePathBuf[PATH_MAX] = {};
                           std::uint32_t exeSize = sizeof(exePathBuf);
                           if (_NSGetExecutablePath(exePathBuf, &exeSize) == 0)
                           {
                               const auto exeDir = std::filesystem::path(exePathBuf).parent_path();
                               if (tryLoadFromPath(exeDir / fileName))
                                   return;

                               // Also try <game>/red4ext/... by walking up from the executable directory.
                               if (tryLoadFromRed4extRootNear(exeDir))
                                   return;
                           }
                       });
        return db;
    };

    const auto base = RelocBase::GetImageBase();
    auto& db = loadAddressDb();
    if (db.loaded)
    {
        static std::once_flag loadedOnce;
        std::call_once(loadedOnce,
                       [&]()
                       {
                           std::cerr << "[RED4ext.SDK] Loaded " << db.offsets.size() << " address entries from "
                                     << db.path.string() << "\n";
                       });

        const auto it = db.offsets.find(aHash);
        if (it != db.offsets.end() && it->second != 0)
        {
            return base + it->second;
        }
    }

    static std::atomic_uint32_t missingCount{0};
    const auto missingIdx = ++missingCount;
    if (missingIdx <= 10)
    {
        std::cerr << "[RED4ext.SDK] Missing address hash 0x" << std::hex << aHash << std::dec;
        if (db.loaded)
        {
            std::cerr << " (not present or offset=0 in " << db.path.string() << ")";
        }
        std::cerr << "\n";
        if (missingIdx == 10)
        {
            std::cerr << "[RED4ext.SDK] Further missing-hash logs suppressed." << "\n";
        }
    }

    // Fallback: ask RED4ext for the address if the runtime provides a resolver.
    const auto resolveFunc = GetAddressResolverFunction();
    const auto address = resolveFunc(aHash);
    if (address == 0)
    {
        static std::once_flag warnOnce;
        std::call_once(warnOnce,
                       [&]()
                       {
                           std::cerr
                               << "[RED4ext.SDK] macOS address resolution returned 0 for at least one hash. "
                                  "Ensure cyberpunk2077_addresses.json is present and matches the game version.\n";
                       });
    }
    return address;
#else
    const auto resolveFunc = GetAddressResolverFunction();

    auto address = resolveFunc(aHash);
    if (address == 0)
    {
        std::wostringstream stream;
        stream << L"Failed to find the address for the hash (" << std::dec << aHash << ") provided by the plugin.\n"
               << L"This issue is likely caused by the mod using an incorrect or outdated hash.";

        ShowErrorAndTerminateProcess(stream.str(), 0);
    }

    return address;
#endif
}

RED4EXT_INLINE HMODULE RED4ext::UniversalRelocBase::GetRED4extModule()
{
#if defined(_WIN32) || defined(_WIN64)
    static constexpr auto moduleName = L"RED4ext.dll";
    const auto handle = GetModuleHandleW(moduleName);
#else
    static constexpr auto moduleName = "RED4ext.dylib";
    const auto handle = dlopen(moduleName, RTLD_LAZY | RTLD_NOLOAD);
#endif

    if (!handle)
    {
        static constexpr auto msg =
#if defined(_WIN32) || defined(_WIN64)
            L"The mod you are using could not locate the necessary module (i.e. RED4ext.dll) in the "
            L"loaded modules, which is required by the mod to resolve addresses correctly.\n"
#else
            L"The mod you are using could not locate the necessary module (i.e. RED4ext.dylib) in the "
            L"loaded modules, which is required by the mod to resolve addresses correctly.\n"
#endif
            L"This may occur if RED4ext is not properly loaded into the current process.\n"
            L"\n"
            L"Please ensure that RED4ext is correctly installed.\n"
            L"\n"
            L"If you are the mod's developer, verify that your mod was loaded by RED4ext. "
            L"Alternatively, you may need to provide your own address resolver.";

#if defined(_WIN32) || defined(_WIN64)
        ShowErrorAndTerminateProcess(msg, GetLastError());
#else
        ShowErrorAndTerminateProcess(msg, static_cast<uint32_t>(errno));
#endif
    }

    return handle;
}

RED4EXT_INLINE RED4ext::UniversalRelocBase::ResolveFunc_t RED4ext::UniversalRelocBase::
    InitializeAddressResolverFunction()
{
    static constexpr auto procName = "RED4ext_ResolveAddress";

    const auto handle = GetRED4extModule();

#if defined(_WIN32) || defined(_WIN64)
    const auto func = std::bit_cast<ResolveFunc_t>(GetProcAddress(handle, procName));
#else
    const auto func = std::bit_cast<ResolveFunc_t>(dlsym(handle, procName));
#endif

    if (func == nullptr)
    {
        static constexpr auto msg =
            L"The mod you are using is unable to find the required address resolver function from RED4ext.\n"
            L"This may occur if RED4ext is not properly loaded, OR if the mod is incompatible with the current "
            L"version of RED4ext.\n"
            L"\n"
            L"Please ensure that RED4ext is correctly installed AND that both RED4ext and the mod are "
            L"up-to-date.";

#if defined(_WIN32) || defined(_WIN64)
        ShowErrorAndTerminateProcess(msg, GetLastError());
#else
        ShowErrorAndTerminateProcess(msg, static_cast<uint32_t>(errno));
#endif
    }

    return func;
}

RED4EXT_INLINE RED4ext::UniversalRelocBase::ResolveFunc_t RED4ext::UniversalRelocBase::GetAddressResolverFunction()
{
    static const ResolveFunc_t func = InitializeAddressResolverFunction();
    return func;
}

RED4EXT_INLINE HMODULE RED4ext::UniversalRelocBase::GetCurrentModuleHandle()
{
#if defined(_WIN32) || defined(_WIN64)
    HMODULE result;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            std::bit_cast<LPCWSTR>(&UniversalRelocBase::Resolve), &result))
    {
        static constexpr auto msg =
            L"Unable to retrieve the handle for a plugin.\n"
            L"Normally, this issue should not happen.\n"
            L"\n"
            L"What you can do:\n"
            L"    1. Disable all mods.\n"
            L"    2. Enable them one by one.\n"
            L"    3. Start the game after each change to see if the issue happens again.\n"
            L"\n"
            L"For more detailed instructions on identifying the mod causing the issue, visit the following link:\n"
            L"    "
            L"https://wiki.redmodding.org/cyberpunk-2077-modding/for-mod-users/"
            L"user-guide-troubleshooting#finding-the-broken-mod-bisecting\n"
            L"\n"
            L"By following these instructions, you can identify the mod causing the issue and report it to the mod "
            L"author for further assistance.";

        MessageBoxW(nullptr, msg, L"RED4ext.SDK", MB_ICONERROR | MB_OK);
        TerminateProcess(GetCurrentProcess(), 1);
    }

    return result;
#else
    Dl_info info;
    if (dladdr(std::bit_cast<void*>(&UniversalRelocBase::Resolve), &info))
    {
        return dlopen(info.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
    }
    return nullptr;
#endif
}

RED4EXT_INLINE std::filesystem::path RED4ext::UniversalRelocBase::GetCurrentModulePath()
{
#if defined(_WIN32) || defined(_WIN64)
    static constexpr auto pathLength = MAX_PATH;
    const auto handle = GetCurrentModuleHandle();

    std::wstring fileName;
    DWORD length = 0;

    do
    {
        fileName.resize(fileName.size() + pathLength, L'\0');
        length = GetModuleFileNameW(handle, fileName.data(), static_cast<uint32_t>(fileName.size()));
    } while (GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    if (length > 0)
    {
        // Resize it to the real, std::filesystem::path" will use the string's length instead of recounting it.
        fileName.resize(length);
    }

    return fileName;
#else
    Dl_info info;
    if (dladdr(std::bit_cast<void*>(&UniversalRelocBase::Resolve), &info))
    {
        return std::filesystem::path(info.dli_fname);
    }
    return {};
#endif
}

RED4EXT_INLINE RED4ext::UniversalRelocBase::QueryFunc_t RED4ext::UniversalRelocBase::GetCurrentPluginQueryFunction()
{
    static constexpr auto procName = "Query";

    const auto handle = GetCurrentModuleHandle();

#if defined(_WIN32) || defined(_WIN64)
    const auto func = std::bit_cast<QueryFunc_t>(GetProcAddress(handle, procName));
#else
    const auto func = std::bit_cast<QueryFunc_t>(dlsym(handle, procName));
#endif

    if (func == nullptr)
    {
        static constexpr auto msg = L"Could not get the 'Query' function for the current mod.\n"
                                    L"Normally, this issue should not happen.\n"
                                    L"\n"
                                    L"If you are the mod's developer, verify that your mod was loaded by RED4ext and "
                                    L"that it exports the 'Query' function needed for the mod to interact with "
                                    L"RED4ext. Alternatively, you may need to provide your own address resolver.";

#if defined(_WIN32) || defined(_WIN64)
        ShowErrorAndTerminateProcess(msg, GetLastError(), false);
#else
        ShowErrorAndTerminateProcess(msg, static_cast<uint32_t>(errno), false);
#endif
    }

    return func;
}

RED4EXT_INLINE bool RED4ext::UniversalRelocBase::QueryCurrentPlugin(PluginInfo& aPluginInfo)
{
    const auto queryFunc = GetCurrentPluginQueryFunction();
    if (!queryFunc)
    {
        return false;
    }

    try
    {
        queryFunc(&aPluginInfo);
    }
    catch (...)
    {
        return false;
    }

    return true;
}

RED4EXT_INLINE void RED4ext::UniversalRelocBase::ShowErrorAndTerminateProcess(std::wstring_view aMsg,
                                                                              std::uint32_t aLastError,
                                                                              bool aQueryPluginInfo)
{
    const auto path = GetCurrentModulePath();

#if defined(_WIN32) || defined(_WIN64)
    std::wstring pluginName = path.stem().wstring();
#else
    // Convert narrow string to wide string on macOS
    auto stemStr = path.stem().string();
    std::wstring pluginName(stemStr.begin(), stemStr.end());
#endif
    std::wstring pluginVersion = L"Not available (Query was intentionally disabled)";

    if (aQueryPluginInfo)
    {
        PluginInfo pluginInfo{};

        auto isQuerySuccessful = QueryCurrentPlugin(pluginInfo);
        if (isQuerySuccessful)
        {
            if (pluginInfo.name)
            {
                pluginName = pluginInfo.name;
            }

            pluginVersion = std::to_wstring(pluginInfo.version);
        }
        else
        {
            pluginVersion = L"Not available (Query failed)";
        }
    }

    auto title = pluginName + L": Address Resolver";

    std::wostringstream msg;
    msg << aMsg << L"\n"
        << L"-----------------------------\n"
        << L"The mod has encountered a critical error while trying to resolve an address hash and needs to terminate "
           L"the game's process to prevent unexpected behavior in the game.\n"
        << L"-----------------------------\n"
        << L"Here is some debug information that may help resolve or report the issue:\n"
        << L"    - Error Code (Platform): " << std::dec << aLastError << "\n"
        << L"    - Plugin: " << pluginName << "\n"
        << L"    - Version: " << pluginVersion << "\n"
        << L"    - Path: " << path.c_str();

#if defined(_WIN32) || defined(_WIN64)
    MessageBoxW(nullptr, msg.str().c_str(), title.c_str(), MB_ICONERROR | MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
#else
    // Convert wide string to narrow string for std::cerr on macOS
    auto msgStr = msg.str();
    auto titleStr = title;
    
    // Simple UTF-16 to UTF-8 conversion
    std::string narrowTitle, narrowMsg;
    for (wchar_t c : titleStr)
    {
        if (c < 0x80)
            narrowTitle += static_cast<char>(c);
        else
            narrowTitle += '?'; // Replace non-ASCII with ?
    }
    for (wchar_t c : msgStr)
    {
        if (c < 0x80)
            narrowMsg += static_cast<char>(c);
        else if (c == L'\n')
            narrowMsg += '\n';
        else
            narrowMsg += '?';
    }
    
    std::cerr << "[" << narrowTitle << "] " << narrowMsg << std::endl;
    exit(1);
#endif
}
