# macOS Address Database Validation

This document describes how to validate the macOS address database (`cyberpunk2077_addresses.json`) for the RED4ext.SDK.

## Overview

On macOS, the SDK resolves function addresses from a JSON database instead of using the Windows Address Library. Each entry maps a 32-bit FNV1a hash to an offset within the game's `__TEXT` segment.

## Database Format

```json
{
  "version": "1.0",
  "game_version": "2.3.1",
  "stats": {
    "total": 126,
    "resolved": 126,
    "unresolved": 0
  },
  "Addresses": [
    {
      "hash": "1247874916",
      "offset": "1:0x3452734"
    }
  ]
}
```

- **hash**: Decimal string representation of the 32-bit FNV1a hash (defined in `include/RED4ext/Detail/AddressHashes.hpp`)
- **offset**: Format is `segment:0xOFFSET` where segment is always `1` (`__TEXT` segment)

## Quick Validation Checklist

Run these commands from the repository root to validate a new address database:

### 1. Validate SDK Database (Basic Checks)

```bash
python3 scripts/check_addresses.py --strict
```

**Expected output:**
```
total=126
duplicates=0
zero_offsets=0
```

Exit code: `0` on success, `1` on failure (if `--strict` is used)

### 2. Validate Loader Database (Extended Checks)

```bash
python3 scripts/check_loader_addresses.py --strict
```

**Expected output:**
```
db=/path/to/RED4ext.SDK/cyberpunk2077_addresses.loader.json
total=134
duplicates=0
zero_offsets=0
missing_required_hooks=0
```

Exit code: `0` on success, `1` on failure (if `--strict` is used)

### 3. Generate/Update Loader Database

If you have new hook targets in `scripts/loader_hook_targets.json`:

```bash
python3 scripts/generate_loader_addresses.py
```

This merges the SDK database with loader-specific hook targets.

### 4. Verify All Hashes are Defined

Count the hash constants in the SDK:

```bash
grep -c 'constexpr std::uint32_t' include/RED4ext/Detail/AddressHashes.hpp
```

Should match `total` in the database stats.

### 5. Check for Zero Offsets (Unimplemented Addresses)

```bash
python3 scripts/check_addresses.py | grep zero_offsets
```

Any non-zero count indicates addresses that need reverse engineering.

## Integration Testing

### Build the Smoke Test

```bash
# Configure with examples enabled
mkdir -p build-macos && cd build-macos
cmake .. -DRED4EXT_BUILD_EXAMPLES=ON

# Build the smoke test
cmake --build . --target macos_smoke_test
```

### Expected Behavior

When the smoke test plugin loads, it will:
1. Search for `cyberpunk2077_addresses.json` using standard search paths
2. Load all hashes and attempt resolution
3. Log missing (zero) addresses and duplicates to `/tmp/RED4ext.SDK_smoke_test.log`

**Successful run:**
```
[RED4ext.SDK smoke] Loading...
[RED4ext.SDK smoke] Loaded 126 hashes from /path/to/cyberpunk2077_addresses.json
[RED4ext.SDK smoke] ImageBase=0x100000000
[RED4ext.SDK smoke] Done. missing=0 dup=0
```

## Address Database Search Paths

The SDK searches for `cyberpunk2077_addresses.json` in this order:

1. `$RED4EXT_SDK_ADDRESS_DB` environment variable (if set)
2. Same directory as the plugin (`.dylib`)
3. `red4ext/` or `red4ext/bin/x64/` relative to the plugin
4. Same directory as the game executable
5. `red4ext/` or `red4ext/bin/x64/` relative to the executable

## Adding New Addresses

1. Add the hash constant to `include/RED4ext/Detail/AddressHashes.hpp`:
   ```cpp
   constexpr std::uint32_t MyNewFunction = 0xDEADBEEF;
   ```

2. Add the entry to `cyberpunk2077_addresses.json`:
   ```json
   {
     "hash": "3735928559",
     "offset": "1:0x12345678"
   }
   ```

3. Run validation:
   ```bash
   python3 scripts/check_addresses.py --strict
   ```

4. Update loader database if needed:
   ```bash
   python3 scripts/generate_loader_addresses.py
   ```

## Troubleshooting

### "Missing address hash" warnings

- Check that the hash exists in `AddressHashes.hpp`
- Verify the hash value matches (decimal vs hex)
- Ensure the database file is in one of the search paths

### Duplicate offsets

- Two different hashes pointing to the same offset may indicate:
  - A copy-paste error in the database
  - Intentional (e.g., multiple functions pointing to the same stub)
  - The smoke test recognizes known stubs like `CBaseRTTIType_sub_98` and `CBaseRTTIType_sub_A0`

### Zero offsets

- `offset: "1:0x0"` is a placeholder for unimplemented addresses
- The SDK will log these and fall back to RED4ext runtime resolution
- Use `--strict` flag in validation scripts to catch these

## File Locations

| File | Purpose |
|------|---------|
| `cyberpunk2077_addresses.json` | Main SDK address database (126 entries) |
| `cyberpunk2077_addresses.loader.json` | Extended database including loader hooks (134 entries) |
| `scripts/loader_hook_targets.json` | Loader-specific hook targets |
| `include/RED4ext/Detail/AddressHashes.hpp` | Hash constant definitions |
| `include/RED4ext/Relocation-inl.hpp` | Address resolution implementation |

## Continuous Integration

Add this to your CI pipeline:

```bash
#!/bin/bash
set -e

# Validate SDK database
python3 scripts/check_addresses.py --strict

# Validate loader database
python3 scripts/check_loader_addresses.py --strict

# Build smoke test (macOS only)
if [[ "$OSTYPE" == "darwin"* ]]; then
    mkdir -p build-macos
    cd build-macos
    cmake .. -DRED4EXT_BUILD_EXAMPLES=ON
    cmake --build . --target macos_smoke_test
fi
```
