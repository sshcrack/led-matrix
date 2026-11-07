import SpotifyConnection from '~/components/spotify/SpotifyConnection'

export default function Integrations() {
  return <div className="space-y-4">
    <div>
      <h1 className="text-xl font-semibold">Integrations</h1>
      <p className="text-sm text-muted-foreground">Connect Spotify now or come back whenever you’re ready.</p>
    </div>
    <SpotifyConnection settings />
  </div>
}
