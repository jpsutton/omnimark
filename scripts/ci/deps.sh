#!/usr/bin/env bash
# Sourced by the CI scripts: installs the Arch packages Omnimark builds with.
# Runs in an archlinux container (as root).
set -euo pipefail
pacman -Syu --noconfirm --needed \
  base-devel git cmake ninja nodejs pnpm \
  qt6-base qt6-webengine qt6-webchannel qt6-svg qt6-wayland qt6-serialport \
  ttf-dejavu >/dev/null
