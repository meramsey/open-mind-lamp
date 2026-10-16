// No CDN, telemetry, external fonts, or browser-side RNG driving the lamp.
const $ = id => document.getElementById(id);
let token = sessionStorage.getItem('oml-token') || '';
let current = null;
let pollPromise = null, epoch = 0, calibrationBusy = false, calibrationKey = '';
let colorReceivedAt = 0, colorTransitMs = 0;
function notice(message, error = false) {
  $('notice').textContent = message;
  $('notice').classList.toggle('error', error);
}
async function api(path, data) {
  const requestToken = token;
  const response = await fetch(`/api/${path}`, {
    method: data === undefined ? 'GET' : 'POST', cache: 'no-store',
    headers: { Authorization: `Bearer ${requestToken}`, 'Content-Type': 'application/json' },
    ...(data === undefined ? {} : { body: JSON.stringify(data) })
  });
  if (!response.ok) {
    const detail = await response.json().catch(() => ({}));
    if (response.status === 401 && token === requestToken) { token = ''; ++epoch; sessionStorage.removeItem('oml-token'); $('login').hidden = false; }
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
function colorAge() {
  if (!current?.color || !token) { $('color-age').textContent = 'No current color snapshot'; return; }
  const color = current.color;
  const seconds = (color.age_ms + colorTransitMs + performance.now() - colorReceivedAt) / 1000;
  $('color-age').textContent = `${color.valid ? 'Last transmitted command' : 'Output unknown; last successful command retained'} · frame ${color.frame_id} · ~${seconds.toFixed(1)} s ago${seconds > 5 ? ' · STALE' : ''}`;
}
function showCalibration(c, state) {
  const active = c.active;
  const key = `${c.run_id}:${c.step}`;
  if (key !== calibrationKey) { $('calibration-seen').value = c.expected; calibrationKey = key; }
  $('calibration-state').textContent = `Saved order: ${c.saved_order} (${c.saved_order_confirmed ? 'user-confirmed' : 'not confirmed by this workflow'}) · ${c.phase}${active ? ` · ${c.phase === 'identify' ? 'identify' : 'verify'} ${c.index + 1}/${c.phase === 'identify' ? 3 : 7} · proposed ${c.candidate_order} · ${Math.ceil(c.remaining_ms / 1000)} s left` : ''}`;
  $('calibration-expected').textContent = c.expected;
  $('calibration-start').disabled = active || state.session.active || state.rng_failed || !state.led_ok || !state.network.rf_entropy_ready || calibrationBusy;
  $('calibration-observation').hidden = !active || c.ready;
  const pending = !c.presented || calibrationBusy;
  $('calibration-match').disabled = pending; $('calibration-report').disabled = pending;
  $('calibration-save').hidden = !active || !c.ready;
  $('calibration-save').disabled = pending;
  $('calibration-cancel').hidden = !active;
  $('calibration-cancel').disabled = calibrationBusy;
}
function showState(state) {
  current = state; colorReceivedAt = performance.now();
  $('login').hidden = true;
  const color = state.color;
  if (!color) throw new Error('Firmware lacks rendered color state; deploy the matching update.');
  const command = color.commanded_rgb;
  $('hue').textContent = color.logical_hue.toFixed(1);
  $('orb').style.backgroundColor = color.valid ? `rgb(${command.join(', ')})` : 'transparent';
  $('orb').style.boxShadow = 'none';
  $('orb').setAttribute('aria-label', color.valid ? `Commanded RGB ${command.join(', ')}; not optically measured` : 'LED output unknown');
  $('rgb-command').textContent = `Commanded RGB: ${command.join(', ')} · ${color.lit_pixels}/64 pixels lit · ${color.channel_order} wire order`;
  colorAge();
  $('device').textContent = state.rng_failed ? 'RNG health alarm — sampling halted; LEDs off' : state.led_ok ? 'Lamp running independently' : 'LED error';
  const entries = {
    'Entropy source': state.source, 'Firmware': state.version,
    'Walk target hue': `${state.target_hue.toFixed(1)}°`, 'Smoothed walk hue': `${state.hue.toFixed(1)}°`,
    'Logical RGB (before transforms)': color.logical_rgb.join(', '),
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
  const rng = state.rng;
  if (rng) {
    const modes = ['Fixed ambient', 'Random Walk', 'Walk + timed breathing brightness',
      'Walk + single pixel', 'Fixed manual calibration', 'Walk + RNG activity mask', 'Fixed experiment status'];
    $('rng-mode').textContent = state.calibration.active ? 'Guided calibration overrides the walk display.' :
      `${modes[color.mode]}${!color.lit_pixels ? ' · output is black' : ''}. Use Random Walk (mode 1), LEDs enabled, for the live causality test.`;
    const values = {
      'Source / readiness': `${rng.source} · ${rng.ready ? 'ready (RF contract)' : 'NOT READY'}`,
      'Successful hardware samples': rng.successful_samples,
      'Rejected readiness reads': rng.rejected_reads,
      'Latest 32-bit word': rng.latest_word == null ? 'No samples yet' : rng.latest_word_hex,
      'Latest word ones / zeros': rng.latest_word == null ? '—' : `${rng.ones} / ${rng.zeros}`,
      'Latest net contribution': rng.latest_word == null ? '—' : `${rng.net >= 0 ? '+' : ''}${rng.net} net bits${rng.latest_applied ? ' (applied)' : ' (not applied)'}`,
      'Current window': `${rng.window_id} · ${rng.window_samples} words · net ${rng.window_net}`,
      'Pending target movement': `${rng.pending_movement_deg.toFixed(3)}°`,
      'Last committed movement': rng.last_commit.valid ? `${rng.last_commit.movement_deg.toFixed(3)}° from ${rng.last_commit.samples} words` : 'None yet',
      'Target / smoothed hue': `${rng.target_hue.toFixed(3)}° / ${rng.smoothed_hue.toFixed(3)}°`,
      'Frame walk / logical hue': `${color.walk_hue?.toFixed(3) ?? '—'}° / ${color.logical_hue.toFixed(3)}°`,
      'Final commanded RGB': color.commanded_rgb.join(', '),
      'Frame / transmission': `${color.frame_id} · ${color.valid ? 'successful command' : 'UNKNOWN'}`,
      'RNG health': rng.health_failed ? 'LATCHED FAILURE; sampling halted' : 'No gross failure observed',
      'Three separate clocks': `${rng.sample_hz} words/s · ${rng.accumulation_ms} ms/window · ${rng.frame_hz} frames/s`
    };
    $('rng-diagnostics').replaceChildren(...Object.entries(values).flatMap(([key, value]) => {
      const dt = document.createElement('dt'), dd = document.createElement('dd');
      dt.textContent = key; dd.textContent = value; return [dt, dd];
    }));
  } else $('rng-mode').textContent = 'Live RNG diagnostics require firmware 0.2.1 or later.';
  const s = state.session;
  $('session').textContent = `${s.reason} · ${s.count} / ${s.planned} words${s.id ? ` · ${s.id}` : ''}`;
  $('lamp-fields').disabled = s.active || state.calibration.active;
  $('trial-fields').disabled = s.active || state.calibration.active || s.available || state.rng_failed || !s.storage_ready;
  for (const e of $('wifi').elements) e.disabled = s.active || state.calibration.active;
  $('recover').disabled = s.active || state.calibration.active;
  $('stop').disabled = !s.active; $('export').disabled = !s.available || s.active; $('clear').disabled = !s.available || s.active;
  $('network').textContent = `${state.network.ap_ssid} · AP: 192.168.4.1 · LAN: ${state.network.station_ip || 'not connected'} · ${state.network.hostname}.local`;
  showCalibration(state.calibration, state);
}
async function poll() {
  if (!token) return;
  if (pollPromise) return pollPromise;
  const requestedEpoch = epoch;
  pollPromise = (async () => {
    const started = performance.now();
    const state = await api('status');
    if (epoch === requestedEpoch && token) {
      colorTransitMs = performance.now() - started; // conservative full request time, no clock synchronization
      showState(state);
    }
  })();
  try { await pollPromise; } finally { pollPromise = null; }
}
async function refresh() { if (pollPromise) await pollPromise.catch(() => {}); await poll(); }
async function change(path, data) {
  ++epoch; // Do not apply a status response that predates this mutation.
  await api(path, data); await refresh();
}
function run(action) { return async event => { event?.preventDefault(); try { await action(); } catch (error) { notice(error.message, true); } }; }
$('auth').addEventListener('submit', run(async () => {
  token = $('token').value.trim(); ++epoch; await refresh(); setForm(await api('settings'));
  sessionStorage.setItem('oml-token', token); $('token').value = ''; notice('Connected locally. Changes are saved only when you press Save.');
}));
$('controls').addEventListener('submit', run(async () => {
  const data = {};
  for (const e of $('controls').elements) if (e.name) data[e.name] = e.type === 'checkbox' ? e.checked : Number(e.value);
  await change('settings', data); notice('Settings saved; walk restarted from starting hue.');
}));
$('trial').addEventListener('submit', run(async () => {
  const data = Object.fromEntries(new FormData($('trial'))); data.samples = Number(data.samples); data.client_utc = new Date().toISOString();
  await change('session/start', data); notice('Recording on the device. You may close this page.');
}));
$('stop').addEventListener('click', run(async () => { await change('session/stop', {}); notice('Trial stopped early and marked user_stopped.'); }));
$('export').addEventListener('click', run(async () => {
  const blob = await api('export'); const url = URL.createObjectURL(blob); const link = document.createElement('a');
  link.href = url; link.download = `open-mind-lamp-${current?.session.id || 'retained-trial'}.ndjson`; document.body.append(link); link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 10000);
  notice('Trial downloaded. Keep the original file for independent analysis.');
}));
$('clear').addEventListener('click', run(async () => {
  if (!confirm('Permanently delete the saved trial? Download and verify it first.')) return;
  await change('session/clear', {}); notice('Saved trial cleared.');
}));
$('wifi').addEventListener('submit', run(async () => { await change('wifi', Object.fromEntries(new FormData($('wifi')))); $('wifi').reset(); notice('Wi-Fi saved. The private AP remains available.'); }));
$('recover').addEventListener('click', run(async () => { await change('wifi/recover', {}); notice('Home Wi-Fi forgotten.'); }));
async function calibrate(action, observed) {
  if (calibrationBusy) return;
  calibrationBusy = true;
  if (current) showCalibration(current.calibration, current);
  try {
    const c = current?.calibration;
    const request = action === 'start' ? {} : { run_id: c.run_id, step: c.step, ...(observed ? { observed } : {}) };
    await change(`calibration/${action}`, request);
    if (action === 'save') { setForm(await api('settings')); notice('User-confirmed channel ordering saved. Physical spectral color is not measured.'); }
    else notice('Calibration uses your physical observations; settings remain unchanged until Save.');
  } finally {
    calibrationBusy = false;
    if (current) showCalibration(current.calibration, current);
  }
}
$('calibration-start').addEventListener('click', run(() => calibrate('start')));
$('calibration-match').addEventListener('click', run(() => calibrate('observe', current.calibration.expected)));
$('calibration-report').addEventListener('click', run(() => calibrate('observe', $('calibration-seen').value)));
$('calibration-save').addEventListener('click', run(() => calibrate('save')));
$('calibration-cancel').addEventListener('click', run(() => calibrate('cancel')));
$('logout').addEventListener('click', () => { token = ''; ++epoch; sessionStorage.removeItem('oml-token'); $('login').hidden = false; current = null; $('orb').style.backgroundColor = 'transparent'; colorAge(); notice('Browser disconnected; lamp continues. Calibration times out if left active.'); });
$('theme').addEventListener('click', () => { document.body.classList.toggle('light'); localStorage.setItem('oml-theme', document.body.classList.contains('light') ? 'light' : 'dark'); });
document.body.classList.toggle('light', localStorage.getItem('oml-theme') === 'light');
$('controls').addEventListener('input', updateOutputs); updateOutputs();
setInterval(colorAge, 500); // Local age display only; no additional API requests.
(async () => { if (token) { try { await poll(); setForm(await api('settings')); } catch (e) { notice(e.message, true); } } })();
// Single in-flight poll, including on a slow ESP32 or disconnected network.
(async function loop() { await new Promise(resolve => setTimeout(resolve, 1500)); try { await poll(); } catch (e) { notice(`Connection: ${e.message}. Lamp remains standalone.`, true); } loop(); })();
