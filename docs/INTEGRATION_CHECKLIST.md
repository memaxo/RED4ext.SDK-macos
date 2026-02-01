# RED4ext macOS Integration Checklist

**Purpose:** Step-by-step guide for integrating the macOS SDK port into RED4ext core and validating with real mods.

**Prerequisites:**
- macOS development machine with Xcode/Clang
- Cyberpunk 2077 macOS installed via Steam
- CMake 3.21+, Python 3.8+
- GitHub CLI (`gh`) for PR operations

---

## Phase 1: SDK PR Review & Merge

### 1.1 Review the SDK PR

**PR URL:** `https://github.com/memaxo/RED4ext.SDK-macos/pull/1`

**Review Checklist:**
- [ ] **Commit A (aeacd8b5)**: 8 new files (tooling/docs/examples)
  - Verify no build artifacts committed
  - Check scripts are executable (`chmod +x`)
  - Confirm docs are accurate
  
- [ ] **Commit B (c487686e)**: 32 modified files (SDK changes)
  - Review `Relocation-inl.hpp` parsing logic
  - Verify search path order matches documentation
  - Check Windows compatibility is maintained

**Quick validation during review:**
```bash
# Checkout PR branch
git fetch origin pull/1/head:pr-review
git checkout pr-review

# Run validations
python3 scripts/check_addresses.py --strict
python3 scripts/check_loader_addresses.py --strict

# Build smoke test
mkdir build && cd build
cmake .. -DRED4EXT_BUILD_EXAMPLES=ON
cmake --build . --target macos_smoke_test
```

### 1.2 Merge the PR

```bash
# Merge to master
git checkout master
git merge --no-ff feature/macos-port-validation

# Push to origin
git push origin master
```

**Post-merge verification:**
```bash
# Verify commits are on master
git log --oneline -3
# Expected:
# c487686e Add macOS ARM64 support to RED4ext.SDK
# aeacd8b5 Add macOS validation tooling, docs, and smoke test
# 7d25b0b7 ... (previous commit)
```

---

## Phase 2: Align RED4ext SDK Dependency

### 2.1 Determine Your Vendoring Strategy

**Option A: Git Submodule (Recommended)**
If RED4ext uses `deps/red4ext.sdk` as a submodule:

```bash
cd /path/to/RED4ext
git checkout -b update-sdk-macos

# Update submodule to merged commit
cd deps/red4ext.sdk
git fetch origin master
git checkout c487686e  # Use actual merged commit SHA
cd ../..

# Stage submodule update
git add deps/red4ext.sdk
git commit -m "Update RED4ext.SDK to macOS port (v2.3.1 support)"
```

**Option B: Copy Headers (Legacy)**
If RED4ext copies headers into `deps/`:

```bash
cd /path/to/RED4ext

# Backup existing
cp -r deps/RED4ext deps/RED4ext.backup

# Copy updated headers from SDK repo
cp -r /path/to/RED4ext.SDK/include/RED4ext deps/

# Verify key files updated
ls -la deps/RED4ext/Relocation-inl.hpp
ls -la deps/RED4ext/Common.hpp
```

### 2.2 Build RED4ext with Updated SDK

```bash
cd /path/to/RED4ext
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --target RED4ext

# Verify dylib created
ls -la RED4ext.dylib
```

---

## Phase 3: SDK Database Validation

### 3.1 Validate SDK DB (from SDK repo)

```bash
cd /path/to/RED4ext.SDK

# Strict validation
python3 scripts/check_addresses.py --strict
python3 scripts/check_loader_addresses.py --strict
```

**Expected output:**
```
total=126
duplicates=0
zero_offsets=0

db=/path/to/cyberpunk2077_addresses.loader.json
total=134
duplicates=0
zero_offsets=0
missing_required_hooks=0
```

### 3.2 Build Smoke Test

```bash
cd /path/to/RED4ext.SDK
mkdir -p build && cd build
cmake .. -DRED4EXT_BUILD_EXAMPLES=ON
cmake --build . --target macos_smoke_test

# Verify output
file examples/libmacos_smoke_test.dylib
# Expected: Mach-O 64-bit dynamically linked shared library arm64
```

---

## Phase 4: End-to-End Runtime Validation

### 4.1 Install Address Database

**Target location:** `<game_root>/red4ext/bin/x64/cyberpunk2077_addresses.json`

```bash
# Find game installation
GAME_ROOT="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077"

# Create red4ext directory structure
mkdir -p "$GAME_ROOT/red4ext/bin/x64"
mkdir -p "$GAME_ROOT/red4ext/plugins"

# Copy address DB
cp /path/to/RED4ext.SDK/cyberpunk2077_addresses.json \
   "$GAME_ROOT/red4ext/bin/x64/"

# Verify
cat "$GAME_ROOT/red4ext/bin/x64/cyberpunk2077_addresses.json" | head -20
```

### 4.2 Install RED4ext Loader

```bash
# Copy built RED4ext.dylib to game
cp /path/to/RED4ext/build/RED4ext.dylib "$GAME_ROOT/"

# Verify
ls -la "$GAME_ROOT/RED4ext.dylib"
```

### 4.3 Install Smoke Test Plugin

```bash
# Copy smoke test plugin
mkdir -p "$GAME_ROOT/red4ext/plugins/smoke_test"
cp /path/to/RED4ext.SDK/build/examples/libmacos_smoke_test.dylib \
   "$GAME_ROOT/red4ext/plugins/smoke_test/"

# Create manifest
cat > "$GAME_ROOT/red4ext/plugins/smoke_test/manifest.json" << 'EOF'
{
  "name": "smoke_test",
  "version": "1.0.0",
  "author": "RED4ext",
  "description": "macOS SDK smoke test"
}
EOF
```

