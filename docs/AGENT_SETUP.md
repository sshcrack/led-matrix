# LED Matrix: agent setup guide

> **Audience: an AI coding agent** (Claude Code, Codex, Cursor, …) that has been asked to set up the LED Matrix Controller for a human.
> **Humans:** you don't need to read this. Paste the prompt from the [README](../README.md#set-up-with-an-ai-agent) into your agent.

You are setting up [led-matrix](https://github.com/sshcrack/led-matrix): a daemon on a Raspberry Pi that drives HUB75 LED panels, with a web UI on port 8080 and an optional desktop app. The product is a calm, passive ambient display.

Your job has up to eight phases (Phase 0 only if they're starting from scratch). **Do them in order. Interview first, act second, and verify everything.**

### Where you are running (the usual setup)

Most often you run on the human's **own computer** (their work or desktop PC), the same machine where they want the desktop app. From there you set up the Pi **over SSH**, then install and connect the desktop app **locally**:

```text
 this machine (you + desktop app)  ──ssh──▶  Raspberry Pi (led-matrix daemon + panels)
                                   ──TCP/UDP :8080──▶
```

- **Commands for the Pi** (Phases 2–4, 6) run through SSH: `ssh pi@<pi-host> '<command>'`, or an interactive session. `localhost:8080` in those commands means the Pi itself.
- **Commands for the desktop app** ([Phase 5](#desktop-app-on-this-machine)) run on this machine, in its own shell (bash on Linux, PowerShell on Windows). Here the API is `http://<pi-host>:8080`.
- If you run **on the Pi** instead, do everything there, and give the human the desktop app steps to run on their PC.
- The desktop app exists for **Windows x64 and Linux x86_64** only. On macOS or another architecture, skip it and say so.

| # | Phase | You | The human |
|:-:|:--|:--|:--|
| 0 | Starting point | Find out what exists; guide wiring and flashing if needed | Does the physical steps |
| 1 | Interview | Ask a few questions | Answers |
| 2 | Preflight | Inspect the Pi | – |
| 3 | Install | Run commands | Approves `sudo` if needed |
| 4 | Verify and tune | Check service, API, tune flags | **Looks at the physical panels** |
| 5 | Desktop app and integrations (optional) | Install and connect the desktop app on this machine; configure what they asked for | Looks at the app window; browser steps (Spotify login) |
| 6 | Starter content | Create presets and schedules | – |
| 7 | Hand-off | Summarise | Reads the summary |

---

## Ground rules

1. **Ask before you assume hardware.** Wrong panel numbers give a scrambled or dark display and make the person think the install failed. Ask the questions in [Phase 1](#phase-1-interview); don't guess what you can't detect.
2. **Detect, don't ask,** what you can read yourself: architecture, OS, Pi model, whether the `pi` user exists, IP address, and the OS and architecture of the machine you run on.
3. **You cannot see the LEDs.** Only the human can confirm what the panels show. After every visual change, ask what they see. Never claim the display "works" from a healthy service alone.
4. **Secrets** (Spotify secret, API token, Wi-Fi or SSH passwords):
   - Never invent one, never reuse one from another context, and never print one in a summary.
   - If your environment has a way to request a secret privately, use it. Otherwise tell the human the exact command to run themselves.
   - Don't write secrets into files in a git repository.
5. **Confirm before anything destructive or hard to reverse:** `apt purge`, deleting `/var/lib/led-matrix`, changing SSH or network config, rebooting a machine you're connected to. State what you're about to do and wait.
6. **One change at a time** when tuning. Change a flag, reconfigure, ask what they see, repeat.
7. **Don't edit `/etc/default/led-matrix` by hand.** It is regenerated from debconf on every upgrade and reconfigure, so your edit will vanish. Use `debconf-set-selections` plus `dpkg-reconfigure` ([how](#changing-settings-later)).
8. **Never** run the emulator build on the Pi, build from source on the Pi, or open port 8080 to the internet.
9. Report honestly. If a step failed or was skipped, say so with the error output.

---

## Phase 0: Where are they starting?

Before the interview, find out how far along the person is. Ask one question: **"Do you already have a Raspberry Pi running an OS that I can SSH into, with the panels wired to it?"**

| Answer | What you do |
|:--|:--|
| **Yes, all of it** | Continue with Phase 1. |
| **Pi has an OS but panels aren't wired yet** | Walk them through wiring (checklist below), wait for them to confirm, then continue. |
| **I have the parts but nothing is set up** | Walk them through the checklist below (wiring, flashing, first SSH login). They do the physical and imaging steps, you guide and verify. |
| **I have nothing yet** | Help them choose parts (README [shopping list](../README.md#shopping-list)). Offer the **emulator** so they can see the project first: `git clone --recurse-submodules https://github.com/sshcrack/led-matrix.git`, then follow the README's [Development](../README.md#development) steps and run `./scripts/run_web_emulator.sh`. Stop there until the hardware arrives. |
| **No Pi planned, just curious** | The emulator is the whole setup. Don't try to install the `.deb` anywhere that isn't an arm64 Pi. |

### From-scratch checklist (you guide, they do)

You cannot touch the hardware, so give them these in small batches and wait for a confirmation after each:

1. **Wire it** (everything powered off): adapter or HAT on the Pi header; HUB75 ribbon from the adapter to panel 1's *input*; panel 1's *output* to panel 2's *input* for chained panels; a second adapter output for a parallel chain; the 5 V supply to **every** panel's power connector; the Pi on its own supply. Details: README [Assemble and wire](../README.md#assemble-and-wire) and the upstream [wiring guide](https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/wiring.md).
2. **Flash the SD card** with Raspberry Pi Imager: *Raspberry Pi OS Lite (64-bit)*; customisation: username **`pi`**, a password, Wi-Fi (if no Ethernet), **SSH enabled**, optional hostname.
3. **First boot**: they insert the card and power on, then wait about two minutes.
4. **Find the Pi**: try `ssh pi@<hostname>.local`; otherwise they look up the IP in their router's device list. If they can't reach it, ask what the Pi's activity LED does and whether Wi-Fi credentials were entered correctly. Don't change their network settings.
5. **First login**: have them run `ssh pi@<host>` (they enter the password, you never see it), then continue with Phase 1. If you operate over SSH yourself, ask them to set up key-based login (`ssh-copy-id pi@<host>`), or ask them to run the privileged commands.

Don't skip ahead: the display can't be verified until the wiring and power are right, and an undetected wiring fault looks like a software bug.

---

## Phase 1: Interview

Ask these as **one short batch** (or two) rather than one question per message. Offer the options and recommended defaults shown. Skip questions the person already answered in their request.

### A. Hardware (required)

| # | Question | Notes and mapping |
|:-:|:--|:--|
| A1 | **How do I reach the Pi over SSH?** Hostname or IP, user (normally `pi`) and port. | Assume you SSH in from this machine (see [Where you are running](#where-you-are-running-the-usual-setup)); only if you're already on the Pi, skip this. You need `sudo` there. The installer expects a user called **`pi`** to exist. Test with `ssh -o BatchMode=yes pi@<host> true`; if that fails, ask them to run `ssh-copy-id pi@<host>`. |
| A2 | **Which panels do you have?** Size of each (e.g. 64×64, 64×32, 32×32) and how many? | → `rows` × `cols` of one panel. Ask for the product name or a photo/link if unsure. |
| A3 | **How are they arranged?** Rows and columns of panels, and how they're cabled | → `chain` (panels in series on one cable) and `parallel` (separate cables from the adapter). See the [layout table](#layout-cheat-sheet). |
| A4 | **Which adapter or HAT** connects the Pi to the panels? | Plain adapter (e.g. Electrodragon Active-3, hzeller's adapters) → default GPIO mapping. **Adafruit HAT/Bonnet** → `--led-gpio-mapping=adafruit-hat`. |
| A5 | **Which Raspberry Pi model?** | Detect it yourself (below); confirm only if it's ambiguous. |

### B. Features (optional, ask once)

| # | Question | If yes |
|:-:|:--|:--|
| B1 | Do you want **Spotify** (album art, track-aware scenes)? | The human enters credentials and signs in through the web UI. See [Spotify](#spotify). |
| B2 | Should the web UI and API be **protected with a token**? (Anyone on your network can control it otherwise.) | Ask them to choose or generate one. See [Secrets](#ground-rules). |
| B3 | **Automatic updates** from GitHub releases? (Default: yes, every 24 h; updates roll back on failure.) | `auto-update-enable` / `auto-update-interval` |
| B4 | Should I install the **desktop app on this computer**? (Music reactivity from what this PC plays, GPU shader scenes, music videos, and taking over heavy scenes from the Pi. It runs in the tray and updates itself.) Recommend **yes** when this is a Windows or Linux PC that is often on. Follow-ups if yes: start it with the computer? Video scenes too (needs `yt-dlp` and `ffmpeg`)? | See [Desktop app](#desktop-app-on-this-machine). On another machine, give them the steps instead. |
| B5 | Any **schedule** in mind (e.g. calm scenes in the evening, dim at night)? | See [Phase 6](#phase-6-starter-content). |
| B6 | If this is a **re-install or upgrade**: keep the existing presets and settings? | Default: yes. Don't purge. |

### Layout cheat sheet

Width = `cols` × `chain`. Height = `rows` × `parallel`.

| Physical layout | rows | cols | chain | parallel | Display |
|:--|:-:|:-:|:-:|:-:|:--|
| 2×2 of 64×64 panels *(project's primary target)* | 64 | 64 | 2 | 2 | 128×128 |
| 1 panel, 64×64 | 64 | 64 | 1 | 1 | 64×64 |
| 2 panels 64×32 side by side | 32 | 64 | 2 | 1 | 128×32 |
| 4 panels 32×32 in a row | 32 | 32 | 4 | 1 | 128×32 |
| 2×2 of 64×32 panels (two chains of two) | 32 | 64 | 2 | 2 | 128×64 |

If the person isn't sure which panels are on which chain, start with `chain` = total panels, `parallel` = 1 and adjust after you see the result. The first physical check will reveal it.

### Defaults you can propose

| Setting | Default | Reason |
|:--|:--|:--|
| GPIO slowdown | `3` for Pi 3, `2` for Pi 4 | From the installer's own recommendation. Raise it if there's flicker. |
| Brightness | leave unset (100) | Suggest `--led-brightness=60` if the person is sitting close to a bright panel or has a small power supply. |
| Auto-update | on, 24 h | Safe: transactional with rollback. |

---

## Phase 2: Preflight

Run these on the Pi (via SSH) and check each result.

```bash
uname -m                                   # must be aarch64 (arm64). If armv7l/armhf: STOP, a 64-bit OS is required.
tr -d '\0' < /proc/device-tree/model; echo # Pi model. "Raspberry Pi 5" -> STOP: unsupported by the LED driver library.
. /etc/os-release && echo "$PRETTY_NAME"   # Debian 12 (Bookworm) or newer; Raspberry Pi OS 64-bit is ideal.
id pi                                      # user "pi" must exist. If not, ask the human before creating it (sudo adduser pi).
sudo -n true && echo "sudo ok"             # passwordless sudo? If not, ask the human to run privileged steps or enter the password.
df -h / | tail -1                          # need roughly 1 GB free (the package is about 165 MB).
curl -fsS -o /dev/null -w "github: %{http_code}\n" https://api.github.com/repos/sshcrack/led-matrix/releases/latest
systemctl is-active led-matrix 2>/dev/null || echo "not installed yet"
ss -ltn | grep ':8080 ' || echo "port 8080 free"
```

On **this machine**, if the human wants the desktop app, also check the platform (Linux: `uname -sm` must be `Linux x86_64`; Windows: `[Environment]::Is64BitOperatingSystem` and an x64 CPU) and that it can reach the Pi: `ping -c 1 <pi-host>` (Windows: `Test-Connection <pi-host> -Count 1`).

**Stop and ask** if: the OS is 32-bit, the Pi is a model 5, GitHub is unreachable, port 8080 is used by something else (the daemon honours a `PORT` environment variable, see [Configuration](#changing-settings-later)), or an existing install is present and the human hasn't said whether to keep it.

**One-time hardware prep.** The Pi's onboard sound conflicts with the LED driver. Ask permission, then:

```bash
echo "blacklist snd_bcm2835" | sudo tee /etc/modprobe.d/blacklist-rgb-matrix.conf
CFG=/boot/firmware/config.txt; [ -f "$CFG" ] || CFG=/boot/config.txt
sudo sed -i 's/^dtparam=audio=on/dtparam=audio=off/' "$CFG"
```

A reboot is needed for this to take effect. Tell the human, and do it at the end of Phase 3 with their consent.

---

## Phase 3: Install

Use the package directly with pre-seeded answers. This is fully non-interactive and doesn't need a terminal.

### 3.1 Pre-seed the configuration

Substitute the values from Phase 1. Leave unused keys empty.

```bash
sudo debconf-set-selections <<'EOF'
led-matrix led-matrix/led-rows string 64
led-matrix led-matrix/led-cols string 64
led-matrix led-matrix/led-chain string 2
led-matrix led-matrix/led-parallel string 2
led-matrix led-matrix/led-slowdown-gpio string 3
led-matrix led-matrix/extra-opts string
led-matrix led-matrix/spotify-enable boolean false
led-matrix led-matrix/auto-update-enable boolean true
led-matrix led-matrix/auto-update-interval select 24
EOF
```

Secrets go in separately, so they don't land in your logs. Ask the human to run this themselves, or use your private-secret mechanism:

```bash
# Optional. Run by the human, or through a private secret channel. Don't echo the values.
sudo debconf-set-selections <<< "led-matrix led-matrix/api-token password <TOKEN>"
```

For an Adafruit HAT or other flags, put them in `extra-opts`, e.g. `--led-gpio-mapping=adafruit-hat --led-brightness=60`.

### 3.2 Download and install the latest release

```bash
cd "$(mktemp -d)"
URL=$(curl -fsS https://api.github.com/repos/sshcrack/led-matrix/releases/latest \
  | grep browser_download_url | grep -E 'led-matrix-[0-9]+\.[0-9]+\.[0-9]+_arm64\.deb' | cut -d'"' -f4 | head -n1)
echo "$URL"; [ -n "$URL" ] || { echo "no arm64 .deb found"; exit 1; }
curl -fL -o led-matrix.deb "$URL"
dpkg-deb -f led-matrix.deb Package Version Architecture     # expect: led-matrix, x.y.z, arm64
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y ./led-matrix.deb
```

`apt-get install ./file.deb` resolves the package's dependencies (`jq`, `curl`, `debconf`, …) for you.

Alternative (interactive, needs a real terminal for the human): `curl -fsSLO https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/install_led_matrix.sh && bash install_led_matrix.sh`.

### 3.3 What the package did

| Thing | Where |
|:--|:--|
| Daemon, plugins, web UI | `/usr/bin/led-matrix`, `/usr/lib/led-matrix/`, `/usr/share/led-matrix/` |
| Runtime flags and secrets (generated) | `/etc/default/led-matrix` (mode 600) |
| Config, presets, uploads, crash reports | `/var/lib/led-matrix/` (owned by `pi`) |
| Service | `led-matrix.service` (starts as root, drops to `pi:pi` after GPIO init), plus `led-matrix-update.path` |

Confirm the flags were applied as intended (this file may contain secrets, so don't paste it into chat unredacted):

```bash
sudo grep '^MATRIX_OPTS' /etc/default/led-matrix
```

---

## Phase 4: Verify and tune

### 4.1 Software checks

```bash
systemctl is-active led-matrix            # active
journalctl -u led-matrix -n 40 --no-pager # no repeated errors/crash loops
curl -fsS localhost:8080/status           # {"turned_on":true,"operation_mode":"automatic",...}
curl -fsS localhost:8080/list_scenes | jq 'length'   # roughly 60+ scenes
```

If you set an API token, add `-H "Authorization: Bearer $LED_MATRIX_API_TOKEN"` (the human must supply it; `/status` is protected too).

If the service is not active, read the full log (`journalctl -u led-matrix -n 200 --no-pager`) before changing anything. Typical causes: wrong `--led-*` values, permission problems, the port already in use.

### 4.2 Visual check (the human looks)

Create a test preset with the `clock` scene. Digits make mirroring, rotation and panel order obvious.

```bash
curl -fsS -X POST "localhost:8080/add_preset?id=setup-test" -H 'Content-Type: application/json' -d '{
  "display_name":"Setup test","transition_duration":0,"transition_name":"blend",
  "scenes":[{"type":"clock","arguments":{"duration":600000}}]}'
curl -fsS "localhost:8080/operation_mode?mode=manual"
curl -fsS "localhost:8080/set_active?id=setup-test"
curl -fsS "localhost:8080/set_enabled?enabled=true"
```

Then ask: **"What do you see on the panels? Is the clock readable, upright, complete and the right colours?"** Use their answer in the tree below. Afterwards switch back with `operation_mode?mode=automatic` and delete the test preset (play a different preset first; the active one can't be deleted).

### 4.3 Tuning decision tree

Apply **one** change, [reconfigure](#changing-settings-later), and ask again.

| What they see | Likely cause | Change |
|:--|:--|:--|
| Everything dark, service healthy | Panel init or power | Check 5 V power and ribbon cable direction. Then add `--led-panel-type=FM6126A` to `extra-opts`. |
| Flicker, noise or shimmering | GPIO timing | Raise `led-slowdown-gpio` by 1 (up to 4). Confirm the sound-disable step. |
| Colours swapped (red↔green etc.) | RGB order | `--led-rgb-sequence=RBG` (then `BGR`, `GRB`, `GBR`, `BRG`). |
| Scrambled, striped or half the rows | Multiplexing | `--led-multiplexing=N` (try 1–8, up to 17), then `--led-row-addr-type=N` (1–4). |
| Clock split or in pieces, wrong order | `chain` and `parallel` wrong, or panels cabled differently | Re-check A3. Swap `chain` and `parallel`, or try `--led-pixel-mapper="U-mapper"`. |
| Upside down or rotated | Mounting | `--led-pixel-mapper="Rotate:180"` (or `90`, `270`). |
| Only part of the display used | Wrong `rows`, `cols`, `chain` or `parallel` | Recompute from the [cheat sheet](#layout-cheat-sheet). |
| Too bright or hot | Power | `--led-brightness=50`. |
| Looks right but flickers on camera | PWM refresh | `--led-pwm-bits=9` or `--led-limit-refresh=120`. |

Flags are the upstream [rpi-rgb-led-matrix](https://github.com/hzeller/rpi-rgb-led-matrix) options. Consult its README for anything exotic.

### Changing settings later

```bash
# 1. Update the stored answers (examples)
echo "led-matrix led-matrix/led-slowdown-gpio string 4" | sudo debconf-set-selections
echo "led-matrix led-matrix/extra-opts string --led-rgb-sequence=RBG --led-brightness=60" | sudo debconf-set-selections
# 2. Regenerate /etc/default/led-matrix and restart the service
sudo DEBIAN_FRONTEND=noninteractive dpkg-reconfigure led-matrix
# 3. Confirm
sudo grep '^MATRIX_OPTS' /etc/default/led-matrix; systemctl is-active led-matrix
```

`extra-opts` **replaces** the previous value, so repeat every extra flag you still want.

For environment variables that aren't in debconf (`PORT`, `SHADERTOY_API_KEY`, `LED_MATRIX_ALLOWED_ORIGINS`, `SPDLOG_LEVEL`), use a systemd override, which survives upgrades:

```bash
sudo systemctl edit led-matrix       # add:  [Service]\n Environment=PORT=8090
sudo systemctl restart led-matrix
```

**Stop condition:** if you have iterated on one symptom about five times without improvement, stop. Summarise what you tried and what you saw, and ask the human whether to continue, check the wiring, or open an issue.

---

## Phase 5: Desktop app and integrations (only what they asked for)

### Spotify

The human completes the web UI’s **Connect Spotify** guide. Leave `spotify-enable` false during installation to defer credentials to this guide; the installed plugin remains available.

1. Open **Integrations** (on phones: **More → Integrations**) and follow the **Connect Spotify** guide. The human creates the developer app, registers the exact redirect URI shown, and enters the Client ID directly in the UI. New sign-ins use PKCE and need no Client secret.
2. Choose **Save Client ID**, then **Connect Spotify**, and approve access in the new tab. The hosted callback redirects the browser to the matrix UI. If that fails, the human pastes the full callback address into **Not sent back to your matrix?** and chooses **Finish connecting**. Keep that temporary authorization code out of chat and logs.
3. Verify `GET /spotify/status` reports `configured: true` and `connected: true` (include API authentication when configured). Then ask the human to play a track and verify the cover appears.

Use the [Spotify plugin guide](../plugins/SpotifyScenes/README.md) for the redirect URI, self-hosting, account requirements, credential precedence and expired attempts. Legacy secret-based logins remain supported until reconnecting switches them to PKCE.

### API token

Set `led-matrix/api-token` via debconf and reconfigure. The web UI prompts for the token once and remembers it. Scripts send `Authorization: Bearer <token>` or `X-Api-Token`. The desktop app has an *API token* field. Don't expose port 8080 beyond the LAN; recommend a VPN or SSH tunnel for remote access.

### Desktop app (on this machine)

Do this when the human said yes to B4 and you run on their Windows or Linux PC. The Pi must already be installed and healthy (Phase 4). Do the steps in order: the app saves its config file when it exits, so the file is written while the app is **not** running.

**1. Find an address this machine can use for the Pi.** Prefer the Pi's IP (`ssh pi@<host> hostname -I`, first entry) over a `.local` name, which some networks don't resolve. Suggest a DHCP reservation in their router so the IP doesn't change. Check it from here:

```bash
curl -fsS http://<pi-ip>:8080/status          # Windows PowerShell: curl.exe -fsS http://<pi-ip>:8080/status
```

With an API token, add `-H "Authorization: Bearer <token>"` (from your private secret channel; don't echo it). If this fails while it works on the Pi, a firewall or client isolation (guest Wi-Fi) is blocking it: tell the human. The app only makes outgoing connections to the Pi (TCP and UDP port 8080), so this machine needs no inbound firewall rule.

**2. Close the app if it is already running.** Ask first, then on Linux `pkill -x led-matrix-desktop`, on Windows `Stop-Process -Name led-matrix-desktop -ErrorAction SilentlyContinue`.

**3. Write the connection settings.** The file is `~/.config/led-matrix-desktop/config.json` on Linux and `%APPDATA%\led-matrix-desktop\config.json` on Windows (Roaming AppData, not Local). Merge into an existing file; don't drop other keys or `pluginSettings`. Set `turnMatrixOnOnStart` and `turnMatrixOffOnExit` to `false` (the app's defaults are `true`, which switches the display off whenever the PC shuts down) unless the human wants the PC to control the display's power. Leave `apiToken` empty when no token is set.

Linux:

```bash
PI_IP=192.168.1.50     # the Pi's IP from step 1
CFG=~/.config/led-matrix-desktop/config.json
mkdir -p "${CFG%/*}"; [ -s "$CFG" ] || echo '{}' > "$CFG"
python3 - "$CFG" "$PI_IP" 8080 <<'PY'
import json, sys, os
path, host, port = sys.argv[1], sys.argv[2], int(sys.argv[3])
cfg = json.load(open(path))
g = cfg.setdefault("general", {})
g.update(hostname=host, port=port, turnMatrixOnOnStart=False, turnMatrixOffOnExit=False)
g["apiToken"] = os.environ.get("LED_MATRIX_API_TOKEN", g.get("apiToken", ""))
cfg.setdefault("pluginSettings", {})
json.dump(cfg, open(path, "w"), indent=4)
PY
```

Windows (PowerShell):

```powershell
$PiIp = '192.168.1.50'   # the Pi's IP from step 1
$dir = Join-Path $env:APPDATA 'led-matrix-desktop'; $cfgPath = Join-Path $dir 'config.json'
New-Item -ItemType Directory -Force $dir | Out-Null
$cfg = if (Test-Path $cfgPath) { Get-Content $cfgPath -Raw | ConvertFrom-Json } else { [pscustomobject]@{} }
if (-not $cfg.general) { $cfg | Add-Member general ([pscustomobject]@{}) }
if (-not $cfg.pluginSettings) { $cfg | Add-Member pluginSettings ([pscustomobject]@{}) }
$set = @{ hostname = $PiIp; port = 8080; turnMatrixOnOnStart = $false; turnMatrixOffOnExit = $false;
          apiToken = $(if ($env:LED_MATRIX_API_TOKEN) { $env:LED_MATRIX_API_TOKEN } elseif ($cfg.general.apiToken) { $cfg.general.apiToken } else { '' }) }
foreach ($k in $set.Keys) { $cfg.general | Add-Member -Force $k $set[$k] }
$cfg | ConvertTo-Json -Depth 20 | Set-Content -Encoding utf8 $cfgPath
```

To pass a token, put it in `LED_MATRIX_API_TOKEN` for that one command through your private secret mechanism. Never write it into chat or a repository. The config file stores it in plain text in the user's profile, which is expected.

**4. Install.** Both installers are per-user (no `sudo`, no admin) and replace an older `.deb` or admin install on the app's first update. Don't use the desktop `.deb`.

- **Linux:** `curl -fsSL https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/install-desktop.sh | sh`. It installs to `~/.local/share/led-matrix-desktop/` and adds a launcher entry and `~/.local/bin/led-matrix-desktop`. It does not start the app.
- **Windows:** `irm https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/install-desktop.ps1 | iex`. It runs the setup, which installs to `%LOCALAPPDATA%\led-matrix-desktop\` and **starts the app**. It shows a short progress window and asks nothing.

**5. Start it** (Linux only; Windows already started it). It needs the graphical session of the logged-in user, so run it from a terminal of that session, not from an SSH login without a display:

```bash
setsid -f ~/.local/share/led-matrix-desktop/current/bin/led-matrix-desktop >/dev/null 2>&1
```

The app follows the matrix's version. If the Pi runs a different release, the app may update itself and restart within the first minute. That is expected.

**6. Verify the connection.** Wait about 15 seconds, then from this machine:

```bash
curl -fsS http://<pi-ip>:8080/diagnostics | jq '{desktop_connections, worker: .remote_render | {worker_available, worker_compatible}, audio_fresh: .audio.fresh}'
```

| Field | Expect |
|:--|:--|
| `desktop_connections` | `1` or more. `0` means the app isn't connected: check the address, port and token in the config, and that the app is running. |
| `worker_available`, `worker_compatible` | `true` after up to a minute (the scene worker for offload). `worker_compatible: false` usually means the app is still updating to the Pi's version. |
| `audio_fresh` | `true` while this PC plays audio. Ask the human to play some music, then check again. |

Without `jq`, read the same fields from the plain JSON. Also ask the human to open the app from the tray: the status must read *WebSocket is currently: Open*.

**7. Start with the computer** (if they said yes): ask the human to tick **Start with System** in the app's *LED Matrix Controller* menu. The app writes the right autostart entry itself (it starts minimised to the tray).

**8. Video scenes** (optional, only if they want them): the `video` and `spotifymv` scenes need `ffmpeg` and `yt-dlp` on this machine. Installing system packages needs their consent (and `sudo` on Linux).

- **Linux:** `sudo apt install ffmpeg` (or the distro's equivalent). Distro `yt-dlp` packages go stale quickly; install the current release instead: `curl -fL https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o ~/.local/bin/yt-dlp && chmod +x ~/.local/bin/yt-dlp`. Then set `"ytdlpPath"` in the `general` section of the config to that absolute path (apps started from the launcher may not have `~/.local/bin` on `PATH`), with the app closed as in steps 2–3.
- **Windows:** `winget install --id yt-dlp.yt-dlp -e` and `winget install --id Gyan.FFmpeg -e`. Quit and restart the app afterwards so it sees the new `PATH`.

If a tool is still missing, the video scenes' panel in the app names it (for example *ffmpeg not found in PATH*).

**If you are not on the PC** (you run on the Pi, or the PC runs macOS): give the human the install command for their OS from the [README](../README.md#desktop-app), tell them to enter the Pi's IP, port `8080` and the token in the app, and click **Connect**. Then verify `desktop_connections` from the Pi with `curl -s localhost:8080/diagnostics | jq .desktop_connections`.

---

## Phase 6: Starter content

The Pi works out of the box. **Automatic Mode** (the default) picks from curated looks on its own. Offer to add a bit more.

```bash
# Three ready-made ambiance presets: quiet-evening, rainy-window, deep-space
for p in quiet-evening rainy-window deep-space; do curl -fsS -X POST "localhost:8080/ambiance_packs?id=$p"; echo; done
curl -fsS localhost:8080/list_presets | jq 'to_entries[] | {id: .key, name: .value.display_name}'
```

**Schedule** (example: Quiet Evening every day 18:00–23:00; use a real preset ID from the list above):

```bash
curl -fsS -X POST localhost:8080/schedule -d '{"name":"Evening","preset_id":"<PRESET_ID>",
  "start_hour":18,"start_minute":0,"end_hour":23,"end_minute":0,"days_of_week":[0,1,2,3,4,5,6],"enabled":true}'
# Only after the person has read the scheduling note below:
curl -fsS -X POST localhost:8080/scheduling_status -d '{"enabled":true}'
```

**Night dimming:**

```bash
curl -fsS -X POST localhost:8080/brightness_schedule -d '{"enabled":true,"day_brightness":100,"night_brightness":25,
  "night_start_hour":22,"night_start_minute":0,"night_end_hour":7,"night_end_minute":0,"fade_minutes":20}'
```

**How scheduling behaves (tell the person before enabling it):** while *Auto Scheduling* is on, the schedule in force **overrides Automatic Mode**, and outside every schedule window the display **turns off**. If they only want dimming at night, use the brightness schedule and leave Auto Scheduling off. If they want scheduled presets, make the windows cover every hour they want lit. Check the result with `curl -s localhost:8080/status` (`turned_off`, `current`).

Finish by setting the mode the person prefers: `curl -fsS "localhost:8080/operation_mode?mode=automatic"` (the Director picks scenes) or `mode=manual` (their presets play).

---

## Phase 7: Hand-off

Finish with a short summary like this. **Don't include secrets.**

```text
LED Matrix is set up.

Display:        128×128 (4 × 64×64 panels, rows=64 cols=64 chain=2 parallel=2)
Flags:          --led-slowdown-gpio=3 (+ any extras)
Web UI:         http://<pi-ip>:8080/   (token: set / not set)
Mode:           Automatic (Director picks scenes)
Service:        led-matrix.service, enabled at boot
Updates:        automatic every 24 h, with rollback
Desktop app:    <installed on this PC & connected, starts with the computer: yes/no / skipped>
Integrations:   Spotify: <done / skipped>   Video tools: <installed / skipped>
Presets/schedules created: <list>

Useful commands:
  journalctl -u led-matrix -f
  sudo dpkg-reconfigure led-matrix      # change panel layout, Spotify, token, updates
  sudo apt remove led-matrix            # uninstall (keeps your data)

Not verified by me: anything you must see with your own eyes. Tell me if colours/orientation look off.
```

Mention anything left undone, any assumption you made (for example *"I assumed `chain=2 parallel=2`"*) and what to try if the display looks wrong.

---

## Reference

### debconf keys

| Key | Type | Meaning |
|:--|:--|:--|
| `led-matrix/led-rows` | string | Rows per panel (`--led-rows`) |
| `led-matrix/led-cols` | string | Columns per panel (`--led-cols`) |
| `led-matrix/led-chain` | string | Panels per chain (`--led-chain`) |
| `led-matrix/led-parallel` | string | Parallel chains (`--led-parallel`) |
| `led-matrix/led-slowdown-gpio` | string | GPIO slowdown; blank = library default |
| `led-matrix/extra-opts` | string | Extra flags, appended verbatim |
| `led-matrix/spotify-enable` | boolean | Write Spotify credentials to the env file |
| `led-matrix/spotify-client-id` | string | Spotify Client ID |
| `led-matrix/spotify-client-secret` | password | Spotify Client Secret |
| `led-matrix/api-token` | password | Require this token for the web UI and API |
| `led-matrix/auto-update-enable` | boolean | Automatic updates |
| `led-matrix/auto-update-interval` | select | Hours between checks: `6, 12, 24, 48, 168` |

Inspect stored answers with `sudo debconf-get-selections | grep led-matrix` (needs `debconf-utils`; the output may include secrets, so don't paste it into chat).

### Handy commands

```bash
systemctl status led-matrix                 # service state
journalctl -u led-matrix -f                 # live logs
sudo systemctl restart led-matrix
curl -s localhost:8080/diagnostics | jq     # renderer FPS, per-scene timing, desktop link, audio, Director
curl -s localhost:8080/diagnostics | jq '{desktop_connections, remote_render, audio_fresh: .audio.fresh}'   # desktop app link
ls /var/lib/led-matrix/crashes/             # crash reports (ask the human to attach one to an issue)
cat /var/log/led-matrix-update.log          # updater log
```

### API quick reference

`GET /status` · `GET /list_presets` · `GET /set_active?id=` · `GET /operation_mode?mode=automatic|manual` · `GET /set_enabled?enabled=true|false` · `GET /skip` · `GET /list_scenes` · `POST /add_preset[?id=]` · `POST /ambiance_packs?id=` · `POST /schedule` · `POST /scheduling_status` · `POST /brightness_schedule` · `GET /diagnostics`.
The full table is in the [README](../README.md#rest-api).

### Upgrade, rollback, uninstall

```bash
# Upgrade (keeps config): repeat 3.2 with the new release. Presets and settings are preserved.
# The built-in updater (Updates page or POST /api/update/install) does this transactionally and rolls back on failure.
sudo apt remove led-matrix      # remove, keep /var/lib/led-matrix and the stored answers
sudo apt purge led-matrix       # ALSO deletes /var/lib/led-matrix. Destructive: confirm with the human first
```

### When to stop and hand back to the human

- The OS is 32-bit, or the Pi is a model 5.
- Wiring or power problems you can't diagnose from software (no light at all after the panel-type and power checks).
- You've failed the same step several times.
- Anything that would expose the API beyond their network or change their SSH, firewall or network settings.

Explain what you found and what you'd try next, and let them decide.
