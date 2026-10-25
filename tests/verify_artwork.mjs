// Capture adapted SVG reference and C++ frames through an existing dedicated CDP browser.
import { readFileSync, writeFileSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const machine = fileURLToPath(new URL('../', import.meta.url));
const options = {};
for (let i = 2; i < process.argv.length; i += 2) {
  if (!process.argv[i].startsWith('--') || !process.argv[i + 1])
    throw new Error(
      'Expected --sdk-root PATH --out PATH [--cdp-url URL] [--cxx COMPILER] [--python PYTHON]',
    );
  options[process.argv[i].slice(2)] = process.argv[i + 1];
}
if (!options['sdk-root'] || !options.out) throw new Error('--sdk-root and --out are required');
const out = options.out.replace(/\/$/, '') + '/';
const python = options.python ?? 'python3';
const argumentsForPython = [
  '--sdk-root',
  options['sdk-root'],
  '--out',
  options.out,
  '--cxx',
  options.cxx ?? 'g++',
];
execFileSync(python, [machine + 'tests/verify_artwork.py', 'prepare', ...argumentsForPython], {
  stdio: 'inherit',
});
const frames = JSON.parse(readFileSync(out + 'frames.json', 'utf8'));
const cdp = options['cdp-url'] ?? 'http://127.0.0.1:9222';
const target = await (await fetch(cdp + '/json/new?about:blank', { method: 'PUT' })).json();
const socket = new WebSocket(target.webSocketDebuggerUrl);
await new Promise((r, j) => {
  socket.onopen = r;
  socket.onerror = j;
});
let sequence = 0;
const pending = new Map();
socket.onmessage = ({ data }) => {
  const m = JSON.parse(data);
  if (m.id) {
    const p = pending.get(m.id);
    pending.delete(m.id);
    m.error ? p.reject(new Error(JSON.stringify(m.error))) : p.resolve(m.result);
  }
};
const command = (method, params = {}) =>
  new Promise((resolve, reject) => {
    const id = ++sequence;
    pending.set(id, { resolve, reject });
    socket.send(JSON.stringify({ id, method, params }));
  });
try {
  await command('Page.enable');
  await command('Emulation.setDeviceMetricsOverride', {
    width: 480,
    height: 220,
    deviceScaleFactor: 1,
    mobile: false,
  });
  for (let k = 0; k < frames.length; ++k)
    for (const kind of ['reference', 'cpp']) {
      const svg = readFileSync(out + kind + '-' + k + '.svg', 'utf8');
      const expression = `(()=>{document.body.style='margin:0;background:black;overflow:hidden';document.body.innerHTML=${JSON.stringify(svg)};const s=document.querySelector('svg');s.pauseAnimations();s.setCurrentTime(${frames[k].animation_time});return true;})()`;
      const r = await command('Runtime.evaluate', { expression, returnByValue: true });
      if (r.exceptionDetails) throw new Error(JSON.stringify(r.exceptionDetails));
      await new Promise((r) => setTimeout(r, 40));
      const shot = await command('Page.captureScreenshot', {
        format: 'png',
        clip: { x: 0, y: 20, width: 480, height: 200, scale: 1 },
        captureBeyondViewport: true,
      });
      writeFileSync(out + kind + '-' + k + '.png', Buffer.from(shot.data, 'base64'));
    }
} finally {
  await command('Page.close');
  socket.close();
}
execFileSync(python, [machine + 'tests/verify_artwork.py', 'compare', ...argumentsForPython], {
  stdio: 'inherit',
});
