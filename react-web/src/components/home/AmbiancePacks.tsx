import { useState } from 'react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Button } from '~/components/ui/button'

interface Pack {
  id: string
  name: string
  description: string
  colors: string[]
  available: boolean
  missing_scenes: string[]
}

export default function AmbiancePacks({ onCreated }: { onCreated: () => void }) {
  const { data: packs, error, isLoading, setRetry } = useFetch<Pack[]>('/ambiance_packs')
  const apiUrl = useApiUrl()
  const [adding, setAdding] = useState<string | null>(null)
  const add = async (pack: Pack) => {
    if (!apiUrl || adding) return
    setAdding(pack.id)
    try {
      const response = await fetch(`${apiUrl}/ambiance_packs?id=${encodeURIComponent(pack.id)}`, { method: 'POST' })
      if (!response.ok) throw new Error('Could not add the preset')
      toast.success(`Added “${pack.name}” to your presets`)
      onCreated()
    } catch { toast.error('Could not add the preset. Try again.') }
    finally { setAdding(null) }
  }
  return <section className="space-y-3 pt-4" aria-labelledby="ambiance-heading">
    <div><h3 id="ambiance-heading" className="font-semibold">Set the mood</h3>
      <p className="mt-1 text-sm text-muted-foreground">Ready-to-play looks with soft color and slow movement. Add a copy, then make it yours.</p></div>
    {isLoading && <p className="text-sm text-muted-foreground">Loading ambiance packs…</p>}
    {error && <div className="text-sm text-muted-foreground">Could not load ambiance packs. <button className="underline" onClick={() => setRetry(value => value + 1)}>Try again</button></div>}
    <div className="divide-y divide-border/60">{packs?.map(pack => <div key={pack.id} className="flex flex-wrap items-center gap-3 py-3">
      <div className="flex h-12 w-12 shrink-0 overflow-hidden rounded-lg" aria-hidden="true">
        {pack.colors.map(color => <span key={color} className="flex-1" style={{ backgroundColor: color }} />)}
      </div>
      <div className="min-w-0 flex-1 basis-40"><h4 className="text-sm font-medium">{pack.name}</h4>
        <p className="mt-1 text-xs leading-relaxed text-muted-foreground">{pack.description}</p>
        {!pack.available && <p className="mt-1 text-xs text-muted-foreground">Required scenes are unavailable: {pack.missing_scenes.join(', ')}.</p>}
      </div>
      <Button variant="outline" size="sm" disabled={!pack.available || adding !== null} onClick={() => add(pack)}>
        {adding === pack.id ? 'Adding…' : 'Add preset'}<span className="sr-only"> {pack.name}</span>
      </Button>
    </div>)}</div>
  </section>
}
