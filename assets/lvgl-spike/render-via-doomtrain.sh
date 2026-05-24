#!/usr/bin/env bash
set -euo pipefail

# Render Power Sentinel LVGL fixtures on the Windows host without writing into
# the Windows git checkout. This avoids pull conflicts from generated files.
#
# Requirements:
# - SSH alias `doomtrain` configured on the Hermes/Linux side.
# - Node/npm and lvgl-mcp-server installed on DOOMTRAIN.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SPIKE_DIR="$ROOT/assets/lvgl-spike"
RESULTS_DIR="$SPIKE_DIR/results"
REMOTE_WORKDIR='C:/Users/marti/Progetti/lvgl-spike-work'
REMOTE_RESULTS="$REMOTE_WORKDIR/results"

mkdir -p "$RESULTS_DIR"

scp \
  "$SPIKE_DIR/run-lvgl-mcp-render.mjs" \
  "$SPIKE_DIR/render-power-sentinel-tabs.mjs" \
  "$SPIKE_DIR/power-sentinel-dashboard-fixture.c" \
  doomtrain:"$REMOTE_WORKDIR/"

ssh doomtrain "powershell -NoProfile -ExecutionPolicy Bypass -Command \"
  New-Item -ItemType Directory -Force -Path '$REMOTE_WORKDIR' | Out-Null;
  New-Item -ItemType Directory -Force -Path '$REMOTE_RESULTS' | Out-Null;
  Set-Location '$REMOTE_WORKDIR';
  node .\\render-power-sentinel-tabs.mjs
\""

for name in home-current nut-current pve-current ha-current m5s-current; do
  scp doomtrain:"$REMOTE_RESULTS/$name.png" "$RESULTS_DIR/$name.png"
done

if command -v ffmpeg >/dev/null 2>&1; then
  (
    cd "$RESULTS_DIR"
    ffmpeg -y \
      -i home-current.png \
      -i nut-current.png \
      -i pve-current.png \
      -i ha-current.png \
      -i m5s-current.png \
      -filter_complex "[0:v]pad=640:720:0:0:color=0x141414[a];[a][1:v]overlay=320:0[b];[b][2:v]overlay=0:240[c];[c][3:v]overlay=320:240[d];[d][4:v]overlay=0:480" \
      dashboard-current-contact-sheet.png >/tmp/power-sentinel-lvgl-ffmpeg.log 2>&1
  )
fi

printf 'Rendered LVGL MCP baselines via doomtrain scratch workspace: %s\n' "$REMOTE_WORKDIR"
printf 'Updated local results: %s\n' "$RESULTS_DIR"
