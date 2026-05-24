import { spawnSync } from 'node:child_process';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const source = join(scriptDir, 'power-sentinel-dashboard-fixture.c');
const fixtures = [
  ['home-current', 0],
  ['nut-current', 1],
  ['pve-current', 2],
  ['ha-current', 3],
  ['m5s-current', 4],
];

for (const [name, tab] of fixtures) {
  console.log(`\n=== Rendering ${name} (PS_ACTIVE_TAB=${tab}) ===`);
  const result = spawnSync(process.execPath, [
    join(scriptDir, 'run-lvgl-mcp-render.mjs'),
    '--source', source,
    '--name', name,
    '--define', `PS_ACTIVE_TAB=${tab}`,
  ], { stdio: 'inherit' });
  if (result.status !== 0) process.exit(result.status ?? 1);
}
