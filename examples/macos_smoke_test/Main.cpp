#include <RED4ext/RED4ext.hpp>
#include <RED4ext/Relocation.hpp>
#include <RED4ext/TLS.hpp>

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <vector>

#if defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <limits.h>
#endif

namespace
{
std::ostream& LogStream()
{
#if defined(__APPLE__)
    static std::ofstream s_log("/tmp/RED4ext.SDK_smoke_test.log", std::ios::out | std::ios::app);
    if (s_log.is_open())
    {
        static bool s_configured = false;
        if (!s_configured)
        {
            s_log << std::unitbuf;
            s_configured = true;
        }
        return s_log;
    }
#endif
    return std::cerr;
}

bool TryParseU32(std::string_view aStr, std::uint32_t& aOut)
{
    aOut = 0;
    const char* begin = aStr.data();
    const char* end = aStr.data() + aStr.size();
    auto res = std::from_chars(begin, end, aOut, 10);
    return res.ec == std::errc{} && res.ptr == end;
}

std::pair<std::string_view, size_t> ExtractQuotedValueAfterKey(std::string_view aText, std::string_view aKey,
                                                               size_t aFrom)
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
}

std::filesystem::path GetThisModulePath()
{
#if defined(__APPLE__)
    Dl_info info{};
    if (dladdr(reinterpret_cast<const void*>(&GetThisModulePath), &info) && info.dli_fname)
    {
        return std::filesystem::path(info.dli_fname);
    }
#endif
    return {};
}

std::filesystem::path GetExecutableDir()
{
#if defined(__APPLE__)
    char exePathBuf[PATH_MAX] = {};
    std::uint32_t exeSize = sizeof(exePathBuf);
    if (_NSGetExecutablePath(exePathBuf, &exeSize) == 0)
    {
        return std::filesystem::path(exePathBuf).parent_path();
    }
#endif
    return {};
}

std::filesystem::path FindAddressDbPath()
{
    auto tryPaths = [](const std::filesystem::path& aDir) -> std::filesystem::path
    {
        if (aDir.empty())
            return {};

        {
            auto p = aDir / "cyberpunk2077_addresses.json";
            if (std::filesystem::exists(p))
                return p;
        }

        {
            auto p = aDir / "bin" / "x64" / "cyberpunk2077_addresses.json";
            if (std::filesystem::exists(p))
                return p;
        }

        return {};
    };

    auto findRed4extRootNear = [&](std::filesystem::path p) -> std::filesystem::path
    {
        // Search upwards for either the red4ext dir itself, or a parent that contains it.
        for (int i = 0; i < 10 && !p.empty(); ++i)
        {
            if (p.filename() == "red4ext")
                return p;

            auto candidate = p / "red4ext";
            if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate))
                return candidate;

            p = p.parent_path();
        }
        return {};
    };

    if (const char* envPath = std::getenv("RED4EXT_SDK_ADDRESS_DB"); envPath && *envPath)
    {
        std::filesystem::path p(envPath);
        if (std::filesystem::exists(p))
            return p;
    }

    const auto modulePath = GetThisModulePath();
    if (!modulePath.empty())
    {
        // 1) Same directory as the module.
        if (auto p = tryPaths(modulePath.parent_path()); !p.empty())
            return p;

        // 2) Under red4ext root (common when plugin lives in red4ext/plugins/*).
        if (auto red4extRoot = findRed4extRootNear(modulePath.parent_path()); !red4extRoot.empty())
        {
            if (auto p = tryPaths(red4extRoot); !p.empty())
                return p;
        }
    }
    const auto exeDir = GetExecutableDir();
    if (!exeDir.empty())
    {
        if (auto p = tryPaths(exeDir); !p.empty())
            return p;

        if (auto red4extRoot = findRed4extRootNear(exeDir); !red4extRoot.empty())
        {
            if (auto p = tryPaths(red4extRoot); !p.empty())
                return p;
        }
    }

    return {};
}

