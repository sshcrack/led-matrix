# Spotify Scenes Plugin

Shows the currently playing Spotify album artwork and supplies playback information to music-aware scenes and the director.

## Setup

Open the matrix web UI. The Spotify setup guide appears automatically when the plugin is installed but not connected. No terminal access or daemon restart is needed. Choose **Dismiss setup** if you do not use Spotify yet; this browser remembers the dismissal. Reopen it any time from **Integrations** in the desktop navigation, or **More → Integrations** on a phone.

1. Open the [Spotify developer dashboard](https://developer.spotify.com/dashboard) from the guide and create an app with Web API access.
2. Add the exact redirect URI shown in the guide: `http://127.0.0.1:8080/spotify/callback`. Save the app settings.
3. Copy the app’s Client ID and Client secret into the guide and choose **Save credentials**.
4. Choose **Connect Spotify** and approve access in the new tab.
5. When accessing a Pi from another computer or phone, the returned loopback page usually cannot load. This is expected: copy the **full address** from that tab, return to the matrix UI, expand **Browser says it cannot connect?**, paste the address and choose **Finish connecting**. Keep the address private; it contains a temporary authorization code. On the same computer with the daemon on port 8080, the callback finishes automatically.
6. Play music in Spotify. The setup row shows the connection status; expand **Settings** to reconnect or update credentials later.

Spotify permits HTTP redirects only for explicit loopback addresses, so `http://ledmat:8080` and `localhost` cannot replace the supplied redirect URI. See [redirect URI requirements](https://developer.spotify.com/documentation/web-api/concepts/redirect_uri). Development mode requires the app owner to have Premium; other accounts must be added under User Management. See [quota modes](https://developer.spotify.com/documentation/web-api/concepts/quota-modes).

Credentials and tokens are saved in the daemon’s `config.json` (in `/var/lib/led-matrix` for a DEB install). Existing `SPOTIFY_CLIENT_ID` and `SPOTIFY_CLIENT_SECRET` environment settings still work; credentials saved in the UI take precedence. The secret is never returned to the browser. Changing app credentials clears the old login, so connect again afterwards. A blank secret preserves the saved secret.

A connection attempt expires after ten minutes and starting a new attempt replaces the previous one. If Spotify rejects a login, check the credentials and exact redirect URI and choose **Connect Spotify** again. Setup never blocks daemon startup.
