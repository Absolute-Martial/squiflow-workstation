#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
prefix="${1:-$root/build/ci-libsodium}"
archive="$root/external/dist/libsodium-1.0.22.tar.gz"
test -f "$archive" || { echo "missing $archive" >&2; exit 2; }
rm -rf "$root/build/ci-libsodium-src" "$prefix"
mkdir -p "$root/build/ci-libsodium-src"
tar -xzf "$archive" -C "$root/build/ci-libsodium-src" --strip-components=1
cd "$root/build/ci-libsodium-src"
./configure --prefix="$prefix" --disable-shared --enable-static >/dev/null
make -j"${SQUIFLOW_BUILD_JOBS:-2}" >/dev/null
make install >/dev/null
test -f "$prefix/lib/libsodium.a"
printf '%s\n' "$prefix"
