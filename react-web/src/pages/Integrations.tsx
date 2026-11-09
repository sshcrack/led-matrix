import SpotifyConnection from '~/components/spotify/SpotifyConnection'
import EnvironmentCard from '~/components/environment/EnvironmentCard'

export default function Integrations() {
  return <div className="space-y-4">
    <div>
      <h1 className="text-xl font-semibold">Integrations</h1>
      <p className="text-sm text-muted-foreground">Connect Spotify and choose how scenes follow the real world.</p>
    </div>
    <SpotifyConnection settings />
    <EnvironmentCard />
  </div>
}
