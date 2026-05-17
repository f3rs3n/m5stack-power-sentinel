# CoreS3 display firmware

Planned stack:

- PlatformIO
- Arduino framework
- M5Unified
- M5GFX
- ArduinoJson
- WiFi / HTTPClient

Planned UI pages:

1. UPS / Power
2. Home Assistant
3. Proxmox
4. M5Stack/System
5. Alert / Offline fallback

The display should consume the LLM Module local summary endpoint, not Home Assistant as the primary source.

Planned endpoint:

```text
GET http://192.168.2.202:8088/api/v1/summary
```
