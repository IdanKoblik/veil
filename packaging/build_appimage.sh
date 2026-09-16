#!/usr/bin/env bash
set -euo pipefail

ARCH=$(uname -m)
BUILD_DIR=build/appimage
APPDIR=$BUILD_DIR/AppDir

cd "$(dirname "$0")/.."

for tool in cmake curl magick; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$0: $tool is required" >&2
        exit 1
    fi
done

echo "Building veil-gui"
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DVEIL_BUILD_CLI=OFF \
    -DVEIL_BUILD_GUI=ON \
    -DVEIL_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" -j"$(nproc)"

rm -rf "$APPDIR"
DESTDIR="$PWD/$APPDIR" cmake --install "$BUILD_DIR"

linuxdeploy=$BUILD_DIR/linuxdeploy-$ARCH.AppImage
if [ ! -x "$linuxdeploy" ]; then
    echo "Downloading linuxdeploy"
    curl -fL -o "$linuxdeploy" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
    chmod +x "$linuxdeploy"
fi

# linuxdeploy only accepts icons in the standard sizes.
magick assets/logo.png -resize 512x512 "$BUILD_DIR/veil-gui.png"

cat > "$BUILD_DIR/veil-gui.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Veil
Comment=Steganography inspector
Exec=veil-gui
Icon=veil-gui
Categories=Utility;
DESKTOP

echo "Packaging AppImage"
APPIMAGE_EXTRACT_AND_RUN=1 LINUXDEPLOY_OUTPUT_VERSION="$(cat VERSION)" \
    "$linuxdeploy" \
    --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/veil-gui" \
    --desktop-file "$BUILD_DIR/veil-gui.desktop" \
    --icon-file "$BUILD_DIR/veil-gui.png" \
    --output appimage

echo "Done:"
ls -1 Veil-*.AppImage
