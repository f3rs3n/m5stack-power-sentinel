# M5Stack Power Sentinel

Power Sentinel è un homelab companion locale. La baseline corrente è NUT Monitor, con UI CoreS3 Ledcards e backend locale dedicato a UPS/NUT, ma il prodotto non è limitato al solo UPS.

Gli sviluppi precedenti multi-dashboard non sono più parte della baseline corrente. Restano recuperabili dalla storia Git; il repo di lavoro contiene il nucleo da cui riaggiungere moduli indipendenti in modo controllato.

## Direzione prodotto

- Baseline implementata: pagina `NUT` con la nuova UI Ledcards fullscreen.
- Moduli: `nut` -> pagina `NUT` implementata; `proxmox` -> pagina `PROXMOX` con primo adapter API read-only; `ha` -> pagina `HA` ancora placeholder.
- Ogni modulo deve avere backend, contratto, test e UI propri, installabili/aggiornabili separatamente.
- Power Sentinel privilegia integrazioni leggere e handoff contestuali verso gli strumenti autorevoli; non deve diventare una console sostitutiva per sistemi specialistici.
- NUT resta read-only lato Power Sentinel: lo shutdown reale è Standard NUT/`upsmon`; nessun controllo custom Proxmox/API per spegnimenti.
- Regola critica: non spegnere se la linea è presente. Solo `OB LB` è intenzione di shutdown; `OL ... LB` è warning/no-shutdown.

## Layout corrente

```text
backend/
  bin/
    power-sentinel-api.py             API locale modulare, NUT implementato
    power-sentinel-stackflow-unit.py  bridge StackFlow -> API locale
    m5stack-ups-detect.py             diagnostica NUT/USB read-only
  config/                             template pubblicabili, niente segreti
  systemd/                            unit per API e StackFlow unit
  tests/                              test stdlib-only
firmware/core-s3-display/
  src/main.cpp                        firmware clean NUT Monitor
  src/ledcards-interface-page.*       nuova UI NUT Monitor
assets/lvgl-spike/
  power-sentinel-nut-ledcards-interface-fixture.c
  results/nut-ledcards-interface-*.png/json
scripts/install-power-sentinel.sh     install/update modulare
```

## Contratto API

Il contratto API v1 è documentato in `docs/architecture/api-contract-v1.md`; il contratto specifico del primo adapter CoreS3 Ambient Console è `docs/architecture/nut-ambient-console-contract.md`.

Endpoint principali:

- `GET /api/v1/summary`
- `GET /api/v1/summary?stackflow_safe=1`
- `GET /api/v1/health`

Il contratto conserva `schema = power-sentinel.summary.v1`, ma ora espone chiaramente:

- `profile: nut-monitor-clean-baseline`
- `available_modules: ["nut", "proxmox", "ha"]`
- `enabled_modules`, controllati da config o installer
- `modules.nut`, implementato
- `modules.proxmox`, primo adapter API read-only senza controlli remoti
- `modules.ha`, placeholder fino alla reintroduzione
- alias compatibili `ups` e `nut` per il firmware NUT Monitor

Default: solo `nut` è abilitato.

## Config moduli

Template: `backend/config/power-sentinel.example.json`.

Esempio:

```json
{
  "modules": {
    "nut": true,
    "proxmox": false,
    "ha": false
  },
  "nut": {
    "ups": "homelab_ups@localhost"
  }
}
```

Override rapido:

```bash
POWER_SENTINEL_MODULES=nut,proxmox ./backend/bin/power-sentinel-api.py --summary
```

Nota: abilitare `proxmox` con il token esempio `CHANGE_ME` non tenta chiamate live e riporta `status: not_observed`. Con credenziali reali read-only, il modulo legge solo API Proxmox leggere. `ha` resta placeholder.

## Installer modulare

Dry-run:

```bash
sudo scripts/install-power-sentinel.sh --modules nut --dry-run
```

Install/update API + StackFlow + NUT baseline:

```bash
sudo scripts/install-power-sentinel.sh --modules nut
```

Preparazione moduli aggiuntivi:

```bash
sudo scripts/install-power-sentinel.sh --modules nut,proxmox,ha --dry-run
```

Lo script accetta `--modules` per installare/aggiornare solo i moduli necessari. Oggi installa artefatti comuni e NUT; Proxmox è configurabile come adapter API read-only, mentre HA resta dichiarato ma non implementato.

## Verifica locale

```bash
python3 tools/run_tests.py
python3 backend/bin/power-sentinel-api.py --summary
```

Build firmware:

```bash
cd firmware/core-s3-display
pio run -e m5stack-cores3-ledcards-interface
```

## Flash workflow

Il workflow Windows/VSCode rimane quello preferito: il repo è buildabile senza segreti. La config privata opzionale resta in `firmware/core-s3-display/include/power_sentinel_config.h` ed è ignorata da Git; il template committed è `power_sentinel_config.example.h`.

Default firmware:

- StackFlow/UART interno su CoreS3 RX=G18 TX=G17, `Serial2`, 115200.
- Nessun polling HTTP dal CoreS3 nella baseline corrente.
- UI unica: NUT Monitor Ledcards.
- Sleep display con long press; loop/telemetria restano attivi.
