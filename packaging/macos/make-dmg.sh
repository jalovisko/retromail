#!/usr/bin/env bash
# Build RetroMail.app and wrap it in a drag-to-Applications disk image.
#
#   ./packaging/macos/make-dmg.sh                 # uses Qt from Homebrew
#   QT_DIR=~/Qt/6.9.0/macos ./packaging/macos/make-dmg.sh
#   SIGN_IDENTITY="Developer ID Application: ..." ./packaging/macos/make-dmg.sh
#
# Output: dist/RetroMail-<version>-macOS.dmg
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR="$ROOT/build-macos"
STAGE="$BUILD_DIR/stage"
DIST="$ROOT/dist"

if [[ -z "${QT_DIR:-}" ]]; then
    if command -v brew >/dev/null 2>&1 && brew --prefix qt >/dev/null 2>&1; then
        QT_DIR=$(brew --prefix qt)
    else
        echo "Qt not found. Install it with 'brew install qt' or set QT_DIR." >&2
        exit 1
    fi
fi

GENERATOR=()
command -v ninja >/dev/null 2>&1 && GENERATOR=(-G Ninja)

echo "==> Configuring with Qt from $QT_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" "${GENERATOR[@]}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_DIR" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    -DRETROMAIL_FETCH_QTKEYCHAIN=ON \
    -DRETROMAIL_BUILD_TESTS=OFF

echo "==> Building"
cmake --build "$BUILD_DIR" --parallel

echo "==> Bundling Qt into RetroMail.app (macdeployqt)"
rm -rf "$STAGE"
cmake --install "$BUILD_DIR" --prefix "$STAGE"
APP="$STAGE/RetroMail.app"
[[ -d "$APP" ]] || { echo "Expected $APP after install" >&2; exit 1; }

echo "==> Signing"
# Ad-hoc signature by default: required on Apple Silicon, but Gatekeeper will
# still warn on other Macs. Set SIGN_IDENTITY for a proper Developer ID build.
if [[ -n "${SIGN_IDENTITY:-}" ]]; then
    codesign --force --deep --options runtime --timestamp --sign "$SIGN_IDENTITY" "$APP"
else
    codesign --force --deep --sign - "$APP"
fi

VERSION=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' "$BUILD_DIR/CMakeCache.txt")
DMG="$DIST/RetroMail-${VERSION}-macOS.dmg"
mkdir -p "$DIST"
rm -f "$DMG"

echo "==> Creating $DMG"
DMG_ROOT=$(mktemp -d)
trap 'rm -rf "$DMG_ROOT"' EXIT
cp -R "$APP" "$DMG_ROOT/"
ln -s /Applications "$DMG_ROOT/Applications"
hdiutil create -volname "RetroMail" -srcfolder "$DMG_ROOT" -ov -format UDZO "$DMG" >/dev/null

if [[ -n "${SIGN_IDENTITY:-}" ]]; then
    codesign --sign "$SIGN_IDENTITY" "$DMG"
fi

echo
echo "Done: $DMG"
