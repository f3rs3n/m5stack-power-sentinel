# M5Stack CoreS3 + LLM Kit Homelab Power Sentinel Plan

Data: 2026-05-15
Device principale: M5Stack CoreS3 + LLM Kit / LLM Module + LLM Mate
IP modulo LLM: 192.168.2.202
Home Assistant: 192.168.2.200 / homeassistant.local

Goal: trasformare lo stack M5Stack in una piccola appliance autonoma per homelab: NUT server primario, display fisico CoreS3, sensori MQTT/Home Assistant, healthcheck del modulo LLM, e futuro satellite Hermes/voice.

Architettura: il modulo LLM deve essere trattato come appliance Linux a basso consumo e possibilmente come ultimo nodo vivo durante blackout. Home Assistant e Proxmox devono consumare lo stato pubblicato dal M5Stack, non essere prerequisiti per leggere lo stato UPS. Il CoreS3 deve mostrare almeno una pagina UPS autonoma anche se Home Assistant o Proxmox non sono disponibili.

Principio chiave: UPS/NUT locale al M5Stack deve diventare la fonte primaria dello stato alimentazione; MQTT/Home Assistant sono esposizione e automazione, non la fonte di verità.

---

## 1. Stato già completato

### 1.1 Discovery e baseline hardware/software

Completato:
- Accesso SSH funzionante verso il modulo LLM.
- Confermato sistema Linux reale basato su Ubuntu 22.04 LTS.
- Confermati servizi StackFlow/vendor sotto systemd.
- Confermata API StackFlow nativa su porta 10001.
- Confermata OpenAI-compatible API su porta 8000 dopo installazione/abilitazione.
- Confermato LED Linux del modulo: verde (`R=0 G=50 B=0`).
- Chiarito che LED rosso sul LLM Mate e' spia power-on, non errore.

Servizi LLM rilevanti attivi:
- llm-sys
- llm-llm
- llm-openai-api
- llm-asr
- llm-kws
- llm-tts
- llm-melotts
- llm-vlm
- llm-yolo

Porte rilevanti:
- 22: SSH
- 8000: OpenAI-compatible API
- 10001: StackFlow native API

### 1.2 Hardening leggero gia' fatto

Completato in precedenza, da non dimenticare:
- Aggiunta autenticazione SSH key per root.
- Verificato login via chiave in sessione separata prima di chiudere accesso legacy.
- Cambiata password root dal default vendor.
- SSH root reso key-only / password disabilitata secondo sequenza lockout-safe.
- Telnet/inetd disabilitato dopo verifica login SSH via chiave.
- rpcbind/rpcbind.socket disabilitati se non necessari.
- Verificato che le porte legacy 23/telnet e 111/rpcbind non siano piu' esposte.

Nota operativa: non ripetere hardening distruttivo senza preflight. Se si deve ricontrollare:

```bash
sshd -T | grep -Ei '^(permitrootlogin|passwordauthentication|pubkeyauthentication|kbdinteractiveauthentication)'
ss -tulpn | grep -E ':(22|23|111|10001|8000)\b' || true
systemctl is-active inetd.service rpcbind.service rpcbind.socket 2>/dev/null || true
```

Stato desiderato:
- SSH 22 aperto su LAN fidata.
- StackFlow 10001 aperto solo su LAN fidata.
- OpenAI API 8000 aperta solo su LAN fidata.
- Telnet 23 non attivo.
- rpcbind 111 non attivo.

### 1.3 Stabilizzazione runtime LLM

Completato:
- Il crash `llm_llm-1.12` con `std::invalid_argument what(): stoi` e' stato risolto con reboot completo dopo refresh/installazione pacchetti.
- Confermato che i probe troppo corti tipo "OK" possono dare falsi negativi o content vuoto.
- Deciso di usare smoke test realistici/doc-style.
- OpenAI-compatible API funzionante con payload chat realistici.

Modelli mantenuti/installati:
- `qwen2.5-0.5B-prefill-20e`: baseline stabile.
- `qwen2.5-HA-0.5B-ctx-ax630c`: modello piu' adatto a Home Assistant / homelab.
- `silero-vad`: VAD per futura pipeline voce.

