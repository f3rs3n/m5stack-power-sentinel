# M5Stack Power Sentinel

Power Sentinel è un homelab companion locale. La baseline corrente include NUT Monitor e Proxmox come moduli Ambient Console indipendenti, con UI CoreS3 Ledcards e backend locale modulare.

Gli sviluppi precedenti multi-dashboard non sono più parte della baseline corrente. Restano recuperabili dalla storia Git; il repo di lavoro contiene il nucleo da cui riaggiungere moduli indipendenti in modo controllato.

## Direzione prodotto

- Baseline implementata: pagine `NUT` e `PROXMOX` con UI Ledcards fullscreen, top bar stabile e touch focus presentation-only.
- Moduli runtime implementati: `nut` -> pagina `NUT`; `proxmox` -> pagina `PROXMOX` API-only/read-only con cinque Ambient Cards. Home Assistant resta un Module Candidate, non un placeholder runtime.
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
  src/main.cpp                        firmware live StackFlow/UART per NUT + Proxmox
  src/ledcards-interface-page.*       renderer Ambient Console Ledcards
  src/ledcards-graphics.h             helper condivisi per card/colori/ring slot
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
- `available_modules: ["nut", "proxmox"]`
- `enabled_modules`, controllati dalla Module Configuration locale o dall'override ambiente
- `modules.nut`, implementato
- `modules.proxmox`, primo adapter API read-only senza controlli remoti
- alias compatibili `ups` e `nut` per il firmware NUT Monitor

Default: solo `nut` è abilitato.

## Config moduli

Template: `backend/config/power-sentinel.example.json`.

Esempio:

```json
{
  "nut": {
    "enabled": true,
    "ups": "homelab_ups@localhost"
  },
  "proxmox": {
    "enabled": false,
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "CHANGE_ME"
  }
}
```

Override rapido:

```bash
POWER_SENTINEL_MODULES=nut,proxmox ./backend/bin/power-sentinel-api.py --summary
```

Nota: abilitare `proxmox` con il token esempio `CHANGE_ME` non tenta chiamate live e riporta `status: not_observed`. Con credenziali reali read-only, il modulo legge solo API Proxmox leggere.

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
sudo scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run
```

Lo script accetta `--modules` per installare/aggiornare solo i moduli necessari. Oggi installa artefatti comuni e NUT; Proxmox è configurabile come adapter API read-only. Moduli futuri vanno aggiunti quando sono capability reali implementate.

## Verifica locale

```bash
python3 tools/run_tests.py
python3 tools/check_core_s3_ui.py
python3 backend/bin/power-sentinel-api.py --summary
```

Build firmware:

```bash
cd firmware/core-s3-display
pio run -e m5stack-cores3
```

## Flash workflow

Il workflow Windows/VSCode rimane quello preferito: il repo è buildabile senza segreti. La config privata opzionale resta in `firmware/core-s3-display/include/power_sentinel_config.h` ed è ignorata da Git; il template committed è `power_sentinel_config.example.h`.

Default firmware:

- StackFlow/UART interno su CoreS3 RX=G18 TX=G17, `Serial2`, 115200.
- Diagnostica seriale StackFlow per timeout, JSON parse, errori StackFlow e risposte stale, conservando il timing dell'ultimo payload valido.
- Nessun polling HTTP dal CoreS3 nella baseline corrente.
- UI live: NUT Monitor Ledcards + pagina Proxmox quando il backend la espone come modulo implementato; le card usano un helper condiviso per testi owned, visual class, colori e ring slot fisici.
- Motion live: transizioni tra pagine Ambient Console e promozione card NUT/Proxmox lungo il ring fisico bidirezionale con ghost non cliccabili.
- Sleep display con long press; loop/telemetria restano attivi.

Firmware di sviluppo/fixture:

- `m5stack-cores3` è il firmware live: interroga StackFlow/UART e richiede `power-sentinel-api.service` + `power-sentinel-stackflow-unit.service` attivi sul Module LLM.
- `m5stack-cores3-ledcards-interface` definisce `POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY=1` e carica una fixture visuale interna; usarlo solo per test statici di layout, non per validazione live.

Flash live su host Linux con CoreS3 visibile come `/dev/ttyACM0`:

```bash
cd firmware/core-s3-display
/home/martino/.platformio/penv/bin/pio run -e m5stack-cores3 -t upload --upload-port /dev/ttyACM0
```

Validazione live lato Module LLM:

```bash
curl -fsS 'http://192.168.2.202:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
ssh root@192.168.2.202 'systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service'
```

## Live-test serial capture

In sessioni non interattive `pio device monitor` può fallire perché richiede un TTY. Per catture bounded dal CoreS3 usare il helper stdlib+pyserial:

```bash
/home/martino/.platformio/penv/bin/python tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

Il helper non resetta il CoreS3: è adatto a controllare uno stato già stabile senza contaminare la cattura con boot/mode ramp. Per salvare un log:

```bash
/home/martino/.platformio/penv/bin/python tools/core_s3_serial_capture.py --output /tmp/power-sentinel-cores3-serial.log --duration 30
```
