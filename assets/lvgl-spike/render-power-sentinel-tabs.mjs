import { existsSync, readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { LvglMcpRenderer } from './lvgl-mcp-render-helper.mjs';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const fixtureSource = join(scriptDir, 'power-sentinel-dashboard-fixture.c');
const fontCandidates = [
  join(scriptDir, 'ps_ui_tab_12.c'),
  join(scriptDir, '..', '..', 'firmware', 'core-s3-display', 'src', 'ps_ui_tab_12.c'),
];
const fontSource = fontCandidates.find((candidate) => existsSync(candidate)) ?? fontCandidates[0];
const bundledSource = join(scriptDir, 'power-sentinel-dashboard-fixture-bundled.c');
const resultsDir = join(scriptDir, 'results');
let source = fixtureSource;
try {
  const fixture = readFileSync(fixtureSource, 'utf8');
  const font = readFileSync(fontSource, 'utf8');
  // lvgl-mcp copies the selected source into its simulator build directory before
  // compiling, so relative #include "ps_ui_tab_12.c" would be lost. Bundle the
  // generated font into the scratch fixture to keep MCP render fidelity identical
  // without writing generated files into the Windows git checkout.
  writeFileSync(bundledSource, fixture.replace('#include "ps_ui_tab_12.c"', font), 'utf8');
  source = bundledSource;
} catch (error) {
  console.warn(`Using raw fixture without bundled sidebar font: ${error.message}`);
}
const fixtures = [
  ['home-current', 0],
  ['nut-current', 1],
  ['pve-current', 2],
  ['ha-current', 3],
  ['m5s-current', 4],
];

mkdirSync(resultsDir, { recursive: true });
const renderer = new LvglMcpRenderer();

try {
  for (const [name, tab] of fixtures) {
    console.log(`\n=== Rendering ${name} (PS_ACTIVE_TAB=${tab}) ===`);
    const summary = await renderer.render({
      source,
      name,
      width: 320,
      height: 240,
      resultsDir,
      defines: [`PS_ACTIVE_TAB=${tab}`],
    });
    console.log(JSON.stringify(summary, null, 2));
  }
} catch (error) {
  console.error(error.stack || error);
  process.exitCode = 1;
} finally {
  renderer.stop();
}