Modelli debug rimossi:
- `llm-model-qwen2.5-0.5b-int4-ax630c`
- `llm-model-qwen2.5-0.5b-p256-ax630c`

### 1.4 Healthcheck locale del modulo LLM

Creato:

```text
/usr/local/bin/m5stack-healthcheck
```

Funzioni:
- verifica servizi `llm-*` principali;
- verifica porte locali StackFlow 10001 e OpenAI 8000;
- interroga StackFlow `sys.hwinfo` e `sys.lsmode`;
- verifica presenza modelli richiesti;
- legge RAM, disco, LED RGB;
- supporta output human-readable;
- supporta output JSON;
- supporta chat smoke opzionale con `--chat`;
- exit code 0 solo se `overall_ok=true`.

Comandi:

```bash
/usr/local/bin/m5stack-healthcheck
/usr/local/bin/m5stack-healthcheck --json
/usr/local/bin/m5stack-healthcheck --chat
```

### 1.5 MQTT/Home Assistant per stato modulo LLM

Creato publisher:

```text
/usr/local/bin/m5stack-ha-publish
/etc/m5stack-ha-publish.json
```

Config:
- `/etc/m5stack-ha-publish.json` e' root-only `0600`.
- Contiene broker MQTT, prefix, username/password dedicati.
- Non inserire credenziali in piani, log o risposte.

Systemd:

```text
/etc/systemd/system/m5stack-ha-publish.service
/etc/systemd/system/m5stack-ha-publish.timer
/etc/systemd/system/m5stack-ha-publish-chat.service
/etc/systemd/system/m5stack-ha-publish-chat.timer
```

Timer:
- healthcheck leggero ogni 60 secondi;
- chat smoke ogni 10 minuti.

Topic MQTT:

```text
m5stack/llm/state
m5stack/llm/chat_state
m5stack/llm/availability
```

Home Assistant MQTT Discovery pubblica un device tipo:

```text
M5Stack LLM Module
```

Entita' attuali:
- Overall OK
- Health Status
- Problem Count
- Last Update
- OpenAI API
- StackFlow API
- Temperature
- StackFlow Memory
- RAM Available
- Disk Free
- LED Color
- Chat Smoke
- Chat Smoke OK

Decisioni gia' prese:
- LED esposto come singola entita' colore, non tre sensori R/G/B.
- Campi raw LED R/G/B restano nel JSON per diagnostica.
- Chat Smoke e' su topic separato per non essere sovrascritto dal publish leggero.

Campi derivati nel payload:

```text
summary.status        # ok/fail
summary.problem_count # numero problemi
summary.problems      # lista problemi
summary.led_color     # green/red/blue/off/unknown
```

Verificato:
- MQTT auth funzionante.
- Discovery accettata da Home Assistant.
- Timer attivi.
- State retained presente.
- Chat state retained presente.

---

## 2. Architettura target aggiornata

### 2.1 Ruolo principale del M5Stack

Nuovo ruolo prioritario:

```text
M5Stack = Power Sentinel autonomo + display fisico + satellite Home Assistant/Hermes
```

Non solo satellite HA. Deve poter restare utile quando Home Assistant o Proxmox sono spenti.

### 2.2 Flussi dati target

UPS fisico:

```text
UPS -> USB OTG -> M5Stack LLM Module -> NUT server locale
```

Stato UPS locale:

```text
NUT locale -> JSON locale -> CoreS3 display pagina UPS
```

Esposizione homelab:

```text
NUT locale -> MQTT -> Home Assistant
NUT locale -> NUT clients -> Proxmox/Home Assistant/altri host
```

Stato servizi:

```text
M5Stack healthcheck -> MQTT -> Home Assistant
Home Assistant/Proxmox checks -> MQTT/API -> CoreS3 display
```

Design importante:
- Home Assistant consuma e automatizza.
- Proxmox consuma per shutdown ordinato.
- CoreS3 deve mostrare pagina UPS anche senza HA.

---

## 3. Fase NUT preflight prima del cavo USB OTG

Obiettivo: preparare tutto il possibile senza collegare ancora l'UPS.

Status 2026-05-15: completata.

### Task 3.1: Verificare pacchetti NUT disponibili

Completato.

