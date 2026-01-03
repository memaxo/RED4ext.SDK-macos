#!/bin/bash
#
# RED4ext.SDK macOS Release Packager
# Creates a distributable release archive
#

set -e

VERSION="${1:-1.0.0}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RELEASE_DIR="$PROJECT_DIR/release"
RELEASE_NAME="RED4ext.SDK-macOS-v${VERSION}"

echo "=== RED4ext.SDK macOS Release Builder ==="
echo "Version: $VERSION"
echo ""

# Create release directory
rm -rf "$RELEASE_DIR/$RELEASE_NAME"
mkdir -p "$RELEASE_DIR/$RELEASE_NAME"

echo "Packaging release..."

# Copy headers (main content)
cp -r "$PROJECT_DIR/include" "$RELEASE_DIR/$RELEASE_NAME/"

# Copy source files (for those who need compiled components)
cp -r "$PROJECT_DIR/src" "$RELEASE_DIR/$RELEASE_NAME/"

# Copy cmake files
cp -r "$PROJECT_DIR/cmake" "$RELEASE_DIR/$RELEASE_NAME/"
cp "$PROJECT_DIR/CMakeLists.txt" "$RELEASE_DIR/$RELEASE_NAME/"

# Copy examples
cp -r "$PROJECT_DIR/examples" "$RELEASE_DIR/$RELEASE_NAME/"

# Copy address database
cp "$PROJECT_DIR/cyberpunk2077_addresses.json" "$RELEASE_DIR/$RELEASE_NAME/"

# Copy documentation
cp "$PROJECT_DIR/README.md" "$RELEASE_DIR/$RELEASE_NAME/"
cp "$PROJECT_DIR/LICENSE.md" "$RELEASE_DIR/$RELEASE_NAME/"
cp "$PROJECT_DIR/MACOS_CHANGES.md" "$RELEASE_DIR/$RELEASE_NAME/"
cp "$PROJECT_DIR/THIRD_PARTY_LICENSES.md" "$RELEASE_DIR/$RELEASE_NAME/"
cp "$PROJECT_DIR/CONTRIBUTING.md" "$RELEASE_DIR/$RELEASE_NAME/"

# Create usage instructions
cat > "$RELEASE_DIR/$RELEASE_NAME/INSTALL.md" << 'EOF'
# RED4ext.SDK Installation

## As Git Submodule (Recommended)

```bash
git submodule add https://github.com/memaxo/RED4ext.SDK.git deps/red4ext.sdk
```

## Manual Installation

1. Extract this archive to your project's `deps/` directory
2. Add to your CMakeLists.txt:

```cmake
add_subdirectory(deps/RED4ext.SDK-macOS-vX.X.X)
target_link_libraries(your_plugin PRIVATE RED4ext::SDK)
```

## Address Database

Copy `cyberpunk2077_addresses.json` to your game's `red4ext/` directory.
EOF

# Create archive
cd "$RELEASE_DIR"
zip -r "${RELEASE_NAME}.zip" "$RELEASE_NAME"
tar -czvf "${RELEASE_NAME}.tar.gz" "$RELEASE_NAME"

echo ""
echo "=== Release Created ==="
echo "Directory: $RELEASE_DIR/$RELEASE_NAME"
echo "Archives:"
ls -la "$RELEASE_DIR"/*.zip "$RELEASE_DIR"/*.tar.gz 2>/dev/null
echo ""
echo "SHA256 checksums:"
shasum -a 256 "$RELEASE_DIR"/*.zip "$RELEASE_DIR"/*.tar.gz 2>/dev/null
