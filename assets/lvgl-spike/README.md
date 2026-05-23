# LVGL MCP local Windows spike

Purpose: prove that `jaklys/lvgl-mcp-esp32` can render a Power Sentinel-style LVGL UI at the CoreS3 resolution, 320x240, before we invest in a full Hermes LXC <-> Windows MCP bridge.

## Current finding: transport

As of the inspected upstream `main` branch, `lvgl-mcp-esp32` is stdio-only:

- `mcp-server/src/index.ts` imports `StdioServerTransport` from `@modelcontextprotocol/sdk/server/stdio.js`.
- `index.ts` creates `const transport = new StdioServerTransport();` and connects the MCP server to that transport.
- There is no HTTP / StreamableHTTP transport source file in `mcp-server/src/`.
- `mcp-server/package.json` exposes only the `lvgl-mcp-server` bin and `npm start`/`dev` scripts.

Implication: run it locally from a Windows MCP client first. Direct Hermes LXC integration will require either a Windows-side MCP HTTP wrapper/proxy or running an agent/client on Windows.

## Windows prerequisites

Install/verify on the Windows PC:

- Windows 10/11 x64
- Visual Studio Build Tools 2019+ with C++ build tools workload
- CMake 3.16+
- Ninja 1.10+
- Node.js 18+
- Git

PowerShell verification:

```powershell
node --version
npm --version
git --version
cmake --version
ninja --version
where cl
```

`where cl` normally only works inside a Developer PowerShell / Developer Command Prompt, unless the MSVC environment is already loaded. If it fails in normal PowerShell but works in "Developer PowerShell for VS", that is acceptable.

## Fast npm install path

```powershell
npm install -g lvgl-mcp-server
npx lvgl-mcp-server
```

Expected behavior for `npx lvgl-mcp-server`: it should start an MCP stdio server and wait; it is not a normal CLI that prints a screenshot. Stop it with Ctrl+C after confirming it does not immediately crash.

## Hermes-driven render harness

After Windows prerequisites and `npm install -g lvgl-mcp-server`, Hermes can render a fixture over SSH without a browser or MCP Inspector:

```powershell
cd C:\Users\marti\Progetti\m5stack-power-sentinel\assets\lvgl-spike
node .\run-lvgl-mcp-render.mjs --source .\power-sentinel-home-online.c --name home-online
```

Outputs are written under `assets/lvgl-spike/results/`:

```text
home-online.png
home-online-widget-tree.json
home-online-render-result.json
home-online-tools.json
home-online-stderr.txt
```

The script is intentionally generic: pass `--source`, `--name`, `--width`, `--height`, and `--results-dir` to render additional fixtures. Default resolution is the CoreS3 display size, 320x240.

`power-sentinel-home-online.c` is a curated visual fixture that mirrors the production HOME tab contract: real tab order `HOME`/`NUT`/`PVE`/`HA`/`M5S`, modern dark theme, HOME hero card, status pills, real `NET`/`M5S` local row, and `SLEEP DISPLAY`. Keep it aligned with `firmware/core-s3-display/src/main.cpp`; `tools/check_core_s3_ui.py` enforces key strings so the fixture does not silently rot.

## Recommended local testing path: MCP Inspector

The MCP Inspector is useful because it lets you call stdio MCP tools from a browser without configuring Claude Code first.

```powershell
npx -y @modelcontextprotocol/inspector npx lvgl-mcp-server
```

Then open the local Inspector URL it prints, connect, and verify the tools list includes:

- `lvgl_render`
- `lvgl_render_full`
- `lvgl_inspect`
- `lvgl_set_resolution`

Use `lvgl_set_resolution` first:

```json
{"width":320,"height":240}
```

Then call `lvgl_render_full` with the contents of:

```text
assets/lvgl-spike/power-sentinel-spike-001.c
```

The render should return a PNG screenshot plus a widget tree JSON.

## Claude Code config option

If using Claude Code on Windows, add something like this to `.claude/settings.json` or global Claude settings:

```json
{
  "mcpServers": {
    "lvgl-simulator": {
      "command": "npx",
      "args": ["lvgl-mcp-server"]
    }
  }
}
```

Then ask Claude Code to:

```text
Use lvgl_set_resolution with width=320 height=240.
Use lvgl_render_full with the complete C file at assets/lvgl-spike/power-sentinel-spike-001.c.
Return the PNG screenshot and widget tree JSON.
```

If installed from source instead, use:

```json
{
  "mcpServers": {
    "lvgl-simulator": {
      "command": "node",
      "args": ["C:/Users/YOUR_USER/path/to/Lvgl-mcp-esp32/mcp-server/dist/index.js"],
      "env": {
        "LVGL_PROJECT_ROOT": "C:/Users/YOUR_USER/path/to/Lvgl-mcp-esp32"
      }
    }
  }
}
```

Use forward slashes in Windows paths.

## Source install fallback

If npm package install fails or the bundled simulator does not download correctly:

```powershell
git clone --recursive https://github.com/jaklys/Lvgl-mcp-esp32.git
cd Lvgl-mcp-esp32
powershell -ExecutionPolicy Bypass -File scripts/setup.ps1
```

Manual fallback:

```powershell
scripts\build.bat
cd mcp-server
npm install
npm run build
```

## Spike success criteria

The spike is successful if:

1. `lvgl_set_resolution` accepts 320x240.
2. `lvgl_render_full` compiles `power-sentinel-spike-001.c`.
3. The PNG shows a dark 320x240 Power Sentinel-style HOME screen, not a blank screen.
4. The widget tree includes labels such as:
   - `POWER SENTINEL`
   - `GRID ONLINE`
   - `NUT OK`
   - `PVE OK`
   - `HA OK`
   - `NET OK   M5S OK`
5. Render time is sane, ideally under a few seconds after first build/warmup.

## What to send back to Hermes

After the render, save or copy back:

- PNG screenshot
- widget tree JSON
- any compile warnings/errors
- exact installed versions:

```powershell
node --version
npm --version
cmake --version
ninja --version
```

If using Inspector, export/copy the tool output into a file under this directory, for example:

```text
assets/lvgl-spike/results/spike-001-widget-tree.json
assets/lvgl-spike/results/spike-001-notes.txt
```

Do not commit large temporary render dumps unless we decide to keep them as curated visual baselines.
