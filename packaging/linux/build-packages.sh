#!/usr/bin/env sh
# Build Linux packages into ./dist:
#   - .tar.gz          always
#   - .deb             always (dependencies are filled in automatically on Debian/Ubuntu)
#   - .rpm             when rpmbuild is installed
#
# Usage: ./packaging/linux/build-packages.sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD_DIR="$ROOT/build-package"
DIST="$ROOT/dist"

GENERATOR=""
command -v ninja >/dev/null 2>&1 && GENERATOR="-G Ninja"

# shellcheck disable=SC2086
cmake -S "$ROOT" -B "$BUILD_DIR" $GENERATOR \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DRETROMAIL_BUILD_TESTS=OFF
cmake --build "$BUILD_DIR" --parallel

GENERATORS="TGZ;DEB"
command -v rpmbuild >/dev/null 2>&1 && GENERATORS="$GENERATORS;RPM"

mkdir -p "$DIST"
(cd "$BUILD_DIR" && cpack -G "$GENERATORS" -B "$DIST")
rm -rf "$DIST/_CPack_Packages"

echo
echo "Packages written to $DIST:"
ls -1 "$DIST"
