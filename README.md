# RED4ext.SDK for macOS

SDK for creating RED4ext plugins on **macOS Apple Silicon**.

> **This is a macOS port** of [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) by WopsS.  
> For Windows, use the [original repository](https://github.com/WopsS/RED4ext.SDK).

---

## ⚠️ Beta Status

This SDK enables plugin development on macOS but **does not guarantee full parity with Windows**.

| Component | Status | Notes |
|-----------|--------|-------|
| Address resolution | ⚠️ Functional | 126 addresses found, not all runtime-verified |
| RTTI types | ⚠️ Ported | May have alignment/vtable differences |
| TweakDB access | ✅ Tested | Basic operations verified |
| Script system | ⚠️ Ported | Not comprehensively tested |
| Memory management | ⚠️ Ported | Uses platform equivalents |

### Known Differences from Windows

| Aspect | Windows | macOS |
|--------|---------|-------|
| Binary format | PE/x64 | Mach-O/ARM64 |
| ABI | Microsoft x64 | ARM64 AAPCS |
| TLS access | `__readgsqword` | pthread TLS |
| Atomics | `Interlocked*` | `__atomic_*` |
| Hooking | Detours | Frida |

**Some SDK functions may behave differently or fail silently.**

---

## Platform Requirements

- **macOS 12+** (Monterey or later)
- **Apple Silicon** (M1/M2/M3/M4)
- **Cyberpunk 2077 v2.3.1** (macOS Steam version)

---

## Quick Start

### As Git Submodule

```bash
git submodule add https://github.com/memaxo/RED4ext.SDK.git deps/red4ext.sdk
```

### CMake Integration

```cmake
add_subdirectory(deps/red4ext.sdk)
target_link_libraries(your_plugin PRIVATE RED4ext::SDK)
```

---

## Basic Plugin

```cpp
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
```

---

## Address Resolution

The SDK resolves 126 game addresses from `cyberpunk2077_addresses.json`.

⚠️ **Important:** Addresses were discovered via pattern matching. Not all have been verified at runtime. If your plugin crashes, the address may be incorrect.

### Custom Address Override

For addresses not in the SDK or to fix incorrect ones:

```cpp
// AddressResolverOverride.hpp - include BEFORE any SDK headers
#pragma once
#include <cstdint>
#include <unordered_map>

namespace RED4ext::Detail
{
    struct AddressResolverOverride {
        static std::uintptr_t Resolve(std::uint32_t aHash) {
            static const std::unordered_map<std::uint32_t, std::uintptr_t> table = {
                { 0xYOURHASH, 0xOFFSET },
            };
            auto it = table.find(aHash);
            return (it != table.end()) ? it->second : 0;
        }
    };
}
```

---

## macOS Platform Layer

This fork provides macOS equivalents for Windows APIs:

| Windows API | macOS Implementation |
|-------------|---------------------|
| `__readgsqword` (TLS) | `pthread_key_t` + runtime init |
| `CRITICAL_SECTION` | `pthread_mutex_t` |
| `Interlocked*` | `__atomic_*` builtins |
| `LoadLibrary` | `dlopen` |
| `GetProcAddress` | `dlsym` |
| `VirtualAlloc` | `mmap` |
| `VirtualProtect` | `mprotect` |

See [MACOS_CHANGES.md](MACOS_CHANGES.md) for details.

---

## Examples

The `examples/` directory contains plugin examples:

| Example | Description | macOS Status |
|---------|-------------|--------------|
| `accessing_properties` | Read/write class properties | ❓ Untested |
| `execute_functions` | Call game functions | ❓ Untested |
| `function_registration` | Register script functions | ❓ Untested |
| `native_class_redscript` | Expose C++ class | ❓ Untested |
| `native_globals_redscript` | Expose globals | ❓ Untested |

```bash
cd examples && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

---

## Building

The SDK is mostly header-only:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

---

## Porting Windows Plugins

To port a Windows plugin to macOS:

1. Replace Windows headers with macOS equivalents
2. Update any inline assembly (x64 → ARM64)
3. Fix path separators (`\` → `/`)
4. Recompile as .dylib
5. Test thoroughly — behavior may differ

---

## Contributing

Help improve macOS support:

- Verify addresses at runtime
- Test SDK examples
- Report bugs with stack traces
- Port popular plugins

---

## Related Projects

| Project | Description |
|---------|-------------|
| [RED4ext](https://github.com/memaxo/RED4ext) | macOS loader |
| [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) | Original Windows SDK |
| [TweakXL-macos](https://github.com/memaxo/cp2077-tweak-xl) | Tested example plugin |

---

## License

MIT License — see [LICENSE.md](LICENSE.md)

## Credits

- **WopsS** — Original RED4ext.SDK author
- **Cyberpunk 2077 modding community**
