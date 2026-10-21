#!/bin/sh
set -e
cd "$(dirname "$0")/.."
BIN=${CHAIRS:-./build/chairs}
mkdir -p renders/takes
for take in takes/*.take; do
  out=renders/takes/$(basename "$take" .take).mp4
  [ -f "$out" ] && continue
  "$BIN" --replay "$take" --resolution 3840x2160 --record "$out" > /dev/null
  uv run --project ../supertune python tools/music.py "$out"
done
