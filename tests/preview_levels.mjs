// Measure the real workbench AudioWorklet, through a dedicated local test browser.
import { parseArgs } from 'node:util';
import { writeFileSync } from 'node:fs';
const { values } = parseArgs({ options: {
  cdp: { type: 'string' }, url: { type: 'string', multiple: true }, out: { type: 'string' },
} });
if (!values.cdp || !values.url?.length || !values.out) throw new Error('--cdp, --url and --out required');
for (const address of [values.cdp, ...values.url]) {
  if (new URL(address).hostname !== '127.0.0.1') throw new Error('Local dedicated servers only');
}
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const report = { hardware_tested: false, measurements: [] };
for (const url of values.url) {
  const manifest = await (await fetch(url + '/preview/manifest.json')).json();
  const target = await (await fetch(values.cdp + '/json/new?about:blank', { method: 'PUT' })).json();
  const socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.onopen = resolve; socket.onerror = reject; });
  let serial = 0;
  const pending = new Map();
  socket.onmessage = ({ data }) => {
    const message = JSON.parse(data);
    if (!message.id) return;
    const entry = pending.get(message.id);
    if (!entry) return;
    pending.delete(message.id); clearTimeout(entry.timer);
    if (message.error) entry.reject(new Error(JSON.stringify(message.error)));
    else entry.resolve(message.result);
  };
  function command(method, params = {}) {
    return new Promise((resolve, reject) => {
      const id = ++serial;
      const timer = setTimeout(() => { pending.delete(id); reject(new Error(method + ' timed out')); }, 15000);
      pending.set(id, { resolve, reject, timer });
      socket.send(JSON.stringify({ id, method, params }));
    });
  }
  async function evaluate(expression) {
    const result = await command('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  }
  try {
    await command('Page.enable'); await command('Runtime.enable');
    await command('Emulation.setDeviceMetricsOverride', { width: 1280, height: 1000, deviceScaleFactor: 1, mobile: false });
    await command('Page.navigate', { url });
    let ready = false;
    for (let n = 0; n < 100; ++n) {
      if (await evaluate('Boolean(window.machineLab?.getStatus().displayBackend)')) { ready = true; break; }
      await delay(100);
    }
    if (!ready) throw new Error('Preview failed to initialize: ' + manifest.engine);
    const point = await evaluate('(() => { const r=document.getElementById("audio").getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2}; })()');
    await command('Input.dispatchMouseEvent', { type: 'mousePressed', ...point, button: 'left', clickCount: 1 });
    await command('Input.dispatchMouseEvent', { type: 'mouseReleased', ...point, button: 'left', clickCount: 1 });
    await delay(600);
    if (await evaluate('machineLab.getStatus().audio') !== 'running') throw new Error('Audio not running');
    for (const note of [45, 57, 69]) {
      await evaluate('machineLab.panic()');
      await delay(160);
      await evaluate(`machineLab.noteOn(${note})`);
      await delay(200);
      const samples = [];
      for (let n = 0; n < 12; ++n) {
        samples.push(await evaluate('machineLab.getStatus().rms'));
        await delay(40);
      }
      const rms = Math.sqrt(samples.reduce((sum, x) => sum + x * x, 0) / samples.length);
      if (!Number.isFinite(rms) || rms <= 0) throw new Error('No measured audio');
      const measurement = { engine: manifest.engine, note, rms, dbfs: 20 * Math.log10(rms),
        monitor: await evaluate('Number(document.getElementById("monitor").value)'),
        status: await evaluate('machineLab.getStatus()'), declared_peak_q31: manifest.definition.resources.outputPeakQ31 };
      report.measurements.push(measurement);
      console.log(`${manifest.engine} MIDI ${note}: ${measurement.dbfs.toFixed(2)} dBFS`);
    }
    await evaluate('machineLab.panic()');
  } finally {
    socket.close();
    await fetch(values.cdp + '/json/close/' + target.id);
  }
}
writeFileSync(values.out, JSON.stringify(report, null, 2) + '\n');
