#!/usr/bin/env bash
# Static checks that need no build: shell scripts, workflow files, desktop and
# AppStream metadata, XML. CI runs this on every pull request.
set -uo pipefail
cd "$(dirname "$0")/../.." || exit 1
failures=0
check() {
  local label=$1 out
  shift
  if ! out=$("$@" 2>&1); then
    echo "FAIL $label"
    printf '    %s\n' "${out//$'\n'/$'\n'    }"
    failures=$((failures + 1))
  else
    echo "ok   $label"
  fi
}
for f in $(git ls-files '*.sh'); do
  check "bash -n $f" bash -n "$f"
  check "shellcheck $f" shellcheck -x "$f"
done
for f in $(git ls-files '*.xml' '*.svg' 'packaging/macos/*.in'); do
  check "xmllint $f" xmllint --noout "$f"
done
for f in $(git ls-files '*.desktop'); do
  check "desktop-file-validate $f" desktop-file-validate "$f"
done
for f in $(git ls-files '*.metainfo.xml'); do
  check "appstreamcli $f" appstreamcli validate --no-net --explain "$f"
done
check "PKGBUILD" bash -c 'cd packaging/linux/arch && makepkg --printsrcinfo >/dev/null'
exit $((failures > 0))
