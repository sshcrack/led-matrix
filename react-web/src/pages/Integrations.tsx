import SpotifyConnection from '~/components/spotify/SpotifyConnection'
import EnvironmentCard from '~/components/environment/EnvironmentCard'
import HaloCard from '~/components/halo/HaloCard'
import PanelHardwareCard from '~/components/hardware/PanelHardwareCard'

export default function Integrations() {
  return <div className="space-y-4">
    <div>
      <h1 className="text-xl font-semibold">Integrations</h1>
      <p className="text-sm text-muted-foreground">Set up the panel, connect Spotify and choose how scenes follow the real world.</p>
    </div>
    <PanelHardwareCard />
    <SpotifyConnection settings />
    <EnvironmentCard />
    <HaloCard />
  </div>
}
