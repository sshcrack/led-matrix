#pragma once

// The recovery web page (served at / in recovery mode). Self-contained: the
// normal web UI lives on the data partition's version or may be missing.
namespace Recovery {
    inline constexpr const char *page_html = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Panel recovery</title>
<style>
  :root { color-scheme: dark; --bg:#101114; --card:#1a1c21; --line:#2c2f36; --text:#e8e8ea; --dim:#9a9ca3;
          --amber:#ffa733; --red:#ff5a5a; --blue:#5b9bff; --green:#4cc38a; }
  * { box-sizing: border-box; }
  body { margin:0; background:var(--bg); color:var(--text); font:16px/1.5 system-ui, sans-serif; }
  main { max-width:640px; margin:0 auto; padding:24px 16px 48px; }
  h1 { font-size:22px; margin:0 0 4px; }
  .dim { color:var(--dim); font-size:14px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:12px; padding:16px; margin-top:16px; }
  .state { display:flex; gap:12px; align-items:flex-start; }
  .dot { width:12px; height:12px; border-radius:50%; margin-top:7px; flex:none; background:var(--amber); }
  .dot.crash { background:var(--red); } .dot.ok { background:var(--green); }
  button { font:inherit; min-height:44px; padding:8px 16px; border-radius:8px; border:1px solid var(--line);
           background:#262931; color:var(--text); cursor:pointer; }
  button.primary { background:var(--blue); border-color:var(--blue); color:#fff; }
  button.danger { border-color:var(--red); color:var(--red); }
  button:disabled { opacity:.45; cursor:not-allowed; }
  .actions { display:grid; gap:12px; }
  .action p { margin:4px 0 8px; color:var(--dim); font-size:14px; }
  progress { width:100%; height:10px; }
  #message.failed { color:var(--red); }
  input[type=file], input[type=password] { font:inherit; font-size:16px; width:100%; }
  a { color:var(--blue); }
</style>
</head>
<body>
<main>
  <h1>Panel recovery</h1>
  <div class="dim" id="version">Connecting…</div>

  <section class="card" id="token-card" hidden>
    <p>This panel is protected with an API token.</p>
    <input type="password" id="token" placeholder="API token" autocomplete="current-password">
    <p><button class="primary" id="token-save">Continue</button></p>
  </section>

  <section class="card state" id="state-card" hidden>
    <span class="dot" id="dot"></span>
    <div><strong id="title"></strong><div id="description"></div></div>
  </section>

  <section class="card" id="progress-card" hidden>
    <div id="message"></div>
    <progress id="progress" max="100"></progress>
  </section>

  <section class="card actions" id="actions" hidden>
    <div class="action" data-needs="card">
      <strong>Repair storage</strong>
      <p>Checks the data partition and fixes errors (e2fsck), then starts the panel. Keeps everything.</p>
      <button class="primary" data-action="repair">Repair storage</button>
    </div>
    <div class="action" data-needs="card">
      <strong>Reset storage</strong>
      <p>Copies settings and uploads off the data partition where it can, formats it and puts them back.
         Use it when repair fails. Anything that cannot be read is lost.</p>
      <button class="danger" data-action="reset">Reset storage…</button>
    </div>
    <div class="action" data-needs="storage">
      <strong>Install latest version</strong>
      <p>Downloads the latest release (SHA-256 checked) onto the data partition and starts it.</p>
      <button data-install>Install latest version</button>
    </div>
    <div class="action" data-needs="storage">
      <strong>Upload config backup</strong>
      <p>Replaces <code>config.json</code> with a backup (for example one downloaded from the normal web UI).</p>
      <input type="file" id="config-file" accept=".json,application/json">
      <p><button data-upload>Upload</button></p>
    </div>
    <div class="action">
      <strong>Try again</strong>
      <p>Leaves recovery mode and starts the panel normally.</p>
      <button data-action="retry">Try again</button>
    </div>
    <div class="action">
      <strong>Logs</strong>
      <p><a href="api/recovery/logs" download="led-matrix-logs.txt">Download logs</a></p>
    </div>
  </section>
</main>
<script>
const $ = (id) => document.getElementById(id);
let status = null;

async function api(path, options = {}) {
  const response = await fetch(path, { credentials: 'same-origin', ...options });
  if (response.status === 401) { $('token-card').hidden = false; throw new Error('token'); }
  const body = await response.json().catch(() => ({}));
  if (!response.ok) throw new Error(body.error || body.message || response.statusText);
  return body;
}

function render() {
  if (!status) return;
  $('version').textContent = `led-matrix ${status.version} · recovery mode`;
  $('state-card').hidden = false;
  $('actions').hidden = false;
  $('title').textContent = status.storage.ok ? 'Storage is fine' : 'Storage problem';
  if (status.reason === 'crash') $('title').textContent = 'The panel app keeps stopping';
  $('dot').className = 'dot ' + (status.reason === 'crash' ? 'crash' : status.storage.ok ? 'ok' : '');
  $('description').textContent = status.description;
  const action = status.action;
  const busy = !!(action && action.state === 'running') || status.installing;
  $('progress-card').hidden = !action && !status.installing;
  if (status.installing) {
    $('message').textContent = status.install_message || 'Installing the latest version…';
    $('message').className = '';
    $('progress').removeAttribute('value');
  } else if (action) {
    $('message').textContent = action.message;
    $('message').className = action.state === 'failed' ? 'failed' : '';
    if (action.progress >= 0) $('progress').value = action.progress; else $('progress').removeAttribute('value');
  }
  for (const element of document.querySelectorAll('.action')) {
    const needs = element.dataset.needs;
    const allowed = !needs || (needs === 'card' ? status.storage.panel_card : status.storage.ok);
    element.hidden = needs === 'card' && !status.storage.panel_card;
    for (const button of element.querySelectorAll('button')) button.disabled = busy || !allowed;
  }
}

async function refresh() {
  try { status = await api('api/recovery/status'); render(); }
  catch (error) { if (error.message !== 'token') $('version').textContent = 'Waiting for the panel…'; }
}

document.addEventListener('click', async (event) => {
  const button = event.target.closest('button');
  if (!button) return;
  try {
    if (button.dataset.action) {
      if (button.dataset.action === 'reset' &&
          !confirm('Format the data partition? Settings and uploads that can still be read are kept; everything else is lost.')) return;
      await api('api/recovery/action?name=' + button.dataset.action, { method: 'POST' });
    } else if (button.hasAttribute('data-install')) {
      await api('api/recovery/install', { method: 'POST' });
    } else if (button.hasAttribute('data-upload')) {
      const file = $('config-file').files[0];
      if (!file) return alert('Choose a config.json first.');
      await api('api/recovery/config', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: await file.text() });
      alert('Config uploaded. Use "Try again" to start the panel with it.');
    } else if (button.id === 'token-save') {
      document.cookie = 'led_matrix_token=' + encodeURIComponent($('token').value) + '; path=/; SameSite=Strict';
      $('token-card').hidden = true;
    }
  } catch (error) { if (error.message !== 'token') alert(error.message); }
  refresh();
});

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)HTML";
}  // namespace Recovery