Pacchetti disponibili/installati da Ubuntu 22.04 arm64:
- `nut` 2.7.4-14ubuntu2
- `nut-server` 2.7.4-14ubuntu2
- `nut-client` 2.7.4-14ubuntu2

Nota su `nut-client`: serve anche sul server per avere gli strumenti client locali (`upsc`, `upsmon`, libreria client) con cui verificare il demone locale e leggere lo stato UPS da script. `nut-server` fornisce driver/upsd; `nut-client` fornisce i comandi per interrogare `upsd`. Per il nostro design, `m5stack-ups-publish` leggerà quasi certamente `upsc homelab_ups@localhost`, quindi `nut-client` è utile anche sul M5Stack server.

### Task 3.2: Dry-run installazione NUT

Completato.

Dry-run risultato ragionevole:
- 9 nuovi pacchetti;
- circa 2.5 MB download;
- circa 9.9 MB spazio aggiuntivo;
- nessuna dipendenza pesante inattesa.

Pacchetti aggiunti:
- `libltdl7`
- `libnspr4`
- `libnss3`
- `libnutscan1`
- `libupsclient4`
- `libusb-0.1-4`
- `nut`
- `nut-client`
- `nut-server`

### Task 3.3: Installare NUT tooling base

Completato.

Comandi installati:
- `/usr/bin/upsc`
- `/usr/sbin/upsdrvctl`
- `/usr/sbin/upsd`
- `/usr/sbin/upsmon`
- `/usr/bin/nut-scanner`

Poiche' non c'e' ancora UPS collegato/configurato, i servizi NUT sono stati lasciati installati ma disabilitati/inactive per evitare failed unit inutili:
- `nut-server`: disabled/inactive
- `nut-monitor`: disabled/inactive
- `nut-driver`: static/inactive

`/etc/nut/nut.conf` e' ancora `MODE=none`, come stato sicuro pre-cavo.

### Task 3.4: Preparare script detect read-only

Completato.

Creato:

```text
/usr/local/bin/m5stack-ups-detect
```

Funzioni:
- stampa kernel e timestamp;
- esegue `lsusb`;
- lista `/dev/bus/usb`;
- stampa ultimi messaggi `dmesg` USB/HID/power;
- esegue `nut-scanner -U`;
- mostra comandi NUT installati;
- mostra stato servizi NUT;
- mostra config NUT redigendo eventuali password;
- non modifica configurazione;
- non avvia driver.

Output pre-cavo:
- `lsusb` non mostra dispositivi oltre al controller;
- `nut-scanner -U` non trova UPS, come atteso senza cavo;
- kernel mostra USB host/gadget/type-C presenti.

### Task 3.5: Preparare nomi e convenzioni

Nome UPS NUT:

```text
homelab_ups
```

Endpoint target:

```text
homelab_ups@192.168.2.202
```

Descrizione:

```text
Homelab UPS
```

Topic MQTT futuri:

```text
m5stack/ups/state
m5stack/ups/availability
```

Device HA futuro:

```text
Homelab UPS Sentinel
```

---

## 4. Fase cavo USB OTG: discovery UPS reale

Quando: dopo arrivo cavo e collegamento UPS al modulo.

### Task 4.1: Collegare UPS e raccogliere discovery

Comandi:

```bash
ssh root@192.168.2.202 '/usr/local/bin/m5stack-ups-detect'
ssh root@192.168.2.202 'lsusb || true'
ssh root@192.168.2.202 'dmesg | grep -Ei "usb|hid|ups" | tail -n 80'
```

Salvare:
- vendor ID;
- product ID;
- modello UPS;
- eventuale seriale;
- device path.

### Task 4.2: Provare nut-scanner

```bash
ssh root@192.168.2.202 'nut-scanner -U 2>&1 | sed -n "1,200p"'
```

Output desiderato:
- se rileva `usbhid-ups`, usare quello.
- se non rileva, verificare permessi/driver/vendorid/productid.

### Task 4.3: Configurare prova minima NUT

File target:

```text
/etc/nut/nut.conf
/etc/nut/ups.conf
/etc/nut/upsd.conf
/etc/nut/upsd.users
```

Configurazione iniziale desiderata:

