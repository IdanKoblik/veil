#!/usr/bin/env bash
#
# Emscripten has to be on PATH: install the emsdk and source its env script, or
# on a distribution that packages it, put emcc's directory on PATH. Everything
# else is fetched and built here, into web/.deps, and nothing is installed
# system wide.
#
#   ./web/build.sh          build
#   ./web/build.sh clean    throw away .deps, the build tree and dist
#
set -euo pipefail

WEB_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname -- "$WEB_DIR")"

DEPS_DIR="$WEB_DIR/.deps"
BUILD_DIR="$WEB_DIR/.build"
DIST_DIR="$WEB_DIR/dist"

SODIUM_VERSION="${SODIUM_VERSION:-1.0.20-RELEASE}"
SODIUM_URL="https://github.com/jedisct1/libsodium/archive/refs/tags/${SODIUM_VERSION}.tar.gz"

if [ "${1:-}" = "clean" ]; then
    rm -rf "$DEPS_DIR" "$BUILD_DIR" "$DIST_DIR"
    echo "cleaned"
    exit 0
fi

for tool in emcc emcmake emconfigure embuilder cmake curl tar; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "error: $tool is not on PATH" >&2
        exit 1
    }
done

echo "==> libjpeg (emscripten port)"
embuilder build libjpeg

if [ ! -f "$DEPS_DIR/lib/libsodium.a" ]; then
    echo "==> libsodium $SODIUM_VERSION"

    src="$DEPS_DIR/src"
    mkdir -p "$src"
    curl -fsSL "$SODIUM_URL" | tar xz -C "$src" --strip-components=1

    (
        cd "$src"
        emconfigure ./configure \
            --prefix="$DEPS_DIR" \
            --host=wasm32-unknown-emscripten \
            --disable-shared \
            --enable-static \
            --without-pthreads \
            --disable-ssp \
            --disable-pie \
            --disable-asm
        emmake make -j"$(nproc)"
        emmake make install
    )
else
    echo "==> libsodium $SODIUM_VERSION (cached)"
fi

echo "==> veil.wasm"
PKG_CONFIG_LIBDIR="$DEPS_DIR/lib/pkgconfig" \
emcmake cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DVEIL_BUILD_WEB=ON \
    -DVEIL_BUILD_CLI=OFF \
    -DVEIL_BUILD_GUI=OFF \
    -DVEIL_BUILD_TESTS=OFF \
    -DCMAKE_FIND_ROOT_PATH="$DEPS_DIR"

cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
echo "built $DIST_DIR/veil.mjs and $DIST_DIR/veil.wasm"
echo "serve the directory over http, wasm will not load from a file:// url:"
echo
echo "    python3 -m http.server -d $WEB_DIR 8000"
