#!/usr/bin/env bash
# CI (checks): build the app and run the C++ unit and GUI tests.
#   docker run --rm -v "$PWD:/src" -w /src archlinux:latest scripts/ci/test-linux.sh
set -euo pipefail
# shellcheck source=scripts/ci/deps.sh
source "$(dirname "$0")/deps.sh"
git config --global --add safe.directory '*'

cmake --preset debug
cmake --build --preset debug
# GUI tests drive real QtWebEngine pages offscreen; Chromium's sandbox
# refuses to run as root, which a container is.
QTWEBENGINE_DISABLE_SANDBOX=1 ctest --preset debug --output-on-failure