```text
# /etc/nut/nut.conf
MODE=netserver
```

```text
# /etc/nut/ups.conf
[homelab_ups]
    driver = usbhid-ups
    port = auto
    desc = "Homelab UPS"
```

Nota: aggiungere `vendorid`, `productid`, `pollinterval` solo se necessario dopo discovery.

### Task 4.4: Test locale driver e upsd

Comandi:

```bash
ssh root@192.168.2.202 'upsdrvctl -t start'
ssh root@192.168.2.202 'systemctl restart nut-server || systemctl restart nut.target || true'
ssh root@192.168.2.202 'upsc homelab_ups@localhost 2>&1 | sed -n "1,220p"'
```

Criterio di successo:
- `upsc` mostra almeno:
  - `ups.status`
  - `battery.charge` oppure equivalente
  - `ups.load` se supportato
  - `battery.runtime` se supportato

---

## 5. Fase NUT server stabile

Obiettivo: rendere il M5Stack il NUT server primario della LAN.

### Task 5.1: Utenti NUT

Creare utenti separati in `/etc/nut/upsd.users`:

- utente read-only monitor per Home Assistant / Proxmox;
- eventuale admin locale solo se serve.

Non inserire password nel piano. Generarle e salvarle root-only.

Esempio struttura:

```text
[monuser]
    password = SECRET
    upsmon slave
```

### Task 5.2: Bind sicuro upsd

In `/etc/nut/upsd.conf`:

```text
LISTEN 127.0.0.1 3493
LISTEN 192.168.2.202 3493
```

Non esporre fuori LAN fidata.

### Task 5.3: Abilitare servizi NUT

Comandi indicativi, da adattare ai nomi unit reali:

```bash
systemctl enable --now nut-server
systemctl status nut-server --no-pager
```

Verifica locale:

```bash
upsc homelab_ups@localhost
```

Verifica LAN:

```bash
upsc homelab_ups@192.168.2.202
```

### Task 5.4: Test reboot/reconnect

Test:
- riavvio servizi NUT;
- scollega/ricollega USB se sicuro;
- reboot modulo solo quando non interrompe lavoro;
- verificare che `upsc` torni operativo.

---

## 6. Fase UPS MQTT/Home Assistant

Obiettivo: pubblicare stato UPS in Home Assistant via MQTT Discovery, mantenendo NUT come fonte primaria.

### Task 6.1: Creare script collector/publisher UPS

Creare:

```text
/usr/local/bin/m5stack-ups-publish
/etc/m5stack-ups-publish.json
```

Input:
- `upsc homelab_ups@localhost`

Output MQTT:

```text
m5stack/ups/state
m5stack/ups/availability
```

Payload JSON normalizzato:

```json
{
  "timestamp": "...",
  "overall_ok": true,
  "status": "OL",
  "on_battery": false,
  "low_battery": false,
  "battery_charge": 100,
  "runtime_seconds": 3600,
  "load_percent": 20,
  "input_voltage": 230.0,
  "output_voltage": 230.0,
  "raw": {}
}
```

### Task 6.2: Home Assistant Discovery UPS

Device:

```text
Homelab UPS Sentinel
```

Entita' proposte:
- UPS Status
- On Battery
- Low Battery
- Battery Charge
- Runtime Remaining
- Load
- Input Voltage
- Output Voltage
- Last Update
- Problem Count

### Task 6.3: Timer systemd UPS

Creare:

```text
/etc/systemd/system/m5stack-ups-publish.service
/etc/systemd/system/m5stack-ups-publish.timer
```

Intervallo iniziale:
- 30 o 60 secondi.

Scelta consigliata:
- 30 secondi se il polling NUT e' leggero;
- 60 secondi se vogliamo minimizzare carico.

---

## 7. Fase client NUT: Home Assistant e Proxmox

Obiettivo: far consumare lo stato UPS agli altri nodi senza renderli fonte primaria.

### 7.1 Home Assistant

Opzioni:
- usare MQTT gia' pubblicato dal M5Stack;
- usare integrazione NUT nativa puntando a `192.168.2.202`.

Scelta suggerita:
- MQTT per dashboard custom e automazioni gia' nel nostro schema;
- NUT integration opzionale se vuoi entita' standard HA.

