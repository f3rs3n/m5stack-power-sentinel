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
5. Se necessario entra in download mode: tieni premuto reset circa 2 secondi finché il LED interno verde si accende, poi rilascia.
6. Esegui `Upload` da PlatformIO.

Upload futuri possibili: OTA dopo una prima flash dedicata, oppure passthrough USB Proxmox -> LXC se si vuole caricare da Hermes. Questa skeleton non abilita ancora OTA.

## Caveat tecnici

- LVGL è scelto per esplorazione visuale; se RAM/refresh/touch risultano troppo pesanti su hardware reale, si può mantenere lo stesso modello dati e rifare il rendering con M5GFX manuale.
- Touch/display bridge è volutamente minimale.
- I dati live sono parsati con ArduinoJson dal contratto `power-sentinel.summary.v1`; campi mancanti usano fallback semplici.
- La UI è una base reviewable, non una dashboard finale rifinita.
