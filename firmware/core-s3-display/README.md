# CoreS3 display firmware

Prima skeleton del frontend fisico per M5Stack Power Sentinel.

Stack previsto:

- PlatformIO
- Arduino framework
- board `m5stack-cores3`
- M5Unified / M5GFX
- LVGL
- ArduinoJson
- WiFi / HTTPClient

Endpoint backend:

```text
GET http://192.168.2.202:8088/api/v1/summary
```

Il display consuma l'endpoint locale del backend Power Sentinel / M5Stack LLM Module; Home Assistant non è la fonte primaria.

## UI inclusa

La skeleton crea una UI LVGL a tab statici, con sample data aderente a `power-sentinel.summary.v1`:

1. `UPS` - stato UPS, batteria, runtime, carico, tensioni.
2. `HA` - raggiungibilità Home Assistant API e MQTT.
3. `PVE` - raggiungibilità Proxmox e shutdown state.
4. `M5` - stato M5Stack/System, temperatura, RAM, disco, OpenAI, StackFlow, chat smoke.
5. `Offline` - fallback demo/offline, schema, timestamp, problemi e stato backend.

All'avvio usa sempre un payload demo locale. Se WiFi è configurato, prova anche a leggere l'endpoint reale e aggiorna le card ogni `SUMMARY_POLL_MS`.

## Configurazione locale, senza segreti nel repo

Non modificare `power_sentinel_config.example.h` con credenziali reali.

```bash
cd firmware/core-s3-display
cp include/power_sentinel_config.example.h include/power_sentinel_config.h
```

Poi modifica solo `include/power_sentinel_config.h` localmente:

```cpp
#define WIFI_SSID "nome-rete-locale"
#define WIFI_PASSWORD "password-locale"
#define POWER_SENTINEL_SUMMARY_URL "http://192.168.2.202:8088/api/v1/summary"
#define POWER_SENTINEL_STACK_POWER_OUT 0
```

`POWER_SENTINEL_STACK_POWER_OUT` controlla la direzione della 5V dello stack:

- `0`: modalità appliance/sentinel consigliata. Il CoreS3 può essere alimentato da LLM Mate/LLM Module/base, ma non alimenta il resto dello stack.
- `1`: modalità CoreS3 sorgente. La USB-C del CoreS3 può alimentare il bus/stack, ma questa modalità può impedire al CoreS3 di avviarsi quando lo stack lo alimenta dall'altro lato.

Il firmware demo M5Stack sembra gestire entrambi i versi dinamicamente. Con M5Unified 0.1.x il flag esposto è invece un output-enable esplicito sul bus, quindi per ora scegliamo la modalità sicura per l'appliance finale.
```

`include/power_sentinel_config.h` è ignorato da git.

Per provare solo la UI statica, lascia `WIFI_SSID` e `WIFI_PASSWORD` vuoti: il firmware resta in modalità sample/offline e non contiene segreti.

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

Se PlatformIO non è installato su questa macchina, la struttura rimane comunque pronta per PlatformIO: `platformio.ini`, `include/`, `src/`.

## Upload / flash

Non è stato eseguito alcun upload hardware da questa skeleton.

Primo flash consigliato: Windows + VS Code + PlatformIO.

1. Clona/pulla il repo su Windows.
2. Apri `firmware/core-s3-display` in VS Code.
3. Copia `include/power_sentinel_config.example.h` in `include/power_sentinel_config.h` e configura eventualmente il WiFi locale.
4. Collega il CoreS3 via USB.
5. Se PlatformIO vede due porte COM mentre lo stack è assemblato, una può essere il CH340/seriale dell'LLM Mate/Module. Per il primo flash è normale e più semplice separare il CoreS3 dallo stack, oppure scegliere esplicitamente la porta COM che appare/scompare collegando solo il CoreS3.
6. Se necessario entra in download mode: tieni premuto reset circa 2 secondi finché il LED interno verde si accende, poi rilascia.
7. Esegui `Upload` da PlatformIO.

Upload futuri possibili: OTA dopo una prima flash dedicata, oppure passthrough USB Proxmox -> LXC se si vuole caricare da Hermes. Questa skeleton non abilita ancora OTA.

## PlatformIO Inspect

`Inspect Project` può segnalare un falso positivo dentro ArduinoJson, ad esempio:

```text
CWE-0: failed to expand 'ARDUINOJSON_BEGIN_PUBLIC_NAMESPACE'
Invalid ## usage when expanding 'ARDUINOJSON_CONCAT_'
```

Questo viene dal parser/static analyzer usato da PlatformIO/cppcheck sulle macro interne di ArduinoJson, non da un difetto del firmware Power Sentinel. Se `pio run` compila e il firmware gira sul CoreS3, il warning può essere ignorato per questa skeleton.

## Caveat tecnici

- LVGL è scelto per esplorazione visuale; se RAM/refresh/touch risultano troppo pesanti su hardware reale, si può mantenere lo stesso modello dati e rifare il rendering con M5GFX manuale.
- Quando il CoreS3 è alimentato dallo stack/base/LLM Mate, il firmware imposta `cfg.output_power = false` prima di `M5.begin(cfg)`, come raccomandato dai docs CoreS3 per alimentazione esterna/Grove/DC; lasciare il default `true` può confliggere con la rail esterna.
- Touch/display bridge è volutamente minimale.
- I dati live sono parsati con ArduinoJson dal contratto `power-sentinel.summary.v1`; campi mancanti usano fallback semplici.
- La UI è una base reviewable, non una dashboard finale rifinita.
