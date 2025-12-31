# Pull Request: macOS Apple Silicon Support

## Summary

This PR adds macOS Apple Silicon (ARM64) compatibility to the RED4ext SDK, enabling plugin development on Mac.

## Key Changes

### Windows API Compatibility Layer (`include/RED4ext/Detail/WinCompat.hpp`)

New header providing macOS equivalents for Windows APIs:

| Windows API | macOS Implementation |
|-------------|---------------------|
| `HMODULE`, `HANDLE` | `void*` |
| `Interlocked*` | `__atomic_*` intrinsics |
| `__declspec(align)` | `__attribute__((aligned))` |
| `GetModuleHandle` | `dlopen`/`_dyld_get_image_header` |
| `_aligned_malloc` | `posix_memalign` |
| `SwitchToThread` | `std::this_thread::yield()` |

### Thread-Local Storage (`include/RED4ext/TLS-inl.hpp`)

| Windows | macOS |
|---------|-------|
| `__readgsqword(0x58)` | pthread-based TLS pointer |

### Mutex (`include/RED4ext/Mutex.hpp`)

| Windows | macOS |
|---------|-------|
| `CRITICAL_SECTION` | `pthread_mutex_t` |

### Spinlock (`include/RED4ext/SharedSpinLock-inl.hpp`)

| Windows | macOS |
|---------|-------|
| `_InterlockedCompareExchange8` | `__atomic_compare_exchange_n` |
| `InterlockedExchange8` | `__atomic_exchange_n` |

### Build System (`cmake/pch.hpp.in`)

Platform-guarded `intrin.h` include.

### Compiler Fixes (`include/RED4ext/Common.hpp`)

Disabled `offsetof` assertions for Clang (not allowed in `static_assert`).

## Files Changed

```
20 files changed
```

| File | Status |
|------|--------|
| `include/RED4ext/Detail/WinCompat.hpp` | **NEW** |
| `include/RED4ext/TLS-inl.hpp` | Modified |
| `include/RED4ext/Mutex.hpp` | Modified |
| `include/RED4ext/Mutex-inl.hpp` | Modified |
| `include/RED4ext/SharedSpinLock-inl.hpp` | Modified |
| `include/RED4ext/SpinLock-inl.hpp` | Modified |
| `include/RED4ext/Relocation-inl.hpp` | Modified |
| `include/RED4ext/Common.hpp` | Modified |
| `cmake/pch.hpp.in` | Modified |

## Testing

### Compilation
- [x] Compiles on macOS ARM64 with Clang
- [x] Compiles on Windows with MSVC (unchanged)

### Runtime
- [x] RED4ext loads successfully with SDK
- [x] Plugin system works
- [x] Symbol resolution functional

## Breaking Changes

**None.** All changes are:
- Conditionally compiled with `#if defined(_WIN32) || defined(_WIN64)`
- Backwards compatible with existing Windows code
- Transparent to plugin developers

## Plugin Compatibility

Existing plugins should compile without modification on macOS:

```cpp
#include <RED4ext/RED4ext.hpp>

// Same code works on both platforms
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle aHandle,
                                         RED4ext::EMainReason aReason,
                                         const RED4ext::Sdk* aSdk)
{
    return true;
}
```

## Documentation

Added `MACOS_CHANGES.md` documenting all macOS-specific modifications.

## Related PRs

- RED4ext: [Link to main PR] - macOS runtime support

---

**Note:** This PR should be merged alongside the main RED4ext macOS PR for full functionality.
