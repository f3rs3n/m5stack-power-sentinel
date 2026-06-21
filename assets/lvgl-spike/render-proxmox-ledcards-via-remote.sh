#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=${ROOT:-$(cd -- "$SCRIPT_DIR/../.." && pwd)}
SPIKE="$ROOT/assets/lvgl-spike"
REMOTE=${REMOTE:-C:/Users/Public/power-sentinel-lvgl-spike-work}
REMOTE_HOST=${REMOTE_HOST:-doomtrain}

SPIKE_PATH="$SPIKE" python3 - <<'PY'
from pathlib import Path
import os
import re
root = Path(os.environ['SPIKE_PATH'])
fixture = (root / 'power-sentinel-proxmox-ledcards-fixture.c').read_text()
fixture = fixture.replace('#include "ps_font_ddin_condensed_bold_60.c"\n#include "ps_font_ddin_condensed_bold_40.c"\n#include "ps_icon_chart_32.c"\n#include "ps_icon_status_14.c"\n', '')
fixture = fixture.replace('&lv_font_montserrat_10', '&lv_font_montserrat_12')

def prefix_font(path: Path, prefix: str) -> str:
    text = path.read_text()
    for name in [
        'glyph_bitmap', 'glyph_dsc', 'unicode_list_0', 'cmaps',
        'kern_pair_glyph_ids', 'kern_pair_values', 'kern_pairs', 'font_dsc',
    ]:
        text = re.sub(rf'(?<![\\.A-Za-z0-9_]){name}(?![A-Za-z0-9_])', f'{prefix}_{name}', text)
    return text

combined = (
    '/* Auto-generated combined LVGL MCP render fixture. Do not port this file to firmware. */\n'
    + prefix_font(root / 'ps_font_ddin_condensed_bold_60.c', 'ddin60')
    + '\n'
    + prefix_font(root / 'ps_font_ddin_condensed_bold_40.c', 'ddin40')
    + '\n'
    + prefix_font(root / 'ps_icon_chart_32.c', 'chart32')
    + '\n'
    + prefix_font(root / 'ps_icon_status_14.c', 'status14')
    + '\n'
    + fixture
)
(root / 'power-sentinel-proxmox-ledcards-fixture-combined.c').write_text(combined)
PY

ssh "$REMOTE_HOST" "powershell -NoProfile -ExecutionPolicy Bypass -Command \"New-Item -ItemType Directory -Force -Path '$REMOTE' | Out-Null; New-Item -ItemType Directory -Force -Path '$REMOTE/results' | Out-Null\""
scp "$SPIKE/run-lvgl-mcp-render.mjs" "$SPIKE/lvgl-mcp-render-helper.mjs" "$SPIKE/power-sentinel-proxmox-ledcards-fixture-combined.c" "$REMOTE_HOST:$REMOTE/" >/dev/null

ssh "$REMOTE_HOST" "powershell -NoProfile -ExecutionPolicy Bypass -Command \"
  Set-Location '$REMOTE';
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-proxmox-ledcards-fixture-combined.c --name proxmox-ledcards-nominal --define PS_PROXMOX_STATE=0 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-proxmox-ledcards-fixture-combined.c --name proxmox-ledcards-storage-warning --define PS_PROXMOX_STATE=1 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-proxmox-ledcards-fixture-combined.c --name proxmox-ledcards-guest-critical --define PS_PROXMOX_STATE=2 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-proxmox-ledcards-fixture-combined.c --name proxmox-ledcards-unconfigured --define PS_PROXMOX_STATE=3 | Out-Null;
\""

mkdir -p "$SPIKE/results"
for name in nominal storage-warning guest-critical unconfigured; do
  scp "$REMOTE_HOST:$REMOTE/results/proxmox-ledcards-$name.png" "$SPIKE/results/proxmox-ledcards-$name.png" >/dev/null
  scp "$REMOTE_HOST:$REMOTE/results/proxmox-ledcards-$name-render-result.json" "$SPIKE/results/proxmox-ledcards-$name-render-result.json" >/dev/null
  scp "$REMOTE_HOST:$REMOTE/results/proxmox-ledcards-$name-widget-tree.json" "$SPIKE/results/proxmox-ledcards-$name-widget-tree.json" >/dev/null || true
done

printf 'Rendered Proxmox Ledcards fixture via remote LVGL MCP host. Individual PNGs:\n'
printf '  %s\n' \
  "$SPIKE/results/proxmox-ledcards-nominal.png" \
  "$SPIKE/results/proxmox-ledcards-storage-warning.png" \
  "$SPIKE/results/proxmox-ledcards-guest-critical.png" \
  "$SPIKE/results/proxmox-ledcards-unconfigured.png"
