# Use homelab-scaled NUT Ledcard visual classes

NUT Ledcard visual classes should use the full palette for glanceable depth, but conditions remain attention semantics rather than color semantics. We will tune Battery, Runtime, Load, Input, and NUT Clients with stepped visual classes where blue/green/yellow may represent healthy or informational depth, orange means warning attention, red means urgent critical, gray means stale telemetry, and purple means unavailable telemetry.

Runtime thresholds are intentionally homelab-scaled rather than enterprise-scaled: a few minutes of estimated UPS runtime can be normal under real homelab load, so runtime critical treatment starts at the final roughly minute-scale reserve instead of at two or five minutes. This preserves useful visual nuance without making the Power Sentinel page permanently alarm on healthy small-to-medium homelab UPS hardware.
