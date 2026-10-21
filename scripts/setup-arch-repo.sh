#!/usr/bin/env bash
# Adds Omnimark's signed pacman repository to this Arch Linux system.
#
#   scripts/setup-arch-repo.sh             add the repo, trust the key, sync
#   scripts/setup-arch-repo.sh --install   ...and install omnimark
#   scripts/setup-arch-repo.sh --remove    undo: drop the repo and the key
#
# Safe to run again: it only adds what's missing. Re-runs itself with sudo.
set -euo pipefail

REPO_NAME=omnimark
SERVER=https://github.com/jpsutton/omnimark/releases/latest/download
KEY_URL=$SERVER/omnimark-signing-key.asc
# The release key; a downloaded key is only trusted if it matches this.
FINGERPRINT=5F2FBC3C175D49D9F491219897A9E84185E02236
PACMAN_CONF=${PACMAN_CONF:-/etc/pacman.conf}

action=add
case ${1:-} in
  '') ;;
  --install) action=install ;;
  --remove) action=remove ;;
  -h | --help)
    sed -n '2,9s/^# \{0,1\}//p' "$0"
    exit 0
    ;;
  *)
    echo "unknown option: $1 (try --help)" >&2
    exit 2
    ;;
esac

if [[ $EUID -ne 0 ]]; then
  # Through bash, so it also works when run as `bash setup-arch-repo.sh`.
  exec sudo --preserve-env=PACMAN_CONF bash "$0" "$@"
fi

log() { printf '==> %s\n' "$*"; }

has_repo() { grep -q "^\[$REPO_NAME\]" "$PACMAN_CONF"; }

remove_repo() {
  if has_repo; then
    log "Removing [$REPO_NAME] from $PACMAN_CONF (backup: $PACMAN_CONF.omnimark.bak)"
    cp -a "$PACMAN_CONF" "$PACMAN_CONF.omnimark.bak"
    # Delete the section header and its lines up to the next section.
    awk -v name="[$REPO_NAME]" '
      $0 == name { skip = 1; next }
      /^\[/ { skip = 0 }
      !skip
    ' "$PACMAN_CONF.omnimark.bak" > "$PACMAN_CONF"
  fi
  if pacman-key --list-keys "$FINGERPRINT" >/dev/null 2>&1; then
    log "Removing key $FINGERPRINT from the pacman keyring"
    pacman-key --delete "$FINGERPRINT"
  fi
  log "Done. Installed omnimark (if any) is untouched: pacman -Rns omnimark to remove it."
}

trust_key() {
  if pacman-key --list-keys "$FINGERPRINT" >/dev/null 2>&1; then
    log "Release key $FINGERPRINT is already in the pacman keyring"
  else
    local tmp
    tmp=$(mktemp -d)
    trap 'rm -rf "$tmp"' RETURN
    log "Downloading the release key"
    curl -fsSL --retry 3 -o "$tmp/key.asc" "$KEY_URL"
    local got
    got=$(gpg --batch --with-colons --show-keys "$tmp/key.asc" 2>/dev/null | awk -F: '/^fpr/ {print $10; exit}')
    if [[ $got != "$FINGERPRINT" ]]; then
      echo "error: downloaded key has fingerprint '${got:-none}', expected $FINGERPRINT; not trusting it" >&2
      exit 1
    fi
    pacman-key --add "$tmp/key.asc" >/dev/null
  fi
  # Locally sign it so pacman trusts packages signed with it.
  pacman-key --lsign-key "$FINGERPRINT" >/dev/null
  log "Release key trusted"
}

add_repo() {
  if has_repo; then
    log "[$REPO_NAME] is already in $PACMAN_CONF"
    return
  fi
  log "Adding [$REPO_NAME] to $PACMAN_CONF (backup: $PACMAN_CONF.omnimark.bak)"
  cp -a "$PACMAN_CONF" "$PACMAN_CONF.omnimark.bak"
  printf '\n[%s]\nServer = %s\n' "$REPO_NAME" "$SERVER" >> "$PACMAN_CONF"
}

if [[ $action == remove ]]; then
  remove_repo
  exit 0
fi

command -v pacman-key >/dev/null || { echo "error: this is not an Arch-based system (no pacman-key)" >&2; exit 1; }
trust_key
add_repo
log "Syncing package databases"
pacman -Sy
if [[ $action == install ]]; then
  log "Installing omnimark"
  pacman -S --needed --noconfirm omnimark
  pacman -Qi omnimark | grep -E '^(Name|Version|Validated By)'
else
  pacman -Si "$REPO_NAME/omnimark" | grep -E '^(Repository|Name|Version)'
  log "Ready. Install with: sudo pacman -S omnimark"
fi