### 7.2 Proxmox

Proxmox deve essere inizialmente read-only / monitor.

Fasi:
1. installare `nut-client` su Proxmox se non presente;
2. verificare `upsc homelab_ups@192.168.2.202`;
3. configurare upsmon come client/slave;
4. testare senza shutdown reale;
5. abilitare shutdown solo dopo simulazione e soglie ragionate.

Regola di sicurezza:
- non abilitare shutdown reale Proxmox finche' non abbiamo validato status, runtime, low battery e comportamento su ritorno corrente.

---

## 8. Fase display CoreS3 autonomo

Obiettivo: CoreS3 come display fisico, non solo UI dipendente da Home Assistant.

### 8.1 Pagine display target

Pagina 1: UPS / Power
- status: ONLINE / ON BATTERY / LOW BATTERY
- battery %
- runtime stimato
- load %
- input voltage
- output voltage se disponibile
- ultimo aggiornamento
- eventuale countdown shutdown

Questa pagina deve funzionare anche se HA e Proxmox sono down.

Pagina 2: Home Assistant
- HA reachable
- MQTT broker reachable
- Zigbee2MQTT status
- eventuali entita' critiche
- ultimo update HA/MQTT

Pagina 3: Proxmox
- Proxmox reachable
- host status
- VM/container critici se leggibili via API
- storage o backup status in futuro
- stato shutdown pending durante blackout

Pagina 4: M5Stack/System
- modulo overall ok
- StackFlow API
- OpenAI API
- Chat Smoke OK
- temperatura modulo
- RAM
- disco
- LED color

### 8.2 Fonte dati display

Preferenza architetturale:

```text
LLM Module genera JSON sintetico locale -> CoreS3 legge e visualizza
```

Possibili trasporti:
- seriale tra CoreS3 e LLM Module, se pratico;
- HTTP locale servito dal modulo;
- MQTT, se CoreS3 puo' collegarsi direttamente;
- file/endpoint vendor se disponibile.

Principio:
- pagina UPS non deve richiedere HA.
- pagina HA puo' dipendere da HA.
- pagina Proxmox puo' dipendere da Proxmox.

### 8.3 Navigazione e UX

- Pulsanti CoreS3 per cambio pagina.
- Timeout ritorno automatico a pagina UPS.
- Colori:
  - verde: ok;
  - giallo: degradato;
  - rosso: critico;
  - blu/viola: on battery / power event.
- Layout ad alta leggibilita', pochi numeri essenziali.

---

## 9. Fase automazioni Home Assistant

Obiettivo: trasformare sensori in alert reali.

Automazioni minime:

1. M5Stack LLM publisher stale
- Se `Last Update` troppo vecchio (>3-5 minuti)
- Notifica: publisher/timer morto o modulo offline.

2. LLM Module Overall OK off
- Se Overall OK off >2 minuti
- Notifica critica.

3. OpenAI API down ma StackFlow up
- Notifica: wrapper OpenAI degradato.

4. StackFlow API down
- Notifica critica modulo/runtime.

5. Chat Smoke OK off per 2 cicli
- Notifica: LLM/tokenizer degradato.

6. UPS on battery
- Notifica immediata.

7. UPS low battery / runtime basso
- Notifica critica; shutdown reale demandato a Standard NUT (`upsmon`), non a orchestrazione Power Sentinel/Proxmox API.

8. Temperatura M5Stack alta
- Soglia iniziale: >70 C per 5 minuti.

9. Disco basso
- Soglia iniziale: <3 GB liberi.

---

## 10. Fase shutdown Standard NUT

Decisione aggiornata: lo shutdown reale sara' Standard NUT, non orchestrazione Power Sentinel/Proxmox API.

Obiettivo: spegnimento ordinato homelab durante blackout usando `upsmon`.

Ruoli desiderati:
1. M5Stack LLM Module: `usbhid-ups` + `upsd` + futuro `upsmon primary`.
2. Proxmox: futuro `upsmon secondary` che legge `homelab_ups@192.168.2.202` e spegne localmente il nodo.
3. Power Sentinel: dashboard/dry-run/observer; non invia comandi di shutdown via Proxmox API.

