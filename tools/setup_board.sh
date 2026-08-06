#!/usr/bin/env bash
# One-time setup: registers Teknic's ClearCore board package with arduino-cli
# and installs it (compiler, bossac, CMSIS all come bundled per the package's
# toolsDependencies -- nothing to install separately for the build itself).
#
# Run from the project root: ./tools/setup_board.sh

set -euo pipefail

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "arduino-cli not found on PATH. Install it first:" >&2
  echo "  https://arduino.github.io/arduino-cli/latest/installation/" >&2
  exit 1
fi

arduino-cli config init --overwrite=false || true

arduino-cli config add board_manager.additional_urls \
  https://www.teknic.com/files/downloads/package_clearcore_index.json

arduino-cli core update-index
arduino-cli core install ClearCore:sam

echo
echo "Installed. Verify with:"
echo "  arduino-cli board listall | grep -i clearcore"
echo
echo "Find your arduino15 data directory (for GDB path / IntelliSense) with:"
echo "  arduino-cli config dump | grep directories -A2"
