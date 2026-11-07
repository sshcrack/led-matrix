# Spotify Scenes Plugin

Shows the currently playing Spotify album artwork and supplies playback information to music-aware scenes and the director.

## Setup

Open the matrix web UI. The Spotify setup guide appears automatically when the plugin is installed but not connected. No terminal access or daemon restart is needed. Choose **Dismiss setup** if you do not use Spotify yet; this browser remembers the dismissal. Reopen it any time from **Integrations** in the desktop navigation, or **More → Integrations** on a phone.

1. Open the [Spotify developer dashboard](https://developer.spotify.com/dashboard) from the guide and create an app with Web API access.
2. Add the exact redirect URI shown in the guide: `https://spotify-auth.sshcrack.me/callback`. Save the app settings.
3. Copy the app’s Client ID into the guide and choose **Save Client ID**. No Client secret is needed: sign-in uses PKCE (the Pi keeps a one-time verifier and only sends its SHA-256 to Spotify).
4. Choose **Connect Spotify** and approve access in the new tab.
5. After you approve Spotify, `spotify-auth.sshcrack.me` (a static page, see `hosted/spotify-auth/`) sends your browser back to the address you used to open the matrix UI, with the one-time code. It only redirects to private/local-network addresses, stores nothing and does not log requests. If you are not sent back, copy the **full address** of that page, expand **Not sent back to your matrix?**, paste it and choose **Finish connecting**. Self-hosting the page? Set `LED_MATRIX_SPOTIFY_REDIRECT_URI` to your copy's URL.
6. Play music in Spotify. The setup row shows the connection status; expand **Settings** to reconnect or update credentials later.

Spotify permits HTTP redirects only for explicit loopback addresses, so `http://ledmat:8080` and `localhost` cannot replace the supplied redirect URI. See [redirect URI requirements](https://developer.spotify.com/documentation/web-api/concepts/redirect_uri). Development mode requires the app owner to have Premium; other accounts must be added under User Management. See [quota modes](https://developer.spotify.com/documentation/web-api/concepts/quota-modes).

The Client ID and tokens are saved in the daemon’s `config.json` (in `/var/lib/led-matrix` for a DEB install). `SPOTIFY_CLIENT_ID` still works as a default; a Client ID saved in the UI takes precedence. Installs that connected before PKCE keep refreshing with their saved Client secret until you reconnect; reconnecting switches to PKCE and drops the stored secret. Changing the Client ID clears the old login, so connect again afterwards.

A connection attempt expires after ten minutes and starting a new attempt replaces the previous one. If Spotify rejects a login, check the credentials and exact redirect URI and choose **Connect Spotify** again. Setup never blocks daemon startup.
