#!/usr/bin/env bash
set -euo pipefail

# Generate the tiny LVGL font used by the CoreS3 left sidebar.
# It merges Montserrat ASCII with a deliberately small Symbols Nerd Font Mono
# subset so tab labels can contain one icon plus the HM/NT/PV/HA/M5 text.
# Do not include a full Nerd Font in firmware; add only explicit PUA glyphs here.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="$ROOT/.cache/lvgl-fonts"
OUT="$ROOT/firmware/core-s3-display/src/ps_ui_tab_12.c"
TMP_OUT="$CACHE_DIR/ps_ui_tab_12.c"

MONTSERRAT_URL="https://github.com/googlefonts/montserrat/raw/master/fonts/ttf/Montserrat-Regular.ttf"
NERD_SYMBOLS_URL="https://github.com/ryanoasis/nerd-fonts/releases/latest/download/NerdFontsSymbolsOnly.zip"
MONTSERRAT_TTF="$CACHE_DIR/Montserrat-Regular.ttf"
NERD_ZIP="$CACHE_DIR/NerdFontsSymbolsOnly.zip"
NERD_TTF="$CACHE_DIR/SymbolsNerdFontMono-Regular.ttf"

mkdir -p "$CACHE_DIR"

if [[ ! -f "$MONTSERRAT_TTF" ]]; then
  curl -L --fail --retry 2 -o "$MONTSERRAT_TTF" "$MONTSERRAT_URL"
fi

if [[ ! -f "$NERD_ZIP" ]]; then
  curl -L --fail --retry 2 -o "$NERD_ZIP" "$NERD_SYMBOLS_URL"
fi

if [[ ! -f "$NERD_TTF" ]]; then
  python3 - "$NERD_ZIP" "$NERD_TTF" <<'PY'
import pathlib
import sys
import zipfile
zip_path = pathlib.Path(sys.argv[1])
out_path = pathlib.Path(sys.argv[2])
with zipfile.ZipFile(zip_path) as zf:
    out_path.write_bytes(zf.read("SymbolsNerdFontMono-Regular.ttf"))
PY
fi

# PUA glyphs selected from Nerd Fonts:
# U+F013 fa-gear, U+F015 fa-home, U+F233 fa-server,
# U+F06F8 nf-md-nut, U+F07D0 nf-md-home_assistant.
# Keep these synchronized with PS_ICON_* in main.cpp and
# assets/lvgl-spike/power-sentinel-dashboard-fixture.c.
"${NPX:-npx}" --yes lv_font_conv \
  --font "$MONTSERRAT_TTF" -r 0x20-0x7E \
  --font "$NERD_TTF" -r 0xF013,0xF015,0xF233,0xF06F8,0xF07D0 \
  --size 12 --bpp 4 --format lvgl --no-compress --no-kerning \
  --lv-include lvgl.h \
  -o "$TMP_OUT"

python3 - "$TMP_OUT" "$OUT" <<'PY'
import pathlib
import sys
src = pathlib.Path(sys.argv[1])
dst = pathlib.Path(sys.argv[2])
text = src.read_text(encoding="utf-8")
start = text.find("/*******************************************************************************")
end = text.find(" ******************************************************************************/", start)
if start == -1 or end == -1:
    raise SystemExit("unexpected lv_font_conv header format")
end += len(" ******************************************************************************/")
header = "/*******************************************************************************\n"
header += " * Size: 12 px\n"
header += " * Bpp: 4\n"
header += " * Source: generated with scripts/generate-lvgl-icon-font.sh from Montserrat Regular + Symbols Nerd Font Mono.\n"
header += " * Ranges: ASCII 0x20-0x7E plus Nerd Font PUA icons 0xF013,0xF015,0xF233,0xF06F8,0xF07D0.\n"
header += " ******************************************************************************/"
dst.write_text(header + text[end:], encoding="utf-8")
PY

printf 'Generated %s\n' "$OUT"
wc -c "$OUT"
