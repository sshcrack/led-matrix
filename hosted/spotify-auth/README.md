# Spotify sign-in relay

Spotify only accepts `https://` redirect URIs (or loopback), and a matrix on your LAN has neither a public
address nor a certificate. This static page is the redirect target. It reads the matrix's address from the
OAuth `state` value (`<nonce>.<base64url(origin)>`, built by `plugins/SpotifyScenes`) and sends the browser
back to `<origin>/spotify/callback?code=…&state=…`. The browser carries the code over the local network;
the page never contacts the matrix, stores nothing, and the vhost disables access logs.

Safety: it only redirects to private/local hosts (RFC1918, `.local`, `.lan`, `.ts.net`, single-label names,
loopback, link-local, CGNAT, ULA), never to public addresses. The matrix still validates the full `state`
(one-time, 10 min) before exchanging the code.

Deployed at `https://spotify-auth.sshcrack.me/callback` (reverse proxy `pi`):

| What | Where |
|---|---|
| Page | `/var/www/spotify-auth/index.html` (copy of `index.html` here) |
| nginx vhost | `/etc/nginx/sites-available/sshcrack/spotify-auth.sshcrack.me.conf` (copy: `nginx.conf` here) |
| Certificate | certbot, `--nginx`, auto-renewed via `/etc/cron.d/certbot-maintenance` |
| DNS | proxied CNAME `spotify-auth` → `sshcrack.me` (Cloudflare; the Pi's DDNS script keeps `@` current) |

Update: `scp hosted/spotify-auth/index.html pi:/var/www/spotify-auth/`. Self-host by serving `index.html` at any
HTTPS URL and setting `LED_MATRIX_SPOTIFY_REDIRECT_URI` on the daemon.
