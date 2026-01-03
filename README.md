# RED4ext.SDK for macOS

SDK for creating RED4ext plugins on **macOS Apple Silicon**.

> **This is a macOS port** of [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) by WopsS.  
> For Windows, use the [original repository](https://github.com/WopsS/RED4ext.SDK).

---

## Status

| Component | Status | Notes |
|-----------|--------|-------|
| Address resolution | ✅ Production | 126/126 addresses |
| RTTI types | ✅ Complete | All game types |
| TweakDB access | ✅ Working | Full read/write |
| Script system | ✅ Working | Function registration |
| Memory management | ✅ Working | Game allocator access |

**Platform:** macOS 12+ on Apple Silicon (M1/M2/M3/M4)  
**Game Version:** Cyberpunk 2077 v2.3.1 (macOS)

---

## Quick Start

### Installation (as submodule)

\`\`\`bash
git submodule add https://github.com/memaxo/RED4ext.SDK.git deps/red4ext.sdk
\`\`\`

### CMake Integration

\`\`\`cmake
add_subdirectory(deps/red4ext.sdk)
target_link_libraries(your_plugin PRIVATE RED4ext::SDK)
\`\`\`

---

## Basic Plugin Example

\`\`\`cpp
#include <RED4ext/RED4ext.hpp>

RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle aHandle,
                                         RED4ext::EMainReason aReason,
                                         const RED4ext::Sdk* aSdk)
{
    switch (aReason) {
    case RED4ext::EMainReason::Load:
        // Plugin loaded
        break;
    case RED4ext::EMainReason::Unload:
        // Plugin unloading
        break;
    }
    return true;
}

RED4EXT_C_EXPORT void RED4EXT_CALL Query(RED4ext::PluginInfo* aInfo)
{
    aInfo->name = L"My Plugin";
    aInfo->author = L"Author";
    aInfo->version = RED4EXT_SEMVER(1, 0, 0);
    aInfo->runtime = RED4EXT_RUNTIME_LATEST;
    aInfo->sdk = RED4EXT_SDK_LATEST;
}

RED4EXT_C_EXPORT uint32_t RED4EXT_CALL Supports()
{
    return RED4EXT_API_VERSION_LATEST;
}
\`\`\`

---

## Address Resolution

The SDK resolves 126 game function addresses from \`cyberpunk2077_addresses.json\`. This file must be present in the \`red4ext/\` directory.

### Manual Address Override (for custom plugins)

If your plugin needs addresses not in the SDK, create an override header:

\`\`\`cpp
// AddressResolverOverride.hpp - include BEFORE any SDK headers
#pragma once

#include <cstdint>
#include <unordered_map>

namespace RED4ext::Detail
{
    struct AddressResolverOverride {
        static std::uintptr_t Resolve(std::uint32_t aHash) {
            static const std::unordered_map<std::uint32_t, std::uintptr_t> table = {
                { 0xYOURHASH, 0xOFFSET },  // Your custom address
            };
            auto it = table.find(aHash);
            return (it != table.end()) ? it->second : 0;
        }
    };
}
\`\`\`

---

## macOS Compatibility

This fork includes platform adaptations for Apple Silicon:

| Windows API | macOS Equivalent |
|-------------|------------------|
| \`__readgsqword\` (TLS) | \`pthread_key_t\` |
| \`CRITICAL_SECTION\` | \`pthread_mutex_t\` |
| \`Interlocked*\` | \`__atomic_*\` builtins |
| \`LoadLibrary\` / \`GetProcAddress\` | \`dlopen\` / \`dlsym\` |
| \`VirtualAlloc\` / \`VirtualProtect\` | \`mmap\` / \`mprotect\` |

See [MACOS_CHANGES.md](MACOS_CHANGES.md) for implementation details.

---

## Examples

The \`examples/\` directory contains working plugin examples:

| Example | Description |
|---------|-------------|
| \`accessing_properties\` | Read/write class properties |
| \`execute_functions\` | Call game functions |
| \`function_registration\` | Register new script functions |
| \`native_class_redscript\` | Expose C++ class to scripts |
| \`native_globals_redscript\` | Expose global functions |

### Build Examples

\`\`\`bash
cd examples
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j\$(sysctl -n hw.ncpu)
\`\`\`

---

## Building the SDK

The SDK is mostly header-only. Build only if you need the reflection helpers:

\`\`\`bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j\$(sysctl -n hw.ncpu)
\`\`\`

---

## Project Structure

\`\`\`
RED4ext.SDK/
├── include/RED4ext/        # Main SDK headers
│   ├── Detail/             # Implementation details
│   ├── Scripting/          # Script system types
│   ├── TweakDB.hpp         # TweakDB interface
│   ├── Relocation.hpp      # Address resolution
│   └── Common.hpp          # Platform types
├── src/                    # Compiled components
├── examples/               # Plugin examples
├── scripts/                # Utility scripts
└── cyberpunk2077_addresses.json  # Address database
\`\`\`

---

## Documentation

| Document | Description |
|----------|-------------|
| [MACOS_CHANGES.md](MACOS_CHANGES.md) | Platform adaptation details |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution guidelines |
| [AGENTS.md](AGENTS.md) | Development guidelines |

---

## Related Projects

| Project | Description |
|---------|-------------|
| [RED4ext](https://github.com/memaxo/RED4ext) | macOS loader |
| [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) | Original Windows SDK |
| [TweakXL-macos](https://github.com/memaxo/cp2077-tweak-xl) | Example plugin |

---

## License

MIT License — see [LICENSE.md](LICENSE.md)

## Credits

- **WopsS** — Original RED4ext.SDK author
- **Cyberpunk 2077 modding community**