### 4.4 Launch Game

```bash
cd "$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077"

# Use the game-local launcher script
./launch_red4ext.sh
```

**Alternative (if script doesn't exist):**
```bash
cd "$GAME_ROOT"

# Set up environment
export DYLD_INSERT_LIBRARIES="$PWD/RED4ext.dylib"

# Launch game
open -W "Cyberpunk 2077.app"
```

### 4.5 Verify Logs

**Check smoke test log:**
```bash
# While game is running or after exit
tail -f /tmp/RED4ext.SDK_smoke_test.log
```

**Expected log entries:**
```
[RED4ext.SDK smoke] Loading...
[RED4ext.SDK smoke] Loaded 126 hashes from /.../cyberpunk2077_addresses.json
[RED4ext.SDK smoke] ImageBase=0x...
[RED4ext.SDK smoke] Done. missing=0 dup=0
```

**Success criteria:**
- [ ] DB load path found (verify path in log)
- [ ] All 126 hashes loaded
- [ ] ImageBase resolved correctly
- [ ] missing=0, dup=0

---

## Phase 5: Unblock Mods

### 5.1 TweakXL Validation

**Install TweakXL:**
```bash
# Copy TweakXL plugin to red4ext plugins folder
mkdir -p "$GAME_ROOT/red4ext/plugins/TweakXL"
cp /path/to/TweakXL.dylib "$GAME_ROOT/red4ext/plugins/TweakXL/"

# Copy TweakXL manifest
cp /path/to/TweakXL/manifest.json "$GAME_ROOT/red4ext/plugins/TweakXL/"
```

**Launch & Verify:**
```bash
cd "$GAME_ROOT"
./launch_red4ext.sh
```

**Check for:**
- [ ] TweakXL loads without errors
- [ ] TweakDB modifications apply correctly
- [ ] No "Missing address hash" warnings for TweakXL-specific hashes

### 5.2 MetalFX Validation

**Install MetalFX:**
```bash
mkdir -p "$GAME_ROOT/red4ext/plugins/MetalFX"
cp /path/to/MetalFX.dylib "$GAME_ROOT/red4ext/plugins/MetalFX/"
cp /path/to/MetalFX/manifest.json "$GAME_ROOT/red4ext/plugins/MetalFX/"
```

**Launch & Verify:**
```bash
cd "$GAME_ROOT"
./launch_red4ext.sh
```

**Check for:**
- [ ] MetalFX hooks install successfully
- [ ] First hook hit logged
- [ ] No crashes during graphics initialization

---

## Troubleshooting

### Issue: "Could not find cyberpunk2077_addresses.json"

**Solution:**
```bash
# Verify file exists in expected location
ls -la "$GAME_ROOT/red4ext/bin/x64/cyberpunk2077_addresses.json"

# Test with explicit env var
export RED4EXT_SDK_ADDRESS_DB="$GAME_ROOT/red4ext/bin/x64/cyberpunk2077_addresses.json"
./launch_red4ext.sh
```

### Issue: "Missing address hash" warnings

**Diagnosis:**
```bash
# Check which hashes are missing
grep "Missing address hash" /tmp/RED4ext.SDK_smoke_test.log

# Verify hash exists in DB
python3 -c "import json; data=json.load(open('cyberpunk2077_addresses.json')); print([a for a in data['Addresses'] if a['hash']=='1234567890'])"
```

**Solution:**
- Add missing hash to `cyberpunk2077_addresses.json`
- Re-run validation scripts
- Restart game

### Issue: Smoke test doesn't load

**Diagnosis:**
```bash
# Check RED4ext logs
ls -la "$GAME_ROOT/RED4ext/logs/"

# Verify plugin structure
ls -la "$GAME_ROOT/red4ext/plugins/smoke_test/"
```

**Solution:**
- Ensure manifest.json exists and is valid
- Check plugin is in correct directory structure
- Verify dylib is arm64 architecture: `file libmacos_smoke_test.dylib`

---

## Sign-Off Checklist

Before declaring integration complete:

- [ ] SDK PR merged to master
- [ ] RED4ext builds with updated SDK
- [ ] `python3 scripts/check_addresses.py --strict` passes
- [ ] Smoke test builds successfully
- [ ] Address DB installed to `red4ext/bin/x64/`
- [ ] Smoke test runs and reports `missing=0 dup=0`
- [ ] TweakXL loads and applies tweaks
- [ ] MetalFX hooks install and hit

**Integration Owner:** _________________ **Date:** _________________

---

## Quick Reference Commands

```bash
# Full validation pipeline
cd /path/to/RED4ext.SDK
python3 scripts/check_addresses.py --strict
python3 scripts/check_loader_addresses.py --strict

# Build everything
mkdir build && cd build
cmake .. -DRED4EXT_BUILD_EXAMPLES=ON
cmake --build . --target RED4ext.SDK macos_smoke_test

# Install to game
GAME_ROOT="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077"
cp cyberpunk2077_addresses.json "$GAME_ROOT/red4ext/bin/x64/"
cp build/examples/libmacos_smoke_test.dylib "$GAME_ROOT/red4ext/plugins/"

# Launch
cd "$GAME_ROOT" && ./launch_red4ext.sh

# Monitor
tail -f /tmp/RED4ext.SDK_smoke_test.log
```
