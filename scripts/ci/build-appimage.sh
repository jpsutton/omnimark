#!/usr/bin/env bash
# CI (build): release build + AppImage, written to out/.
#   docker run --rm -v "$PWD:/src" -w /src archlinux:latest scripts/ci/build-appimage.sh
set -euo pipefail
# shellcheck source=scripts/ci/deps.sh
source "$(dirname "$0")/deps.sh"
pacman -S --noconfirm --needed curl file fuse2 patchelf >/dev/null
git config --global --add safe.directory '*'

cmake --preset release -DOMNIMARK_BUILD_TESTS=OFF
cmake --build --preset release
packaging/linux/appimage/build-appimage.sh
mkdir -p out
mv packaging/linux/appimage/Omnimark-*.AppImage out/
ls -la out
