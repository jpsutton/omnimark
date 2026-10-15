#!/usr/bin/env bash
# Builds Omnimark-x86_64.AppImage with linuxdeploy and its Qt plugin.
# Needs qt6-serialport installed (QtPositioning's NMEA plugin, pulled in by
# QtWebEngine, links against it).
# Run from the repository root after a release build:
#   cmake --preset release && cmake --build --preset release
#   packaging/linux/appimage/build-appimage.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
WORK="$ROOT/packaging/linux/appimage"
APPDIR="$WORK/AppDir"
TOOLS="$WORK/tools"
ARCH="${ARCH:-x86_64}"

mkdir -p "$TOOLS"
fetch() {
  local url="$1" out
  out="$TOOLS/$(basename "$1")"
  [[ -x "$out" ]] || { curl -fL --retry 3 -o "$out" "$url"; chmod +x "$out"; }
}
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage"

rm -rf "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$ROOT/build/release" --prefix /usr

export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
export EXTRA_QT_MODULES="webenginecore;webenginewidgets;webchannel"
# Native Wayland in addition to xcb (the plugin deploys the shell/decoration plugins with it).
export EXTRA_PLATFORM_PLUGINS="libqwayland.so"
export EXTRA_QT_PLUGINS="wayland-graphics-integration-client;wayland-shell-integration;wayland-decoration-client"
export APPIMAGE_EXTRACT_AND_RUN=1
# linuxdeploy bundles an old strip that rejects .relr.dyn sections on current distros.
export NO_STRIP=1
export OUTPUT="Omnimark-$ARCH.AppImage"

cd "$WORK"
"$TOOLS/linuxdeploy-$ARCH.AppImage" \
  --appdir "$APPDIR" \
  --desktop-file "$APPDIR/usr/share/applications/io.github.omnimark.Omnimark.desktop" \
  --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/io.github.omnimark.Omnimark.png" \
  --plugin qt

# AppRun is a symlink, so Qt resolves its application directory to the AppDir
# root and looks for qt.conf there (the plugin only writes usr/bin/qt.conf).
cat > "$APPDIR/qt.conf" <<'CONF'
[Paths]
Prefix = usr
Plugins = plugins
LibraryExecutables = libexec
Translations = translations
Data = .
CONF

# The Qt plugin skips Wayland's EGL buffer integration; copy it in and let
# linuxdeploy resolve its libraries.
QT_PLUGIN_DIR="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
EXTRA_DEPS=()
EXTRA_PLUGIN_DIRS=(wayland-graphics-integration-client)
for dir in "${EXTRA_PLUGIN_DIRS[@]}"; do
  if [[ -d "$QT_PLUGIN_DIR/$dir" && ! -d "$APPDIR/usr/plugins/$dir" ]]; then
    cp -r "$QT_PLUGIN_DIR/$dir" "$APPDIR/usr/plugins/"
    EXTRA_DEPS+=(--deploy-deps-only "$APPDIR/usr/plugins/$dir")
  fi
done

"$TOOLS/linuxdeploy-$ARCH.AppImage" --appdir "$APPDIR" "${EXTRA_DEPS[@]}" --output appimage

echo "Built $WORK/$OUTPUT"
