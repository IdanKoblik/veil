#!/usr/bin/env bash
set -euo pipefail

PREFIX=${PREFIX:-/usr/local}

files=(
    "$PREFIX/bin/veil"
    "$PREFIX/share/man/man1/veil.1"
)

for file in "${files[@]}"; do
    if [ ! -e "$file" ]; then
        continue
    fi

    echo "Removing $file"
    if [ -w "$(dirname "$file")" ]; then
        rm -f "$file"
    else
        sudo rm -f "$file"
    fi
done

echo "Done."
