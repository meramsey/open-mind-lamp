// No CDN, telemetry, external fonts, or browser-side RNG driving the lamp.
const $ = id => document.getElementById(id);
let token = sessionStorage.getItem('oml-token') || '';
let current = null;
function notice(message, error = false) {
  $('notice').textContent = message;
  $('notice').classList.toggle('error', error);
}
async function api(path, data) {
  const response = await fetch(`/api/${path}`, {
    method: data === undefined ? 'GET' : 'POST', cache: 'no-store',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    ...(data === undefined ? {} : { body: JSON.stringify(data) })
  });
  if (!response.ok) {
    const detail = await response.json().catch(() => ({}));
    if (response.status === 401) { token = ''; sessionStorage.removeItem('oml-token'); $('login').hidden = false; }
    throw new Error(detail.error || `Device returned HTTP ${response.status}`);
  }
  return path === 'export' ? response.blob() : response.json();
}
function setForm(settings) {
  for (const [name, value] of Object.entries(settings)) {
    const element = $('controls').elements.namedItem(name);
    if (element) { if (element.type === 'checkbox') element.checked = value; else element.value = value; }
  }
  updateOutputs();
}
function updateOutputs() {
  for (const input of document.querySelectorAll('input[type=range]')) input.nextElementSibling.textContent = input.value;
}
async function poll() {
  if (!token) return;
  const state = await api('status'); current = state;
  $('login').hidden = true;
  $('hue').textContent = state.hue.toFixed(1);
  $('orb').style.backgroundColor = `hsl(${state.hue} 80% 55%)`;
  $('orb').setAttribute('aria-label', `Hue ${state.hue.toFixed(1)} degrees; illustrative browser color`);
  $('device').textContent = state.rng_failed ? 'RNG health alarm — sampling halted; LEDs off' : state.led_ok ? 'Lamp running independently' : 'LED error';
  const entries = {
    'Entropy source': state.source, 'Firmware': state.version,
    'Uptime': `${Math.floor(state.uptime_s)} s`, 'RNG words': state.rng_words,
    'One-bit fraction': state.rng_words ? (state.rng_ones / (32 * state.rng_words)).toFixed(5) : '—',
    'RNG gross-failure alarm': state.rng_failed ? 'LATCHED' : 'None observed',
    'RF entropy condition': state.network.rf_entropy_ready ? 'Active, no power save' : 'Unavailable',
    'Missed sample slots': state.missed_slots, 'Free heap': `${state.free_heap} bytes`
  };
  $('diagnostics').replaceChildren(...Object.entries(entries).flatMap(([key, value]) => {
    const dt = document.createElement('dt'), dd = document.createElement('dd');
    dt.textContent = key; dd.textContent = value; return [dt, dd];
  }));
  const s = state.session;
  $('session').textContent = `${s.reason} · ${s.count} / ${s.planned} words${s.id ? ` · ${s.id}` : ''}`;
  $('lamp-fields').disabled = s.active;
  $('trial-fields').disabled = s.active || s.available || state.rng_failed || !s.storage_ready;
  $('stop').disabled = !s.active; $('export').disabled = !s.available || s.active; $('clear').disabled = !s.available || s.active;
  $('network').textContent = `${state.network.ap_ssid} · AP: 192.168.4.1 · LAN: ${state.network.station_ip || 'not connected'} · ${state.network.hostname}.local`;
}
function run(action) { return async event => { event?.preventDefault(); try { await action(); } catch (error) { notice(error.message, true); } }; }
$('auth').addEventListener('submit', run(async () => {
  token = $('token').value.trim(); await poll(); setForm(await api('settings'));
  sessionStorage.setItem('oml-token', token); $('token').value = ''; notice('Connected locally. Changes are saved only when you press Save.');
}));
$('controls').addEventListener('submit', run(async () => {
  const data = {};
  for (const e of $('controls').elements) if (e.name) data[e.name] = e.type === 'checkbox' ? e.checked : Number(e.value);
  await api('settings', data); notice('Settings saved; walk restarted from starting hue.'); await poll();
}));
$('trial').addEventListener('submit', run(async () => {
  const data = Object.fromEntries(new FormData($('trial'))); data.samples = Number(data.samples); data.client_utc = new Date().toISOString();
  await api('session/start', data); notice('Recording on the device. You may close this page.'); await poll();
}));
$('stop').addEventListener('click', run(async () => { await api('session/stop', {}); notice('Trial stopped early and marked user_stopped.'); await poll(); }));
$('export').addEventListener('click', run(async () => {
  const blob = await api('export'); const url = URL.createObjectURL(blob); const link = document.createElement('a');
  link.href = url; link.download = `open-mind-lamp-${current?.session.id || 'retained-trial'}.ndjson`; document.body.append(link); link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 10000);
  notice('Trial downloaded. Keep the original file for independent analysis.');
}));
$('clear').addEventListener('click', run(async () => {
  if (!confirm('Permanently delete the saved trial? Download and verify it first.')) return;
  await api('session/clear', {}); notice('Saved trial cleared.'); await poll();
}));
$('wifi').addEventListener('submit', run(async () => { await api('wifi', Object.fromEntries(new FormData($('wifi')))); $('wifi').reset(); notice('Wi-Fi saved. The private AP remains available.'); }));
$('recover').addEventListener('click', run(async () => { await api('wifi/recover', {}); notice('Home Wi-Fi forgotten.'); await poll(); }));
$('logout').addEventListener('click', () => { token = ''; sessionStorage.removeItem('oml-token'); $('login').hidden = false; notice('Browser disconnected; lamp continues.'); });
$('theme').addEventListener('click', () => { document.body.classList.toggle('light'); localStorage.setItem('oml-theme', document.body.classList.contains('light') ? 'light' : 'dark'); });
document.body.classList.toggle('light', localStorage.getItem('oml-theme') === 'light');
$('controls').addEventListener('input', updateOutputs); updateOutputs();
(async () => { if (token) { try { await poll(); setForm(await api('settings')); } catch (e) { notice(e.message, true); } } })();
// Single in-flight poll, including on a slow ESP32 or disconnected network.
(async function loop() { await new Promise(resolve => setTimeout(resolve, 1500)); try { await poll(); } catch (e) { notice(`Connection: ${e.message}. Lamp remains standalone.`, true); } loop(); })();
