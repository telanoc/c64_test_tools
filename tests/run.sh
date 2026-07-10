#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if ! command -v uv >/dev/null 2>&1 || ! command -v uvx >/dev/null 2>&1; then
  echo "error: install uv (which provides both uv and uvx)" >&2
  exit 2
fi

PIO=(uvx --from platformio==6.1.19 platformio)
PYTHON=(uv run --no-project --python 3.12)

echo "==> Compiling production sketch"
"${PIO[@]}" run --environment mega2560

echo "==> Compiling optional timing diagnostics"
"${PIO[@]}" run --environment mega2560-testing

echo "==> Compiling simavr timing firmware"
"${PIO[@]}" run --project-conf tests/simavr/platformio.ini --environment timing

SIMAVR="$ROOT/.pio-core/packages/tool-simavr/bin/simavr"
FIRMWARE="$ROOT/.pio/build/timing/firmware.elf"
TRACE="$ROOT/.pio/timing.vcd"

if [[ ! -x "$SIMAVR" ]]; then
  echo "error: PlatformIO did not install simavr at $SIMAVR" >&2
  exit 2
fi

rm -f "$TRACE"

echo "==> Simulating ATmega2560 refresh cycles"
"$SIMAVR" "$FIRMWARE"

echo "==> Checking maximum row refresh interval"
"${PYTHON[@]}" tests/simavr/check_refresh_timing.py \
  "$TRACE" \
  --clock-hz 16000000 \
  --limit-us 4000 \
  --minimum-refreshes 3
