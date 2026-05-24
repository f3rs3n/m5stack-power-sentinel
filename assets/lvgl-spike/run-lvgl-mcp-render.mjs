import { spawn, spawnSync } from 'node:child_process';
import { dirname, isAbsolute, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';

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
const definePrefix = args.defines.map((define) => {
  if (!/^[A-Za-z_][A-Za-z0-9_]*(=.*)?$/.test(define)) throw new Error(`Unsafe define ${define}`);
  const [name, ...rest] = define.split('=');
  return `#define ${name} ${rest.length ? rest.join('=') : '1'}`;
}).join('\n');
const code = `${definePrefix ? `${definePrefix}\n` : ''}${readFileSync(args.source, 'utf8')}`;

const command = process.platform === 'win32' ? 'cmd.exe' : 'npx';
const commandArgs = process.platform === 'win32'
  ? ['/d', '/s', '/c', 'npx lvgl-mcp-server']
  : ['lvgl-mcp-server'];

const child = spawn(command, commandArgs, {
  stdio: ['pipe', 'pipe', 'pipe'],
  env: process.env,
});

let stderr = '';
child.stderr.on('data', (d) => {
  stderr += d.toString();
});

let buffer = Buffer.alloc(0);
const pending = new Map();
let nextId = 1;

function send(obj) {
  // @modelcontextprotocol/sdk stdio transport uses newline-delimited JSON-RPC.
  child.stdin.write(JSON.stringify(obj) + '\n');
}

function request(method, params = {}) {
  const id = nextId++;
  send({ jsonrpc: '2.0', id, method, params });
  return new Promise((resolvePromise, reject) => {
    pending.set(id, { resolve: resolvePromise, reject });
    setTimeout(() => {
      if (pending.has(id)) {
        pending.delete(id);
        reject(new Error(`Timeout waiting for ${method}`));
      }
    }, 180000);
  });
}

function handleMessage(msg) {
  if (msg.id && pending.has(msg.id)) {
    const { resolve: resolvePromise, reject } = pending.get(msg.id);
    pending.delete(msg.id);
    if (msg.error) reject(new Error(JSON.stringify(msg.error)));
    else resolvePromise(msg.result);
  }
}

child.stdout.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  while (true) {
    const newline = buffer.indexOf('\n');
    if (newline === -1) return;
    const line = buffer.subarray(0, newline).toString('utf8').trim();
    buffer = buffer.subarray(newline + 1);
    if (!line) continue;
    handleMessage(JSON.parse(line));
  }
});

function writeResultFile(suffix, data) {
  writeFileSync(join(args.resultsDir, `${args.name}${suffix}`), data);
}

async function main() {
  const init = await request('initialize', {
    protocolVersion: '2024-11-05',
    capabilities: {},
    clientInfo: { name: 'hermes-lvgl-spike', version: '0.2.0' },
  });
  send({ jsonrpc: '2.0', method: 'notifications/initialized', params: {} });

  const tools = await request('tools/list', {});
  writeResultFile('-tools.json', JSON.stringify(tools, null, 2));

  await request('tools/call', {
    name: 'lvgl_set_resolution',
    arguments: { width: args.width, height: args.height },
  });

  const rendered = await request('tools/call', {
    name: 'lvgl_render_full',
    arguments: { code },
  });
  writeResultFile('-render-result.json', JSON.stringify(rendered, null, 2));
  writeResultFile('-stderr.txt', stderr);

  const image = rendered.content?.find((c) => c.type === 'image');
  if (image?.data) {
    writeResultFile('.png', Buffer.from(image.data, 'base64'));
  }
  const widgetText = rendered.content?.find((c) => c.type === 'text' && c.text?.startsWith('Widget tree:'));
  if (widgetText) {
    writeResultFile('-widget-tree.json', widgetText.text.replace(/^Widget tree:\n/, ''));
  }
  console.log(JSON.stringify({
    ok: !rendered.isError,
    source: args.source,
    name: args.name,
    resolution: `${args.width}x${args.height}`,
    init,
    tools: tools.tools?.map(t => t.name),
    resultTypes: rendered.content?.map(c => c.type),
    stderr,
  }, null, 2));
}

function stopChild() {
  try { child.stdin.end(); } catch {}
  if (!child.pid) return;
  if (process.platform === 'win32') {
    spawnSync('taskkill', ['/pid', String(child.pid), '/t', '/f'], { stdio: 'ignore' });
  } else {
    try { child.kill('SIGTERM'); } catch {}
  }
}

main().catch((err) => {
  writeResultFile('-error.txt', `${err.stack || err}\n\nSTDERR:\n${stderr}`);
  console.error(err.stack || err);
  process.exitCode = 1;
}).finally(() => {
  stopChild();
});
