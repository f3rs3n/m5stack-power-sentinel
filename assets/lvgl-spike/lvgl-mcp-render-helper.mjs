import { spawn, spawnSync } from 'node:child_process';
import { join } from 'node:path';
import { readFileSync, writeFileSync } from 'node:fs';

export function buildDefinePrefix(defines = []) {
  return defines.map((define) => {
    if (!/^[A-Za-z_][A-Za-z0-9_]*(=.*)?$/.test(define)) throw new Error(`Unsafe define ${define}`);
    const [name, ...rest] = define.split('=');
    return `#define ${name} ${rest.length ? rest.join('=') : '1'}`;
  }).join('\n');
}

export function buildRenderCode(source, defines = []) {
  const definePrefix = buildDefinePrefix(defines);
  return `${definePrefix ? `${definePrefix}\n` : ''}${readFileSync(source, 'utf8')}`;
}

export class LvglMcpRenderer {
  constructor({ clientName = 'hermes-lvgl-spike', clientVersion = '0.2.0', requestTimeoutMs = 180000 } = {}) {
    this.clientName = clientName;
    this.clientVersion = clientVersion;
    this.requestTimeoutMs = requestTimeoutMs;
    this.stderr = '';
    this.buffer = Buffer.alloc(0);
    this.pending = new Map();
    this.nextId = 1;
    this.started = false;
    this.initialized = false;
    this.initResult = null;
    this.tools = null;

    const command = process.platform === 'win32' ? 'cmd.exe' : 'npx';
    const commandArgs = process.platform === 'win32'
      ? ['/d', '/s', '/c', 'npx lvgl-mcp-server']
      : ['lvgl-mcp-server'];

    this.child = spawn(command, commandArgs, {
      stdio: ['pipe', 'pipe', 'pipe'],
      env: process.env,
    });
    this.started = true;

    this.child.stderr.on('data', (d) => {
      this.stderr += d.toString();
    });

    this.child.stdout.on('data', (chunk) => {
      this.buffer = Buffer.concat([this.buffer, chunk]);
      while (true) {
        const newline = this.buffer.indexOf('\n');
        if (newline === -1) return;
        const line = this.buffer.subarray(0, newline).toString('utf8').trim();
        this.buffer = this.buffer.subarray(newline + 1);
        if (!line) continue;
        try {
          this.#handleMessage(JSON.parse(line));
        } catch (error) {
          this.#rejectAll(new Error(`Failed to parse MCP response: ${error.message}\nLine: ${line}`));
        }
      }
    });

    this.child.once('error', (error) => {
      this.#rejectAll(error);
    });

    this.child.once('close', (code, signal) => {
      this.#rejectAll(new Error(`lvgl-mcp-server exited unexpectedly (code=${code}, signal=${signal})`));
    });
  }

  send(obj) {
    this.child.stdin.write(JSON.stringify(obj) + '\n');
  }

  request(method, params = {}) {
    const id = this.nextId++;
    this.send({ jsonrpc: '2.0', id, method, params });
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        if (this.pending.has(id)) {
          this.pending.delete(id);
          reject(new Error(`Timeout waiting for ${method}`));
        }
      }, this.requestTimeoutMs);
      this.pending.set(id, { resolve, reject, timeout });
    });
  }

  #handleMessage(msg) {
    if (msg.id && this.pending.has(msg.id)) {
      const { resolve, reject, timeout } = this.pending.get(msg.id);
      clearTimeout(timeout);
      this.pending.delete(msg.id);
      if (msg.error) reject(new Error(JSON.stringify(msg.error)));
      else resolve(msg.result);
    }
  }

  #rejectAll(error) {
    for (const { reject, timeout } of this.pending.values()) {
      clearTimeout(timeout);
      reject(error);
    }
    this.pending.clear();
  }

  async initialize() {
    if (this.initialized) return { init: this.initResult, tools: this.tools };
    this.initResult = await this.request('initialize', {
      protocolVersion: '2024-11-05',
      capabilities: {},
      clientInfo: { name: this.clientName, version: this.clientVersion },
    });
    this.send({ jsonrpc: '2.0', method: 'notifications/initialized', params: {} });
    this.tools = await this.request('tools/list', {});
    this.initialized = true;
    return { init: this.initResult, tools: this.tools };
  }

  async render({ source, name, width = 320, height = 240, resultsDir, defines = [] }) {
    if (!resultsDir) throw new Error('resultsDir is required');
    if (!/^[A-Za-z0-9._-]+$/.test(name)) throw new Error(`Unsafe output name ${name}`);

    const stderrStart = this.stderr.length;
    const writeResultFile = (suffix, data) => {
      writeFileSync(join(resultsDir, `${name}${suffix}`), data);
    };

    try {
      const { init, tools } = await this.initialize();
      writeResultFile('-tools.json', JSON.stringify(tools, null, 2));

      await this.request('tools/call', {
        name: 'lvgl_set_resolution',
        arguments: { width, height },
      });

      const code = buildRenderCode(source, defines);
      const rendered = await this.request('tools/call', {
        name: 'lvgl_render_full',
        arguments: { code },
      });

      const stderr = this.stderr.slice(stderrStart);
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

      return {
        ok: !rendered.isError,
        source,
        name,
        resolution: `${width}x${height}`,
        init,
        tools: tools.tools?.map(t => t.name),
        resultTypes: rendered.content?.map(c => c.type),
        stderr,
      };
    } catch (err) {
      writeResultFile('-error.txt', `${err.stack || err}\n\nSTDERR:\n${this.stderr.slice(stderrStart)}`);
      throw err;
    }
  }

  stop() {
    try { this.child.stdin.end(); } catch {}
    if (!this.child.pid) return;
    if (process.platform === 'win32') {
      spawnSync('taskkill', ['/pid', String(this.child.pid), '/t', '/f'], { stdio: 'ignore' });
    } else {
      try { this.child.kill('SIGTERM'); } catch {}
    }
  }
}
