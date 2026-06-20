# Use module-local configuration and runtime module results

Power Sentinel modules are independently scoped capabilities, not only payload sections. Each implemented module owns its operational responsibility, configuration/lifecycle, adapters, condition/status interpretation, and user-facing surface.

Module configuration is local to the module section. Each implemented module uses a shared `enabled: true|false` convention inside its own configuration section, while module-specific settings remain owned and validated by that module. The old central `modules` enablement registry is superseded and should not be preserved as a runtime compatibility path unless a real deployed migration need is identified. The current production footprint is the development unit, so the preferred migration is to update the config, examples, docs, and tests directly rather than carry a permanent compatibility adapter.

`ModuleRuntime` registers real implemented modules only. Roadmap ideas such as Home Assistant should not appear as runtime placeholder modules until they are real capabilities. `Not Implemented` remains vocabulary for roadmap or explicit compatibility needs, not a registry pattern.

A real implemented module always produces a summary so configuration and diagnostics remain visible. Disabled modules produce a disabled summary, but do not contribute a condition and do not expose an active page. Enabled modules contribute a condition and should expose their page when the module has one, including unavailable or unconfigured states. An enabled but unconfigured module uses `condition: unavailable` with `status: unconfigured` and configuration context; `unconfigured` is not a separate canonical condition.

Page visibility is separate from summary construction. On the Ambient Console, an enabled but unconfigured page should remain visible, preserve the module's normal shape where useful, and use unavailable visual treatment plus clear unconfigured copy instead of fake telemetry. In the current Proxmox Ambient Page this is a card/page-model treatment, not a separate overlay layer: metrics render as `--`, card state text renders `UNCONFIGURED`, and the unavailable visual class is used.

Consequences:

- Update `backend/config/power-sentinel.example.json` to module-local `enabled` fields.
- Remove the runtime central `modules` enablement registry and Home Assistant placeholder from the backend unless Home Assistant is implemented.
- Keep `ModuleRuntime` focused on registration, calling modules, aggregate condition, page list construction, and product summary assembly.
- Keep module-specific config validation, external adapters, status interpretation, and condition construction inside each module implementation.
- Update tests to cover disabled summaries with no condition/page contribution, enabled unconfigured modules with unavailable condition and visible page, and aggregate condition ignoring disabled modules.
- Update firmware handling so enabled unavailable/unconfigured module pages render visibly with unavailable visual treatment and unconfigured context rather than disappearing.
