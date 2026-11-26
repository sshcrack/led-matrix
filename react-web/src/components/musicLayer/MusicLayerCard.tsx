import { useState } from 'react'
import { AudioLines } from 'lucide-react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Card, CardContent } from '~/components/ui/card'
import { Switch } from '~/components/ui/switch'

/** Matches the daemon's GET/POST /music_layer. */
export interface MusicLayerSettings {
  enabled: boolean
}

/** Global switch for the spectrum bars that scenery scenes show behind their landscape while music plays. */
export default function MusicLayerCard() {
  const apiUrl = useApiUrl()
  const { data, error, setData } = useFetch<MusicLayerSettings>('/music_layer')
  const [saving, setSaving] = useState(false)
  // An older daemon has no /music_layer: show nothing instead of an error.
  if (error || !data || typeof data.enabled !== 'boolean') return null

  const save = async (enabled: boolean) => {
    if (!apiUrl) return
    setSaving(true)
    try {
      const res = await fetch(`${apiUrl}/music_layer`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled }),
      })
      if (!res.ok) throw new Error(await res.text())
      setData((await res.json()) as MusicLayerSettings)
    } catch {
      toast.error('Could not change the music bars setting')
    } finally {
      setSaving(false)
    }
  }

  return <Card>
    <CardContent className="p-6">
      <label className="flex min-h-10 cursor-pointer items-center justify-between gap-3">
        <span className="flex items-center gap-3 min-w-0">
          <span className="w-10 h-10 shrink-0 rounded-xl bg-primary/10 flex items-center justify-center">
            <AudioLines className="h-5 w-5 text-primary" />
          </span>
          <span className="min-w-0">
            <span className="block font-medium">Music bars behind scenery</span>
            <span className="block text-sm text-muted-foreground">
              While Spotify or other music plays, scenes like the harbour show spectrum bars rising behind the landscape. Each scene can also turn them off in its settings.
            </span>
          </span>
        </span>
        <Switch checked={data.enabled} disabled={saving} aria-label="Music bars behind scenery"
          onCheckedChange={(enabled) => void save(enabled)} />
      </label>
    </CardContent>
  </Card>
}
