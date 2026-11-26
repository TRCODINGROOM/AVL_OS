// AVL OS desktop shell. Runs full screen as the only app of the AVL OS live system.
const { app, BrowserWindow, ipcMain, session, shell } = require('electron');
const { execFile } = require('child_process');
const fs = require('fs'), os = require('os'), path = require('path');

if (process.argv.includes('--vm') || process.env.AVL_SOFT === '1') app.disableHardwareAcceleration();
app.userAgentFallback = app.userAgentFallback.replace(/ Electron\/\S+/, '').replace(/ avl-os-desktop\/\S+/, '');
let win = null;
const HOME = os.homedir();

function run(cmd, args, opt = {}) {
  return new Promise((res) => execFile(cmd, args, { timeout: opt.timeout || 8000, maxBuffer: 4e6, env: { ...process.env, LC_ALL: 'C', ...(opt.env || {}) }, cwd: opt.cwd }, (err, out, errout) =>
    res({ code: err ? (typeof err.code === 'number' ? err.code : 1) : 0, out: String(out || ''), err: String(errout || (err && err.killed ? 'timed out' : '')) })));
}
const splitNm = (line) => { const out = []; let cur = ''; for (let i = 0; i < line.length; i++) { const c = line[i]; if (c === '\\' && line[i + 1]) { cur += line[++i]; } else if (c === ':') { out.push(cur); cur = ''; } else cur += c; } out.push(cur); return out; };
const MAC = /^[0-9A-F]{2}(:[0-9A-F]{2}){5}$/i;

