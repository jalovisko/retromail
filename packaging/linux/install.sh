#!/usr/bin/env sh
# Build RetroMail from source and install it, including the menu entry and icon.
#
#   ./packaging/linux/install.sh                 # install for the current user (~/.local)
#   sudo ./packaging/linux/install.sh            # install system-wide (/usr/local)
#   ./packaging/linux/install.sh --prefix /opt/retromail
#   ./packaging/linux/install.sh --uninstall
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
BUILD_DIR="$ROOT/build-install"
if [ "$(id -u)" -eq 0 ]; then PREFIX=/usr/local; else PREFIX="$HOME/.local"; fi
UNINSTALL=0

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix) PREFIX="$2"; shift 2 ;;
        --prefix=*) PREFIX="${1#*=}"; shift ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --uninstall) UNINSTALL=1; shift ;;
        -h|--help) sed -n '2,8p' "$0"; exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

refresh_desktop() {
    command -v update-desktop-database >/dev/null 2>&1 \
        && update-desktop-database -q "$PREFIX/share/applications" 2>/dev/null || true
    command -v gtk-update-icon-cache >/dev/null 2>&1 \
        && gtk-update-icon-cache -q -t "$PREFIX/share/icons/hicolor" 2>/dev/null || true
}

if [ "$UNINSTALL" -eq 1 ]; then
    MANIFEST="$BUILD_DIR/install_manifest.txt"
    if [ ! -f "$MANIFEST" ]; then
        echo "No install manifest at $MANIFEST; nothing to uninstall." >&2
        exit 1
    fi
    echo "Removing files listed in $MANIFEST"
    while IFS= read -r f; do
        [ -n "$f" ] && rm -f -- "$f" && echo "  removed $f"
    done < "$MANIFEST"
    refresh_desktop
    echo "RetroMail uninstalled. Your settings stay in ~/.config/RetroMail."
    exit 0
fi

for tool in cmake c++; do
    command -v "$tool" >/dev/null 2>&1 || { echo "Missing '$tool'. See README.md for build dependencies." >&2; exit 1; }
done
GENERATOR=""
command -v ninja >/dev/null 2>&1 && GENERATOR="-G Ninja"

echo "==> Configuring (install prefix: $PREFIX)"
# shellcheck disable=SC2086
cmake -S "$ROOT" -B "$BUILD_DIR" $GENERATOR \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DRETROMAIL_BUILD_TESTS=OFF

echo "==> Building"
cmake --build "$BUILD_DIR" --parallel

echo "==> Installing"
cmake --install "$BUILD_DIR"
refresh_desktop

echo
echo "RetroMail is installed. Start it from your applications menu or run:"
echo "  $PREFIX/bin/retromail"
case ":$PATH:" in
    *":$PREFIX/bin:"*) ;;
    *) echo "(Note: $PREFIX/bin is not on your PATH.)" ;;
esac
