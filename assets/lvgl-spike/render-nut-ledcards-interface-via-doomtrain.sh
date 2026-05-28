#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-/home/martino/projects/m5stack-power-sentinel}
SPIKE="$ROOT/assets/lvgl-spike"
REMOTE=${REMOTE:-C:/Users/marti/Progetti/lvgl-spike-work}
REMOTE_HOST=${REMOTE_HOST:-doomtrain}

python3 - <<'PY'
from pathlib import Path
import re
root = Path('/home/martino/projects/m5stack-power-sentinel/assets/lvgl-spike')
fixture = (root / 'power-sentinel-nut-ledcards-interface-fixture.c').read_text()
fixture = fixture.replace('#include "ps_font_ddin_condensed_bold_60.c"\n#include "ps_font_ddin_condensed_bold_40.c"\n#include "ps_icon_chart_32.c"\n', '')
# The LVGL MCP simulator currently exposes Montserrat 12+, while firmware also
# has Montserrat 10. Use 12 only in the combined scratch source sent to MCP.
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
    + fixture
)
(root / 'power-sentinel-nut-ledcards-interface-fixture-combined.c').write_text(combined)
PY

ssh "$REMOTE_HOST" "powershell -NoProfile -ExecutionPolicy Bypass -Command \"New-Item -ItemType Directory -Force -Path '$REMOTE' | Out-Null; New-Item -ItemType Directory -Force -Path '$REMOTE/results' | Out-Null\""
scp "$SPIKE/run-lvgl-mcp-render.mjs" "$SPIKE/lvgl-mcp-render-helper.mjs" "$SPIKE/power-sentinel-nut-ledcards-interface-fixture-combined.c" "$REMOTE_HOST:$REMOTE/" >/dev/null

ssh "$REMOTE_HOST" "powershell -NoProfile -ExecutionPolicy Bypass -Command \"
  Set-Location '$REMOTE';
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-nut-ledcards-interface-fixture-combined.c --name nut-ledcards-interface-nominal --define PS_NUT_HOME_STATE=0 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-nut-ledcards-interface-fixture-combined.c --name nut-ledcards-interface-onbattery --define PS_NUT_HOME_STATE=1 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-nut-ledcards-interface-fixture-combined.c --name nut-ledcards-interface-lowbattery --define PS_NUT_HOME_STATE=2 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-nut-ledcards-interface-fixture-combined.c --name nut-ledcards-interface-stale --define PS_NUT_HOME_STATE=3 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-nut-ledcards-interface-fixture-combined.c --name nut-ledcards-interface-highload --define PS_NUT_HOME_STATE=4 | Out-Null;
  node .\\run-lvgl-mcp-render.mjs --source .\\power-sentinel-nut-ledcards-interface-fixture-combined.c --name nut-ledcards-interface-inputlow --define PS_NUT_HOME_STATE=5 | Out-Null;
\""

mkdir -p "$SPIKE/results"
for name in nominal onbattery lowbattery stale highload inputlow; do
  scp "$REMOTE_HOST:$REMOTE/results/nut-ledcards-interface-$name.png" "$SPIKE/results/nut-ledcards-interface-$name.png" >/dev/null
  scp "$REMOTE_HOST:$REMOTE/results/nut-ledcards-interface-$name-render-result.json" "$SPIKE/results/nut-ledcards-interface-$name-render-result.json" >/dev/null
  scp "$REMOTE_HOST:$REMOTE/results/nut-ledcards-interface-$name-widget-tree.json" "$SPIKE/results/nut-ledcards-interface-$name-widget-tree.json" >/dev/null || true
done

python3 - <<'PY' || true
from pathlib import Path
try:
    from PIL import Image, ImageDraw, ImageFont
except Exception:
    raise SystemExit('Pillow is unavailable; skipped contact-sheet generation after copying individual MCP PNGs.')
root = Path('/home/martino/projects/m5stack-power-sentinel/assets/lvgl-spike/results')
try:
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 10)
except Exception:
    font = None
states = [
    ('nominal', 'nut-ledcards-interface-nominal.png'),
    ('on battery', 'nut-ledcards-interface-onbattery.png'),
    ('low battery', 'nut-ledcards-interface-lowbattery.png'),
    ('stale', 'nut-ledcards-interface-stale.png'),
    ('high load', 'nut-ledcards-interface-highload.png'),
    ('input low', 'nut-ledcards-interface-inputlow.png'),
]
sheet = Image.new('RGB', (960, 528), (20, 20, 20))
d = ImageDraw.Draw(sheet)
for i, (title, filename) in enumerate(states):
    img = Image.open(root / filename).convert('RGB')
    x = (i % 3) * 320
    y = (i // 3) * 264
    d.text((x + 6, y + 5), 'LVGL MCP - ' + title, fill=(230, 230, 230), font=font)
    sheet.paste(img, (x, y + 24))
sheet.save(root / 'nut-ledcards-interface-mcp-6state-contact-sheet.png')

sample_path = Path('/tmp/power-sentinel-images/ledcards-interface-example.webp')
if sample_path.exists():
    sample = Image.open(sample_path).convert('RGB').resize((254, 240))
    comp = Image.new('RGB', (1214, 528), (20, 20, 20))
    d = ImageDraw.Draw(comp)
    d.text((6, 5), 'reference sample', fill=(230, 230, 230), font=font)
    comp.paste(sample, (0, 24))
    for i, (title, filename) in enumerate(states):
        img = Image.open(root / filename).convert('RGB')
        x = 254 + (i % 3) * 320
        y = (i // 3) * 264
        d.text((x + 6, y + 5), 'LVGL MCP - ' + title, fill=(230, 230, 230), font=font)
        comp.paste(img, (x, y + 24))
    comp.save(root / 'nut-ledcards-interface-mcp-6state-vs-reference.png')
PY

if [ ! -f "$SPIKE/results/nut-ledcards-interface-mcp-6state-contact-sheet.png" ] && command -v ffmpeg >/dev/null 2>&1; then
  ffmpeg -y \
    -i "$SPIKE/results/nut-ledcards-interface-nominal.png" \
    -i "$SPIKE/results/nut-ledcards-interface-onbattery.png" \
    -i "$SPIKE/results/nut-ledcards-interface-lowbattery.png" \
    -i "$SPIKE/results/nut-ledcards-interface-stale.png" \
    -i "$SPIKE/results/nut-ledcards-interface-highload.png" \
    -i "$SPIKE/results/nut-ledcards-interface-inputlow.png" \
    -filter_complex "xstack=inputs=6:layout=0_0|320_0|640_0|0_240|320_240|640_240" \
    "$SPIKE/results/nut-ledcards-interface-mcp-6state-contact-sheet.png" >/dev/null 2>&1 || true
fi

printf 'Rendered NUT Ledcards Interface fixture via DOOMTRAIN MCP. Individual PNGs:\n'
printf '  %s\n' \
  "$SPIKE/results/nut-ledcards-interface-nominal.png" \
  "$SPIKE/results/nut-ledcards-interface-onbattery.png" \
  "$SPIKE/results/nut-ledcards-interface-lowbattery.png" \
  "$SPIKE/results/nut-ledcards-interface-stale.png" \
  "$SPIKE/results/nut-ledcards-interface-highload.png" \
  "$SPIKE/results/nut-ledcards-interface-inputlow.png"
printf 'Contact sheets are generated when Pillow is available in the shell Python.\n'
