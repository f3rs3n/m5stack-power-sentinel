# CoreS3 display firmware

Frontend fisico per M5Stack Power Sentinel.

Stack:

- PlatformIO
- Arduino framework
- board `m5stack-cores3`
- M5Unified / M5GFX
- LVGL
- ArduinoJson
- WiFi / HTTPClient solo come fallback di sviluppo

Endpoint backend HTTP, usato direttamente solo dal fallback WiFi:

```text
GET http://192.168.2.202:8088/api/v1/summary
```

Nel setup impilato finale il display consuma i dati via UART interna + StackFlow/`llm_sys`, non via Home Assistant e non tramite un bridge parallelo su `/dev/ttyS1`.

## Trasporto dati

Percorso primario verificato:

```text
CoreS3 UART G18/G17 @ 115200
-> llm_sys
-> ipc:///tmp/rpc.sentinel
-> power-sentinel-stackflow-unit
-> http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1
-> risposta StackFlow object=power-sentinel.summary.v1
```

Il vecchio bridge seriale PS1 è stato rimosso. La scelta StackFlow evita contesa con `llm_sys`, che è il legittimo proprietario di `/dev/ttyS1`, e mantiene compatibili le funzioni vendor future.

## UI inclusa

La UI LVGL V1 segue la specifica `docs/architecture/core-s3-pages-v1.md` e ha cinque tab:

1. `HOME` - overview da scrivania con stato globale, UPS essentials, NUT/PVE/HA/NET/M5S e problemi principali.
2. `NUT` - UPS essentials più card orizzontali per NUT server/client, readiness Standard NUT e dettagli UPS.
3. `PVE` - integrazione Proxmox read-only: CPU/RAM/storage, ZFS, SMART e mini-card per VM/LXC running con barre CPU/RAM/HDD quando le metriche sono disponibili. La latenza API PVE non è più nella card PVE compatta: vive nella diagnostica `M5S`.
4. `HA` - Home Assistant API, MQTT, aggiornamenti disponibili, Zigbee2MQTT e coordinatore; mostra `Z2M devices: interviewed/total` e non spreca spazio con versione Z2M o birth topic HA.
5. `M5S` - stato M5Stack/System più diagnostica trasporto, firmware, schema, UART e contatori poll.

La direzione del prodotto non è più “solo UPS display”: lo stack è un server Linux autonomo multi-funzione con dashboard informative estensibili. Il CoreS3 è il front-panel pratico e moderno per UPS/NUT, Proxmox, Home Assistant/Zigbee2MQTT, rete e M5Stack; futuri mini-dashboard o una companion tab LLM devono agganciarsi allo stesso contratto dati senza rompere le pagine esistenti.

V1b/V1c/V1d più il polish PVE corrente usano un layout moderno embedded-safe senza asset pesanti: tema scuro coerente, sidebar sinistra icon-only con glyph 18 px e rail fissa da 44 px, carousel orizzontale di card per ogni tab, hero card HOME, font hierarchy Montserrat, card con radius/shadow sottile, metric row strutturate, status pill compatte e barre percentuali più consistenti. La HOME mostra il badge severity in maiuscolo (`OK`/`WARN`/`CRITICAL`) e il campo `NET` deriva dal probe `network` del backend sul Linux del modulo LLM, non da Proxmox.

Dettagli PVE attuali: la card principale usa header `PROXMOX` con pill `ONLINE`/`OFFLINE` a destra; mostra CPU/RAM/storage con totale RAM e Total Node Capacity allineati a destra, health pill `ZFS online` / `SMART ok`, pill per interfacce Proxmox attive non-loopback (`active_network_interfaces[]`) e pill `NUT armed` / `NUT disarmed` riferita solo al client NUT del nodo Proxmox selezionato (`shutdown.proxmox_nut_client.armed`). Le mini-card workload mostrano CPU/RAM/HDD con totale RAM/HDD a destra. Per le VM il backend corregge i dati Proxmox grezzi: RAM da `/status/current`, HDD da QEMU guest-agent fsinfo quando disponibile, ignorando valori `disk=0` non affidabili e filesystem HAOS read-only `erofs`.

Non c'è più un payload demo/sample plausibile all'avvio. Finché non arriva il primo summary live, lo stato è esplicitamente `boot` / `offline` / `waiting`.

## Configurazione locale, senza segreti nel repo

Non modificare `power_sentinel_config.example.h` con credenziali reali.

Il file locale deve stare esattamente in:

