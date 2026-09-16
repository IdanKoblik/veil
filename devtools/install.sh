#!/usr/bin/env bash
set -euo pipefail

REPO=IdanKoblik/veil
PREFIX=${PREFIX:-/usr/local}
VERSION=${VERSION:-}

for tool in curl sha256sum; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$0: $tool is required" >&2
        exit 1
    fi
done

arch=$(uname -m)
case "$arch" in
    x86_64 | aarch64) ;;
    *)
        echo "$0: no release binary for $arch, build from source instead" >&2
        exit 1
        ;;
esac

if [ -z "$VERSION" ]; then
    latest=$(curl -fsSLI -o /dev/null -w '%{url_effective}' "https://github.com/$REPO/releases/latest")
    VERSION=${latest##*/v}
fi
VERSION=${VERSION#v}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"

url="https://github.com/$REPO/releases/download/v$VERSION"
binary="veil-$VERSION-linux-$arch"

echo "Downloading veil $VERSION ($arch)"
curl -fsSLO "$url/checksums.txt"
curl -fsSLO "$url/$binary"

# Releases before the man page was published do not ship one.
if grep -q ' veil\.1$' checksums.txt; then
    curl -fsSLO "$url/veil.1"
fi

echo "Verifying checksums"
sha256sum -c --ignore-missing checksums.txt

sudo=""
if ! mkdir -p "$PREFIX" 2>/dev/null || [ ! -w "$PREFIX" ]; then
    sudo="sudo"
fi

echo "Installing to $PREFIX"
$sudo install -Dm755 "$binary" "$PREFIX/bin/veil"
if [ -f veil.1 ]; then
    $sudo install -Dm644 veil.1 "$PREFIX/share/man/man1/veil.1"
fi

echo "Done. Run 'veil help' to get started."