const api = {
  async info() {
    const nets = os.networkInterfaces(), ips = [];
    for (const k of Object.keys(nets)) for (const a of nets[k]) if (!a.internal && a.family === 'IPv4') ips.push(k + ' ' + a.address);
    const bat = await run('bash', ['-c', 'cat /sys/class/power_supply/BAT*/capacity 2>/dev/null | head -1']);
    return { host: os.hostname(), cpus: os.cpus().length, cpuModel: (os.cpus()[0] || {}).model || '', memTotal: Math.round(os.totalmem() / 1048576), memFree: Math.round(os.freemem() / 1048576), kernel: os.release(), uptime: Math.round(os.uptime()), ips, battery: bat.out.trim() ? parseInt(bat.out, 10) : null, home: HOME };
  },
  async netStatus() {
    const r = await run('nmcli', ['-t', '-f', 'DEVICE,TYPE,STATE,CONNECTION', 'dev']);
    const devs = r.out.split('\n').filter(Boolean).map(splitNm).map(([device, type, state, conn]) => ({ device, type, state, conn }));
    const w = await run('nmcli', ['radio', 'wifi']);
    return { ok: r.code === 0, devs, wifiRadio: w.out.trim() === 'enabled', hasWifi: devs.some((d) => d.type === 'wifi') };
  },
  async wifiList() {
    const r = await run('nmcli', ['-t', '-f', 'IN-USE,SSID,SIGNAL,SECURITY', 'dev', 'wifi', 'list', '--rescan', 'auto'], { timeout: 15000 });
    const seen = new Set(), list = [];
    for (const l of r.out.split('\n').filter(Boolean)) { const [use, ssid, sig, sec] = splitNm(l); if (!ssid || seen.has(ssid)) continue; seen.add(ssid); list.push({ ssid, signal: +sig || 0, secure: !!sec && sec !== '--', active: use === '*' }); }
    list.sort((a, b) => (b.active - a.active) || (b.signal - a.signal));
    return { ok: r.code === 0, list, err: r.err };
  },
  async wifiPower(on) { await run('rfkill', ['unblock', 'wifi']); const r = await run('nmcli', ['radio', 'wifi', on ? 'on' : 'off']); return { ok: r.code === 0, err: r.err }; },
  async wifiConnect(ssid, pw) {
    const args = ['dev', 'wifi', 'connect', String(ssid)]; if (pw) args.push('password', String(pw));
    const r = await run('nmcli', args, { timeout: 40000 }); return { ok: r.code === 0, msg: (r.out + r.err).trim() };
  },
  async btStatus() {
    const r = await run('bluetoothctl', ['show']); const rf = await run('rfkill', ['list', 'bluetooth']);
    const has = r.code === 0 && /Controller/.test(r.out);
    return { has, powered: /Powered: yes/.test(r.out), blocked: /Soft blocked: yes/.test(rf.out), name: ((r.out.match(/Alias: (.*)/) || [])[1] || '') };
  },
  async btPower(on) { await run('rfkill', ['unblock', 'bluetooth']); const r = await run('bluetoothctl', ['power', on ? 'on' : 'off']); return { ok: r.code === 0, msg: (r.out + r.err).trim() }; },
  async btScan() { await run('bluetoothctl', ['--timeout', '6', 'scan', 'on'], { timeout: 12000 }); return api.btDevices(); },
  async btDevices() {
    const r = await run('bluetoothctl', ['devices']); const list = [];
    for (const l of r.out.split('\n')) { const m = l.match(/^Device ([0-9A-F:]{17}) (.*)$/i); if (m) { const i = await run('bluetoothctl', ['info', m[1]]); list.push({ mac: m[1], name: m[2], connected: /Connected: yes/.test(i.out), paired: /Paired: yes/.test(i.out) }); } }
    return { ok: true, list };
  },
  async btConnect(mac) {
    if (!MAC.test(mac)) return { ok: false, msg: 'bad address' };
    await run('bluetoothctl', ['--agent', 'NoInputNoOutput', 'pair', mac], { timeout: 25000 }); await run('bluetoothctl', ['trust', mac]);
    const r = await run('bluetoothctl', ['connect', mac], { timeout: 20000 }); return { ok: /successful/i.test(r.out), msg: (r.out + r.err).trim().split('\n').pop() };
  },
  async btDisconnect(mac) { if (!MAC.test(mac)) return { ok: false }; const r = await run('bluetoothctl', ['disconnect', mac], { timeout: 10000 }); return { ok: r.code === 0 }; },
  async volGet() { const r = await run('pactl', ['get-sink-volume', '@DEFAULT_SINK@']); const m = r.out.match(/(\d+)%/); const mu = await run('pactl', ['get-sink-mute', '@DEFAULT_SINK@']); return { ok: !!m, vol: m ? +m[1] : 0, muted: /yes/.test(mu.out) }; },
  async volSet(v) { v = Math.max(0, Math.min(150, v | 0)); const r = await run('pactl', ['set-sink-volume', '@DEFAULT_SINK@', v + '%']); return { ok: r.code === 0 }; },
  async power(what) { if (what === 'off') run('sudo', ['-n', 'systemctl', 'poweroff']); else if (what === 'reboot') run('sudo', ['-n', 'systemctl', 'reboot']); return { ok: true }; },
  async list(dir) {
    try { const d = path.resolve(dir || HOME); const items = fs.readdirSync(d, { withFileTypes: true }).filter((e) => !e.name.startsWith('.')).map((e) => { let size = 0; try { if (!e.isDirectory()) size = fs.statSync(path.join(d, e.name)).size; } catch (_) {} return { name: e.name, dir: e.isDirectory(), size }; }).sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name)); return { ok: true, dir: d, items }; }
    catch (e) { return { ok: false, err: String(e.message) }; }
  },
  async readText(file) { try { const f = path.resolve(file); const st = fs.statSync(f); const fd = fs.openSync(f, 'r'); const buf = Buffer.alloc(Math.min(4096, st.size)); fs.readSync(fd, buf, 0, buf.length, 0); fs.closeSync(fd); return { ok: true, size: st.size, text: buf.toString('utf8') }; } catch (e) { return { ok: false, err: String(e.message) }; } },
  async writeText(file, text) { try { const f = path.resolve(HOME, file); fs.mkdirSync(path.dirname(f), { recursive: true }); fs.writeFileSync(f, String(text)); return { ok: true }; } catch (e) { return { ok: false, err: String(e.message) }; } },
  async exec(cmd, cwd) {
    const r = await run('bash', ['-c', 'cd "$AVL_CWD" 2>/dev/null; eval "$AVL_CMD" 2>&1; echo; echo "__AVLCWD__:$PWD"'], { timeout: 15000, env: { AVL_CWD: cwd || HOME, AVL_CMD: String(cmd) }, cwd: HOME });
    const m = r.out.match(/__AVLCWD__:(.*)\n?$/); let out = r.out.replace(/\n?__AVLCWD__:.*\n?$/, '').replace(/\n+$/, ''); if (r.err === 'timed out') out += '\n[timed out after 15 s]';
    return { out, cwd: m ? m[1] : cwd || HOME };
  },
};
for (const k of Object.keys(api)) ipcMain.handle('avl:' + k, (_e, ...a) => api[k](...a));

app.on('web-contents-created', (_e, c) => {
  c.setWindowOpenHandler(({ url }) => { if (c.getType() === 'webview' && win) win.webContents.send('avl:newtab', url); else if (/^https?:/.test(url)) shell.openExternal(url); return { action: 'deny' }; });
});
app.whenReady().then(() => {
  const ys = session.fromPartition('persist:yadu');
  ys.setPermissionRequestHandler((_wc, _perm, cb) => cb(true));
  ys.on('will-download', (_e, item) => { const dl = path.join(HOME, 'Downloads'); try { fs.mkdirSync(dl, { recursive: true }); } catch (_) {} item.setSavePath(path.join(dl, item.getFilename())); item.once('done', (_ev, st) => { if (win) win.webContents.send('avl:download', item.getFilename(), st); }); });
  win = new BrowserWindow({ fullscreen: true, autoHideMenuBar: true, backgroundColor: '#070b11', webPreferences: { webviewTag: true, contextIsolation: true, nodeIntegration: false, preload: path.join(__dirname, 'preload.js') } });
  win.loadFile('index.html', { query: { desktop: '1' } });
});
app.on('window-all-closed', () => app.quit());
