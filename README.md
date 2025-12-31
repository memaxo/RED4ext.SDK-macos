# RED4ext.SDK for macOS

SDK for creating RED4ext plugins on **macOS Apple Silicon**.

> **This is a macOS port** of [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) by WopsS.  
> For Windows, use the [original repository](https://github.com/WopsS/RED4ext.SDK).

---

## About

This SDK provides the reversed game types and helper functions needed to create RED4ext plugins for Cyberpunk 2077 on macOS. It includes:

- Reversed engine types
- Helper functions for scripting
- Memory management utilities
- RTTI system access

---

## Usage

This SDK is included as a **git submodule** in [RED4ext-macos](https://github.com/memaxo/RED4ext-macos). You typically don't need to clone it separately.

### For Plugin Development

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

### CMake Integration

```cmake
# The SDK is available via RED4ext-macos submodule
target_link_libraries(your_plugin PRIVATE RED4ext::SDK)
```

---

## macOS Changes

This fork includes compatibility changes for Apple Silicon:

| Component | Change |
|-----------|--------|
| TLS | `pthread_key_t` instead of `__readgsqword` |
| Mutex | `pthread_mutex_t` instead of `CRITICAL_SECTION` |
| Atomics | `__atomic_*` instead of `Interlocked*` |
| Modules | `dlopen/dlsym` instead of `LoadLibrary` |

See [MACOS_CHANGES.md](MACOS_CHANGES.md) for full details.

---

## Building

You don't usually need to build the SDK separately. It's header-only and included via RED4ext-macos.

If you need to build standalone:

```bash
git clone https://github.com/memaxo/RED4ext.SDK-macos.git
cd RED4ext.SDK-macos
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

---

## Related Projects

- [RED4ext-macos](https://github.com/memaxo/RED4ext-macos) — macOS loader (uses this SDK)
- [RED4ext.SDK](https://github.com/WopsS/RED4ext.SDK) — Original Windows SDK
- [RED4ext](https://github.com/WopsS/RED4ext) — Original Windows loader

---

## License

MIT License — see [LICENSE.md](LICENSE.md)

## Acknowledgments

- **WopsS** — Original RED4ext.SDK author
