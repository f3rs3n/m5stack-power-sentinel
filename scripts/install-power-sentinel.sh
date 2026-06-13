#!/usr/bin/env bash
set -euo pipefail

MODULES="nut"
DRY_RUN=0
PREFIX="/usr/local"
ETC_DIR="/etc"
SYSTEMD_DIR="/etc/systemd/system"

usage() {
  cat <<'USAGE'
Usage: sudo scripts/install-power-sentinel.sh [--modules nut[,proxmox,ha]] [--dry-run]

Installs or updates the clean modular Power Sentinel baseline.
Current implemented module: nut.
Declared future modules: proxmox, ha (accepted for manifest/config only; no telemetry installed yet).
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --modules)
      MODULES="${2:-}"; shift 2 ;;
    --modules=*)
      MODULES="${1#--modules=}"; shift ;;
    --dry-run)
      DRY_RUN=1; shift ;;
    -h|--help)
      usage; exit 0 ;;
    *)
      echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IFS=',' read -r -a requested <<< "$MODULES"

have_module() {
  local needle="$1"
  for m in "${requested[@]}"; do
    [[ "${m// /}" == "$needle" ]] && return 0
  done
  return 1
}

run() {
  if [[ "$DRY_RUN" == 1 ]]; then
    printf 'DRY-RUN '
    printf '%q ' "$@"
    printf '\n'
  else
    "$@"
  fi
}

write_modules_config() {
  local tmp
  tmp="$(mktemp)"
  python3 - "$MODULES" > "$tmp" <<'PY'
import json, sys
mods = {"nut": False, "proxmox": False, "ha": False}
for item in sys.argv[1].split(','):
    item = item.strip().lower()
    if item:
        if item not in mods:
            raise SystemExit(f"unsupported module: {item}")
        mods[item] = True
print(json.dumps({"modules": mods, "nut": {"ups": "homelab_ups@localhost", "clients_file": "/etc/power-sentinel-nut-clients.json"}}, indent=2))
PY
  if [[ "$DRY_RUN" == 1 ]]; then
    echo "DRY-RUN install -m 600 generated module config $ETC_DIR/power-sentinel.json"
    cat "$tmp"
    rm -f "$tmp"
  else
    install -m 600 "$tmp" "$ETC_DIR/power-sentinel.json"
    rm -f "$tmp"
  fi
}

for m in "${requested[@]}"; do
  m="${m// /}"
  case "$m" in
    nut|proxmox|ha) ;;
    "") ;;
    *) echo "Unsupported module: $m" >&2; exit 2 ;;
  esac
done

if ! have_module nut; then
  echo "Warning: installing without nut leaves only placeholder modules; no CoreS3 telemetry will be produced." >&2
fi
if have_module proxmox; then
  echo "Note: proxmox module uses the stdlib API adapter in power-sentinel-api; configure read-only credentials in /etc/power-sentinel.json." >&2
fi
if have_module ha; then
  echo "Note: ha module is a placeholder in this clean baseline; installer records it but installs no HA backend yet." >&2
fi

run install -d -m 755 "$PREFIX/bin" "$SYSTEMD_DIR"
run install -m 755 "$repo_root/backend/bin/power-sentinel-api.py" "$PREFIX/bin/power-sentinel-api"
run install -m 755 "$repo_root/backend/bin/power-sentinel-stackflow-unit.py" "$PREFIX/bin/power-sentinel-stackflow-unit"
if have_module nut; then
  run install -m 755 "$repo_root/backend/bin/m5stack-ups-detect.py" "$PREFIX/bin/m5stack-ups-detect"
fi
run install -m 644 "$repo_root/backend/systemd/power-sentinel-api.service" "$SYSTEMD_DIR/power-sentinel-api.service"
run install -m 644 "$repo_root/backend/systemd/power-sentinel-stackflow-unit.service" "$SYSTEMD_DIR/power-sentinel-stackflow-unit.service"
write_modules_config

if [[ "$DRY_RUN" == 0 ]]; then
  systemctl daemon-reload
  systemctl enable --now power-sentinel-api.service
  systemctl enable --now power-sentinel-stackflow-unit.service
  systemctl --no-pager --full status power-sentinel-api.service power-sentinel-stackflow-unit.service || true
  curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' >/dev/null
  echo "Power Sentinel installed/updated for modules: $MODULES"
else
  echo "Dry-run complete for modules: $MODULES"
fi
