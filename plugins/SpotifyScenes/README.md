# Spotify Scenes Plugin

This plugin connects to the Spotify API to display your currently playing music information and visualizations on your LED Matrix.

## Features

- Display current track information (title, artist, album)
- Show album artwork on the LED matrix
- Music visualizers synchronized with your music
- Now playing status and playback controls
- Authentication with Spotify OAuth integration

## Configuration

The plugin requires Spotify API credentials and authentication.
Set your following environment variables:
```
SPOTIFY_CLIENT_ID=your_client_id
SPOTIFY_CLIENT_SECRET=your_client_secret
```

## Setup

1. Create a Spotify Developer application at [developer.spotify.com](https://developer.spotify.com/dashboard/)
2. Get your Client ID and Client Secret
3. Set the redirect URI to `http://127.0.01.:8080/spotify/callback` in your Spotify app settings
4. Complete the authentication flow to obtain a refresh token
5. Add your credentials to the configuration file

## Installation
1. Configure your Spotify API credentials
2. If this plugin is not deleted from the plugins directory, install is automatic!


## Notes
- The [getsongbpm.com](https://getsongbpm.com/api) API for beat detection is optional but recommended.
- The plugin requires an active internet connection
- A premium Spotify account might be required for some features
- The refresh token doesn't expire unless you revoke access