Sequenza desiderata:
1. UPS passa ON BATTERY.
2. M5Stack aggiorna NUT/MQTT/display.
3. Se NUT segnala LOWBATT/FSD, `upsmon` coordina lo shutdown standard:
   - i secondary, incluso Proxmox, si spengono localmente;
   - il primary M5Stack si spegne secondo la propria config;
   - killpower UPS resta fuori scope V1 salvo test esplicito futuro.
4. Al ritorno corrente:
   - UPS torna ONLINE;
   - Proxmox/HA ripartono secondo firmware/BIOS/policy host;
   - M5Stack riparte o resta operativo secondo alimentazione disponibile.

Regole:
- Prima read-only.
- Poi dry-run/log-only in Power Sentinel.
- Poi configurazione Standard NUT non armata/templates.
- Poi micro-test controllato.
- Shutdown reale solo dopo conferma esplicita.

---

## 11. Fasi future opzionali

### 11.1 Voice / Home Assistant Satellite

Dopo NUT/display:
- test KWS;
- test VAD;
- test ASR;
- test TTS;
- eventuale Home Assistant Assist;
- whitelist comandi;
- no azioni pericolose senza conferma.

### 11.2 Hermes satellite

Possibili funzioni:
- display stato Hermes;
- push-to-talk;
- notifiche agentiche;
- mini terminale fisico “Servoskull”.

### 11.3 Edge AI / Vision

Possibili usi:
- YOLO/VLM per demo o watchdog fisico;
- priorita' bassa rispetto a UPS/display.

---

## 12. Checklist immediata per domani quando arriva il cavo

1. Collegare UPS via USB OTG al M5Stack.
2. Eseguire:

```bash
ssh root@192.168.2.202 '/usr/local/bin/m5stack-ups-detect'
```

3. Se NUT non e' ancora installato:

```bash
ssh root@192.168.2.202 'apt-get install -s nut nut-server nut-client'
```

4. Installare se dry-run ok.
5. Eseguire:

```bash
ssh root@192.168.2.202 'nut-scanner -U 2>&1 | sed -n "1,200p"'
```

6. Configurare `homelab_ups` con driver corretto.
7. Verificare:

```bash
ssh root@192.168.2.202 'upsc homelab_ups@localhost'
```

8. Solo dopo: MQTT UPS, Home Assistant, Proxmox client.

---

## 13. Verifiche correnti da eseguire periodicamente

Modulo LLM:

```bash
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck'
ssh root@192.168.2.202 'systemctl list-timers "m5stack-ha-publish*" --no-pager'
ssh root@192.168.2.202 'journalctl -u m5stack-ha-publish.service -n 20 --no-pager'
ssh root@192.168.2.202 'journalctl -u m5stack-ha-publish-chat.service -n 20 --no-pager'
```

MQTT retained topics:

```text
m5stack/llm/state
m5stack/llm/chat_state
m5stack/llm/availability
```

Sicurezza:

```bash
ssh root@192.168.2.202 'ss -tulpn | grep -E ":(22|23|111|10001|8000|3493)\b" || true'
```

---

## 14. Decisioni aperte

1. Driver UPS effettivo: da scoprire dopo collegamento USB.
2. Intervallo MQTT UPS: 30s o 60s.
3. Modalita' display CoreS3: seriale, HTTP locale, MQTT diretto o altro.
4. Proxmox: quando configurare `upsmon secondary` Standard NUT e come testare shutdown locale.
5. Home Assistant: usare solo MQTT o anche integrazione NUT nativa.
6. Soglie UPS reali: runtime minimo, battery %, low battery policy.

---

## 15. Regole operative

- Non inserire segreti nei piani.
- Non abilitare shutdown reale senza conferma esplicita.
- Non trasformare subito il M5Stack in single point of failure senza test.
- NUT server sul M5Stack e' sensato; Proxmox shutdown reale deve passare da `upsmon secondary`, non da API Proxmox custom.
- Home Assistant deve essere consumatore/automatore, non prerequisito per sapere se l'UPS e' vivo.
- CoreS3 deve avere almeno pagina UPS autonoma.
- Ogni nuova integrazione deve avere healthcheck, MQTT state e verifica retained.
