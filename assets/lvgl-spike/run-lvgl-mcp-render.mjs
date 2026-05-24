import { dirname, isAbsolute, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { existsSync, mkdirSync } from 'node:fs';
import { LvglMcpRenderer } from './lvgl-mcp-render-helper.mjs';

const scriptDir = dirname(fileURLToPath(import.meta.url));
const defaultSource = join(scriptDir, 'power-sentinel-spike-001.c');
const defaultResultsDir = join(scriptDir, 'results');

function usage() {
  console.error(`Usage: node run-lvgl-mcp-render.mjs [--source file.c] [--name basename] [--width 320] [--height 240] [--results-dir dir] [--define NAME=VALUE]\n\nExamples:\n  node run-lvgl-mcp-render.mjs --source power-sentinel-home-online.c --name home-online\n  node run-lvgl-mcp-render.mjs --source power-sentinel-dashboard-fixture.c --name pve-current --define PS_ACTIVE_TAB=2`);
}

function parseArgs(argv) {
  const args = {
    source: defaultSource,
    name: 'spike-001',
    width: 320,
    height: 240,
    resultsDir: defaultResultsDir,
    defines: [],
  };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    const next = argv[i + 1];
    if (arg === '--help' || arg === '-h') {
      usage();
      process.exit(0);
    }
    if (!next || next.startsWith('--')) throw new Error(`Missing value for ${arg}`);
    i += 1;
    if (arg === '--source') args.source = next;
    else if (arg === '--name') args.name = next;
    else if (arg === '--width') args.width = Number.parseInt(next, 10);
    else if (arg === '--height') args.height = Number.parseInt(next, 10);
    else if (arg === '--results-dir') args.resultsDir = next;
    else if (arg === '--define') args.defines.push(next);
    else throw new Error(`Unknown argument ${arg}`);
  }
  args.source = isAbsolute(args.source) ? args.source : resolve(scriptDir, args.source);
  args.resultsDir = isAbsolute(args.resultsDir) ? args.resultsDir : resolve(scriptDir, args.resultsDir);
  if (!Number.isFinite(args.width) || !Number.isFinite(args.height) || args.width <= 0 || args.height <= 0) {
    throw new Error(`Invalid resolution ${args.width}x${args.height}`);
  }
  if (!/^[A-Za-z0-9._-]+$/.test(args.name)) throw new Error(`Unsafe output name ${args.name}`);
  if (!existsSync(args.source)) throw new Error(`Source not found: ${args.source}`);
  return args;
}

const args = parseArgs(process.argv.slice(2));
mkdirSync(args.resultsDir, { recursive: true });
const renderer = new LvglMcpRenderer();

renderer.render(args).then((summary) => {
  console.log(JSON.stringify(summary, null, 2));
}).catch((err) => {
  console.error(err.stack || err);
  process.exitCode = 1;
}).finally(() => {
  renderer.stop();
});