```text
firmware/core-s3-display/include/power_sentinel_config.h
```

Non metterlo nella root di `firmware/core-s3-display/`: PlatformIO compila con `-I include`, quindi il firmware cerca il config dentro `include/`. Se hai già creato per errore `firmware/core-s3-display/power_sentinel_config.h`, spostalo in `include/` o copiane il contenuto lì.

```bash
cd firmware/core-s3-display
cp include/power_sentinel_config.example.h include/power_sentinel_config.h
```

Poi modifica solo `include/power_sentinel_config.h` localmente se serve:

```cpp
#define WIFI_SSID "nome-rete-locale"       // opzionale, solo fallback HTTP
#define WIFI_PASSWORD "password-locale"    // opzionale, solo fallback HTTP
#define POWER_SENTINEL_SUMMARY_URL "http://192.168.2.202:8088/api/v1/summary"
#define POWER_SENTINEL_STACK_POWER_OUT 0
```

`POWER_SENTINEL_STACK_POWER_OUT` controlla la direzione della 5V dello stack:

- `0`: modalità appliance/sentinel consigliata. Il CoreS3 può essere alimentato da LLM Mate/LLM Module/base, ma non alimenta il resto dello stack.
- `1`: modalità CoreS3 sorgente. La USB-C del CoreS3 può alimentare il bus/stack, ma questa modalità può impedire al CoreS3 di avviarsi quando lo stack lo alimenta dall'altro lato.

Il firmware demo M5Stack sembra gestire entrambi i versi dinamicamente. Con M5Unified il flag esposto è invece un output-enable esplicito sul bus, quindi per ora scegliamo la modalità sicura per l'appliance finale.

`include/power_sentinel_config.h` è ignorato da git.

## Build

Con PlatformIO installato:

```bash
cd firmware/core-s3-display
pio run
```

Oppure da VS Code:

1. apri la cartella `firmware/core-s3-display`;
2. installa/abilita l'estensione PlatformIO;
3. seleziona l'ambiente `m5stack-cores3`;
4. esegui `Build`.

## Upload / flash

Primo flash consigliato: Windows + VS Code + PlatformIO.

1. Clona/pulla il repo su Windows.
2. Apri `firmware/core-s3-display` in VS Code.
3. Copia `include/power_sentinel_config.example.h` in `include/power_sentinel_config.h` se vuoi override locali.
4. Collega il CoreS3 via USB.
5. Se PlatformIO vede due porte COM mentre lo stack è assemblato, una può essere il CH340/seriale dell'LLM Mate/Module. Scegli la porta COM che appare/scompare collegando solo il CoreS3.
6. Se necessario entra in download mode: tieni premuto reset circa 2 secondi finché il LED interno verde si accende, poi rilascia.
7. Esegui `Upload` da PlatformIO.
8. Monitora a 115200 baud.

Log atteso:

```text
Power Sentinel firmware build: stackflow-2026-05-23-pve-polish
Serial transport enabled: RX=18 TX=17 baud=115200 mode=stackflow
Serial TX StackFlow sentinel.summary id=ps-N attempt 1/3
StackFlow summary OK: <bytes> bytes
```

## PlatformIO Inspect

`Inspect Project` può segnalare un falso positivo dentro ArduinoJson, ad esempio:

```text
CWE-0: failed to expand 'ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE'
Invalid ## usage when expanding 'ARDUINOJSON_CONCAT_'
```

Questo viene dal parser/static analyzer usato da PlatformIO/cppcheck sulle macro interne di ArduinoJson, non da un difetto del firmware Power Sentinel. Se `pio run` compila e il firmware gira sul CoreS3, il warning può essere ignorato.

## Caveat tecnici

- LVGL è scelto per esplorazione visuale; se RAM/refresh/touch risultano troppo pesanti su hardware reale, si può mantenere lo stesso modello dati e rifare il rendering con M5GFX manuale.
- Quando il CoreS3 è alimentato dallo stack/base/LLM Mate, il firmware imposta `cfg.output_power = false` prima di `M5.begin(cfg)`, come raccomandato dai docs CoreS3 per alimentazione esterna/Grove/DC; lasciare il default `true` può confliggere con la rail esterna.
- Touch/display bridge è volutamente minimale.
- I dati live sono parsati con ArduinoJson dal contratto `power-sentinel.summary.v1`; campi mancanti usano fallback espliciti `unknown`/`unavailable`, non valori realistici inventati.
- La UI è una base reviewable, non una dashboard finale rifinita.
