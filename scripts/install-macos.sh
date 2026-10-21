#!/usr/bin/env bash
# Install or update Omnimark on macOS (Apple silicon) from the latest release:
#   curl -fsSL https://raw.githubusercontent.com/jpsutton/omnimark/release/scripts/install-macos.sh | bash
#
# Files that curl downloads aren't quarantined, so the app opens without a
# Gatekeeper prompt. The .dmg is checked against the release's SHA256SUMS.
# Installs into $OMNIMARK_INSTALL_DIR, else /Applications if writable, else
# ~/Applications. Run it again to update.
#
# The app bundles this script and runs it to update itself, in two steps:
#   install-macos.sh --stage
#       Download, verify and unpack the latest release while the app keeps
#       running. Prints the unpacked .app path as the last line.
#   install-macos.sh --apply <staged.app> --app <installed.app> [--wait-pid <pid>] [--relaunch]
#       Wait for the app to quit, swap the bundle, optionally reopen it. If
#       that fails, the old app stays in place and an alert explains why.
set -euo pipefail

base=https://github.com/jpsutton/omnimark/releases/latest/download

die() {
  echo "install-macos.sh: $*" >&2
  exit 1
}

# Downloads the latest .dmg into a new temp folder and copies its .app out,
# into $staged. (Not a $(subshell): macOS's bash 3.2 can drop set -e there.)
stage() {
  local work dmg mnt app
  work=$(mktemp -d "${TMPDIR:-/tmp}/omnimark-update.XXXXXX")
  mnt=$work/mnt
  curl -fsSL -o "$work/SHA256SUMS" "$base/SHA256SUMS"
  dmg=$(awk '$2 ~ /\.dmg$/ { print $2; exit }' "$work/SHA256SUMS")
  [[ -n $dmg ]] || die "the latest release has no macOS .dmg"
  echo "Downloading $dmg" >&2
  curl -fL --progress-bar -o "$work/$dmg" "$base/$dmg"
  (cd "$work" && grep " $dmg\$" SHA256SUMS | shasum -a 256 -c -s) || die "checksum mismatch for $dmg"
  mkdir "$mnt" "$work/app"
  hdiutil attach -quiet -nobrowse -readonly -mountpoint "$mnt" "$work/$dmg"
  app=$(find "$mnt" -maxdepth 1 -name '*.app' | head -n 1)
  if [[ -z $app ]]; then
    hdiutil detach -quiet "$mnt" || true
    die "no app in $dmg"
  fi
  ditto "$app" "$work/app/$(basename "$app")"
  hdiutil detach -quiet "$mnt"
  rm -f "$work/$dmg"
  staged=$work/app/$(basename "$app")
}

# Replaces bundle $2 with $1, keeping the old one until the new one is in place.
swap() {
  local staged=$1 dest=$2
  local next=$dest.new old=$dest.old
  rm -rf "$next" "$old"
  ditto "$staged" "$next" || { rm -rf "$next"; return 1; }
  if [[ -e $dest ]]; then
    mv "$dest" "$old" || { rm -rf "$next"; return 1; }
  fi
  if ! mv "$next" "$dest"; then
    [[ -e $old ]] && mv "$old" "$dest"
    rm -rf "$next"
    return 1
  fi
  rm -rf "$old"
}

apply() {
  local staged=$1 dest=$2 pid=$3 relaunch=$4
  if [[ -n $pid ]]; then
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
      ((waited++ < 600)) || { alert "Omnimark didn't quit, so the update wasn't installed."; exit 1; }
      sleep 0.1
    done
  fi
  if swap "$staged" "$dest"; then
    echo "Installed $dest"
  else
    alert "Omnimark couldn't replace $dest. Check that you can write to $(dirname "$dest")."
    [[ $relaunch == 1 ]] && open "$dest"
    exit 1
  fi
  rm -rf "$(dirname "$(dirname "$staged")")"
  if [[ $relaunch == 1 ]]; then open "$dest"; fi
}

alert() {
  echo "install-macos.sh: $1" >&2
  osascript - "$1" >/dev/null 2>&1 <<'EOF' || true
on run argv
  display alert "Omnimark update failed" message (item 1 of argv) as critical
end run
EOF
}

main() {
  local mode=install staged='' dest='' pid='' relaunch=0
  while (($#)); do
    case $1 in
      --stage) mode=stage ;;
      --apply) mode=apply; staged=${2:?--apply needs a path}; shift ;;
      --app) dest=${2:?--app needs a path}; shift ;;
      --wait-pid) pid=${2:?--wait-pid needs a pid}; shift ;;
      --relaunch) relaunch=1 ;;
      *) die "unknown option: $1" ;;
    esac
    shift
  done

  [[ $(uname -s) == Darwin ]] || die "this script is for macOS"
  [[ $(uname -m) == arm64 ]] || die "Omnimark ships for Apple silicon (arm64) only"

  case $mode in
    stage)
      stage
      echo "$staged"
      ;;
    apply)
      [[ -n $dest ]] || die "--apply needs --app"
      apply "$staged" "$dest" "$pid" "$relaunch"
      ;;
    install)
      local dir=${OMNIMARK_INSTALL_DIR:-}
      if [[ -z $dir ]]; then
        if [[ -w /Applications ]]; then dir=/Applications; else dir=$HOME/Applications; fi
      fi
      mkdir -p "$dir"
      stage
      if pgrep -xq omnimark; then
        echo "Omnimark is running. Quit and reopen it to use the new version."
      fi
      apply "$staged" "$dir/$(basename "$staged")" '' 0
      ;;
  esac
}

# Everything runs from main, so a download cut short by `curl | bash` runs nothing.
main "$@"
