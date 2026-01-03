# RED4ext.SDK macOS Port - Agent Guidelines

## Project Context

RED4ext.SDK provides C++ headers and utilities for creating Cyberpunk 2077 mods. This port adds macOS ARM64 support while maintaining Windows compatibility. The SDK is header-only with minimal compiled components.

## Development Practices

### Cross-Platform Design

1. **Header-only preferred.** New functionality should be header-only when possible to avoid platform-specific compilation.
2. **Windows + macOS.** All public APIs must work on both platforms; use `#ifdef __APPLE__` / `#ifdef _WIN32` for divergence.
3. **No breaking changes.** Existing Windows mods must compile without modification against the macOS-compatible SDK.
4. **Type parity.** Windows types (`DWORD`, `HANDLE`) have macOS equivalents in `RED4ext/Common.hpp`.

### Address Resolution

1. **Universal relocation.** Use `UniversalRelocBase` and `UniversalRelocPtr` for all address lookups.
2. **Hash-based lookup.** Addresses are identified by FNV1a hashes defined in `Detail/AddressHashes.hpp`.
3. **Runtime resolution.** On macOS, `Resolve()` reads from `cyberpunk2077_addresses.json`; on Windows, from Address Library.
4. **Fallback chain.** Resolution order: override → JSON file → Address Library → 0 (failure).

### RTTI and Type System

1. **CClass hierarchy.** Game types inherit from `ISerializable` → `IScriptable`. Respect the vtable layout.
2. **CName hashing.** Use `CName` for all string identifiers; internally uses FNV1a64 hashing.
3. **TweakDB access.** Use `TweakDB::Get()` singleton; never cache the pointer across frames.
4. **Property reflection.** Access class properties via `CClass::GetProperty()`; validate existence before use.

## Architecture

### Directory Structure

```
include/RED4ext/
├── Detail/           # Internal implementation details
│   ├── AddressHashes.hpp  # All 126 address hash constants
│   └── ...
├── Scripting/        # Script system types (CStack, CBaseFunction)
├── GameEngine.hpp    # Engine singletons
├── TweakDB.hpp       # TweakDB interface
├── Relocation.hpp    # Address resolution system
└── Common.hpp        # Platform compatibility types
src/                  # Minimal compiled sources (reflection helpers)
```

### Key Files

1. **`Relocation-inl.hpp`** - Contains `UniversalRelocBase::Resolve()` implementation with macOS JSON loading.
2. **`AddressHashes.hpp`** - All 126 hash constants that plugins use for address resolution.
3. **`Common.hpp`** - Platform typedefs (`uint32_t` ↔ `DWORD`, pointer types, calling conventions).

## Code Standards

### Naming

1. **Types.** PascalCase with `C` prefix for game types: `CClass`, `CName`, `CString`.
2. **Enums.** PascalCase: `EGameDataStatType`, `ERTTITypeFlags`.
3. **Hash constants.** PascalCase matching function name: `CRTTISystem_Get`, `TweakDB_CreateRecord`.
4. **Macros.** SCREAMING_SNAKE with `RED4EXT_` prefix: `RED4EXT_ASSERT`, `RED4EXT_UNUSED`.

### Type Safety

1. **No raw casts.** Use `reinterpret_cast` only for address-to-pointer; prefer `static_cast` otherwise.
2. **Explicit sizes.** Use `uint32_t`, `int64_t`, never `int` or `long` for game structures.
3. **Packed structs.** Game structures must match exact memory layout; use `#pragma pack` when needed.
4. **Vtable alignment.** Virtual methods must be in exact order; verify against Windows SDK.

### Documentation

1. **Doxygen comments.** Public APIs need `///` documentation with `@param`, `@return`, `@note`.
2. **Offset comments.** Struct members should note their offset: `uint32_t m_flags; // 0x10`.
3. **Hash comments.** Address hashes should note the function: `constexpr uint32_t TweakDB_Get = 0x36800DE4; // Singleton getter`.

## Address Management

### Adding New Addresses

1. **Add hash constant.** Define in `Detail/AddressHashes.hpp` with descriptive name.
2. **Document in JSON.** Ensure `cyberpunk2077_addresses.json` includes the hash→offset mapping.
3. **Update count.** The SDK expects 126 addresses; update tests if count changes.

### Address Hash Format

```cpp
// Pattern: ClassName_MethodName = 0xHASHVALUE;
constexpr std::uint32_t CRTTISystem_Get = 0x4A610F64;
constexpr std::uint32_t TweakDB_CreateRecord = 0x3201127A;
```

### JSON Format

```json
{
  "Addresses": [
    { "hash": "1247543140", "offset": "1:0x3452734" }
  ]
}
```
- `hash`: Decimal string of the 32-bit FNV1a hash
- `offset`: `segment:hexoffset` where segment 1 = `__TEXT`

## Testing

1. **Compile test.** SDK headers must compile with `-std=c++20` on Clang 15+.
2. **Example builds.** All projects in `examples/` must build successfully.
3. **Type sizes.** Verify `sizeof(CClass)`, `sizeof(CName)` match expected values.
4. **Address resolution.** Test that all 126 hashes resolve to non-zero addresses.

## Common Pitfalls

1. **Missing vtable entries.** macOS vtables may have different padding; verify offsets.
2. **Calling convention.** ARM64 uses different ABI than x64; function pointer types must account for this.
3. **Endianness.** Both platforms are little-endian, but verify binary parsing.
4. **Alignment.** ARM64 has stricter alignment requirements; ensure structs are properly aligned.

## Compatibility Notes

### Windows Mods on macOS

1. **Recompile required.** Windows DLLs cannot run on macOS; source recompilation needed.
2. **Address override.** Mods must include `AddressResolverOverride.hpp` before SDK headers.
3. **No inline assembly.** x64 assembly won't work; use C++ intrinsics or ARM64 assembly.
4. **Hook library.** Replace MinHook/Detours calls with RED4ext hooking API.
