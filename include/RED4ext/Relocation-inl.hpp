#pragma once

#ifdef RED4EXT_STATIC_LIB
#include <RED4ext/Relocation.hpp>
#endif

#include <mutex>
#include <sstream>
#include <iostream>

#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#else
#include <dlfcn.h>
#include <mach-o/dyld.h>
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
    else
    {
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
    }
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
