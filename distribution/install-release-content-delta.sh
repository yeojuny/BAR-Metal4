#!/bin/sh
set -eu

delta=$1
assets=$2

verify_assets() (
  cd "$1"
  /usr/bin/shasum -a 256 -c "$delta/SHA256SUMS" >/dev/null 2>&1
)

if verify_assets "$assets"; then
  exit 0
fi

if ! verify_assets "$delta/assets"; then
  echo "BAR Metal4: the bundled game update failed verification." >&2
  exit 1
fi

/usr/bin/ditto "$delta/assets" "$assets"
if ! verify_assets "$assets"; then
  echo "BAR Metal4: the game update could not be verified after installation." >&2
  exit 1
fi