std::vector<std::uint32_t> LoadHashesFromJson(const std::filesystem::path& aPath)
{
    std::ifstream file(aPath);
    if (!file)
        return {};

    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (contents.empty())
        return {};

    std::vector<std::uint32_t> hashes;

    std::string_view text(contents);
    size_t pos = 0;
    while (true)
    {
        auto [hashStr, hashNext] = ExtractQuotedValueAfterKey(text, "\"hash\"", pos);
        if (hashNext == std::string_view::npos)
            break;

        std::uint32_t hash = 0;
        if (TryParseU32(hashStr, hash))
            hashes.push_back(hash);

        pos = hashNext;
    }

    return hashes;
}
} // namespace

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle aHandle, RED4ext::EMainReason aReason,
                                        const RED4ext::Sdk* aSdk)
{
    RED4EXT_UNUSED_PARAMETER(aHandle);
    RED4EXT_UNUSED_PARAMETER(aSdk);

    if (aReason != RED4ext::EMainReason::Load)
        return true;

    auto& log = LogStream();
    log << "[RED4ext.SDK smoke] Loading..." << "\n";

#if defined(__APPLE__)
    using GetTlsFunc_t = RED4ext::TLS* (*)();
    GetTlsFunc_t exportedGetTls = nullptr;
    if (const auto handle = dlopen("RED4ext.dylib", RTLD_LAZY | RTLD_NOLOAD))
    {
        exportedGetTls = reinterpret_cast<GetTlsFunc_t>(dlsym(handle, "RED4ext_GetTLS"));
    }
    log << "[RED4ext.SDK smoke] RED4ext_GetTLS sym=" << reinterpret_cast<const void*>(exportedGetTls) << "\n";
#endif

    auto* tls = RED4ext::TLS::Get();
    if (!tls)
    {
        for (int i = 0; i < 50 && !tls; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            tls = RED4ext::TLS::Get();
        }
    }

    log << "[RED4ext.SDK smoke] TLS::IsInitialized=" << (tls ? "true" : "false") << "\n";
    log << "[RED4ext.SDK smoke] TLS::Get=" << tls << "\n";

#if defined(__APPLE__)
    if (exportedGetTls)
    {
        auto* exportedTls = exportedGetTls();
        log << "[RED4ext.SDK smoke] RED4ext_GetTLS()=" << exportedTls << "\n";
    }
#endif

    const auto dbPath = FindAddressDbPath();
    if (dbPath.empty())
    {
        log << "[RED4ext.SDK smoke] ERROR: Could not find cyberpunk2077_addresses.json" << "\n";
        return true;
    }

    const auto hashes = LoadHashesFromJson(dbPath);
    log << "[RED4ext.SDK smoke] Loaded " << hashes.size() << " hashes from " << dbPath.string() << "\n";

    const auto base = RED4ext::RelocBase::GetImageBase();
    log << "[RED4ext.SDK smoke] ImageBase=0x" << std::hex << base << std::dec << "\n";

#if defined(__APPLE__)
    const auto expectedStub = base + 0x3228;
    constexpr std::uint32_t kSub98 = RED4ext::Detail::AddressHashes::CBaseRTTIType_sub_98;
    constexpr std::uint32_t kSubA0 = RED4ext::Detail::AddressHashes::CBaseRTTIType_sub_A0;
#endif

    std::size_t zeroCount = 0;
    std::size_t dupCount = 0;
    std::unordered_map<std::uintptr_t, std::uint32_t> byAddr;
    byAddr.reserve(hashes.size());

    for (auto hash : hashes)
    {
        const auto addr = RED4ext::UniversalRelocBase::Resolve(hash);
        if (addr == 0)
        {
            ++zeroCount;
            log << "[RED4ext.SDK smoke] MISSING hash=0x" << std::hex << hash << std::dec << "\n";
            continue;
        }

        const auto [it, inserted] = byAddr.emplace(addr, hash);
        if (!inserted)
        {
#if defined(__APPLE__)
            const bool isKnownStubPair = (addr == expectedStub) &&
                                         ((hash == kSub98 && it->second == kSubA0) ||
                                          (hash == kSubA0 && it->second == kSub98));
            if (!isKnownStubPair)
#endif
            {
                ++dupCount;
                log << "[RED4ext.SDK smoke] DUP addr=0x" << std::hex << addr << " hash=0x" << hash
                    << " first=0x" << it->second << std::dec << "\n";
            }
        }
    }

#if defined(__APPLE__)
    const auto sub98 = RED4ext::UniversalRelocBase::Resolve(kSub98);
    const auto subA0 = RED4ext::UniversalRelocBase::Resolve(kSubA0);
    log << "[RED4ext.SDK smoke] CBaseRTTIType_sub_98=0x" << std::hex << sub98
        << " expected=0x" << expectedStub << std::dec << "\n";
    log << "[RED4ext.SDK smoke] CBaseRTTIType_sub_A0=0x" << std::hex << subA0
        << " expected=0x" << expectedStub << std::dec << "\n";
#endif

    log << "[RED4ext.SDK smoke] Done. missing=" << zeroCount << " dup=" << dupCount << "\n";
    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::PluginInfo* aInfo)
{
    aInfo->name = L"RED4ext.SDK.macos_smoke_test";
    aInfo->author = L"Factory";
    aInfo->version = RED4EXT_SEMVER(1, 0, 0);
    aInfo->runtime = RED4EXT_FILEVER(2, 3, 1, 0);
    aInfo->sdk = RED4EXT_SDK_LATEST;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_LATEST;
}
