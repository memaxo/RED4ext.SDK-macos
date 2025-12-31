# RED4ext.SDK

A library to create mods for REDengine 4 ([Cyberpunk 2077](https://www.cyberpunk.net)).

## Platforms

| Platform | Architecture | Status |
|----------|--------------|--------|
| Windows | x86-64 | ✅ Supported |
| macOS | ARM64 (Apple Silicon) | ✅ Supported |

## What is this library?

RED4ext.SDK provides the reversed game types and helper functions needed to extend REDengine 4. It allows modders to:
- Add new scripting functions
- Modify game behavior
- Access game internals
- Create RED4ext plugins

## Usage

### Header-Only Version

Include the [header files](/include) in your project and use a C++20 compiler.

```cpp
#include <RED4ext/RED4ext.hpp>

// Your plugin code - works on both Windows and macOS
```

### Static Library Version

Add the [header files](/include) and [source files](/src) to your project, define `RED4EXT_STATIC_LIB`, and use a C++20 compiler.

### CMake Integration

```cmake
add_subdirectory(deps/RED4ext.SDK)
target_link_libraries(your_plugin PRIVATE RED4ext.SDK)
```

## Building

### Windows

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### macOS

```bash
# Install dependencies
xcode-select --install
brew install cmake

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(sysctl -n hw.ncpu)
```

## Platform Differences

| Feature | Windows | macOS |
|---------|---------|-------|
| Binary format | PE | Mach-O |
| Architecture | x86-64 | ARM64 |
| Plugin extension | `.dll` | `.dylib` |
| TLS access | `__readgsqword` | pthread |
| Atomics | `Interlocked*` | `__atomic_*` |

### Cross-Platform Code

The SDK handles platform differences transparently. Plugin code should work on both platforms:

```cpp
RED4EXT_C_EXPORT bool RED4EXT_CALL Main(RED4ext::PluginHandle aHandle,
                                         RED4ext::EMainReason aReason,
                                         const RED4ext::Sdk* aSdk)
{
    // Same code works on both platforms
    return true;
}
```

For platform-specific code, use:

```cpp
#if defined(_WIN32) || defined(_WIN64)
    // Windows-specific code
#else
    // macOS-specific code
#endif
```

## Examples

Example plugins are in the [examples/](/examples) directory:
- `accessing_properties` - Reading/writing game properties
- `execute_functions` - Calling game functions
- `function_registration` - Adding new script functions
- `native_class_redscript` - Creating native classes
- `native_globals_redscript` - Adding global functions

## Documentation

- [RED4ext Documentation](https://docs.red4ext.com/)
- [Cyberpunk 2077 Modding Wiki](https://wiki.redmodding.org/)
- [MACOS_CHANGES.md](MACOS_CHANGES.md) - macOS-specific changes

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](/CONTRIBUTING.md).

## License

This project is licensed under the MIT License - see [LICENSE.md](/LICENSE.md).

## Acknowledgments

- **WopsS** - Original RED4ext.SDK author
- **Cyberpunk 2077 modding community**
