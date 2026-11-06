import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Pencil, Play, Plus, Search, X } from 'lucide-react'
import { cn } from '~/lib/utils'
import { Button } from '~/components/ui/button'
import { useMatrix } from '~/core/matrixContext'
import { presetThumbNames } from '~/core/scenes'
import { ThumbStrip } from '~/core/ui/SceneThumb'
import { PresetMenu } from '~/core/ui/PresetMenu'
import { QuickBuilder } from '~/core/ui/QuickBuilder'
import { ModeSwitch } from '~/core/ui/controls'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'
import { useNowPlaying } from '~/core/nowPlaying'

export default function StudioHome() {
  const { apiUrl, presets, scenes, status, activate, sceneByName, error, refreshAll } = useMatrix()
  const now = useNowPlaying()
  const navigate = useNavigate()
  const [building, setBuilding] = useState(false)
  const [q, setQ] = useState('')
  const [preview, setPreview] = useState(true)
  const playing = status?.operation_mode === 'manual' ? status.current : null
  const entries = Object.entries(presets ?? {}).filter(([id, p]) => (p.display_name || id).toLowerCase().includes(q.toLowerCase()))

  if (error && !presets) return <div className="mt-20 space-y-3 text-center"><p className="font-medium">Can’t reach the matrix.</p><Button onClick={refreshAll}>Retry</Button></div>

  return <div className="space-y-6">
    <div className="flex flex-wrap items-end justify-between gap-3">
      <div><h1 className="text-3xl font-bold">Library</h1><p className="text-sm text-muted-foreground">{entries.length} preset{entries.length === 1 ? '' : 's'} · tap a card to play it</p></div>
      <div className="flex items-center gap-2">
        <div className="relative"><Search className="absolute left-2.5 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" /><input value={q} onChange={e => setQ(e.target.value)} placeholder="Search" aria-label="Search presets" className="h-9 w-44 rounded-lg border border-input bg-card pl-8 pr-2 text-sm outline-none focus:ring-2 focus:ring-ring" /></div>
        <Button onClick={() => setBuilding(b => !b)} variant={building ? 'outline' : 'default'}>{building ? <><X className="h-4 w-4" />Close</> : <><Plus className="h-4 w-4" />New preset</>}</Button>
      </div>
    </div>

    {building && <section className="rounded-2xl border border-border bg-card p-4 shadow-sm" aria-label="Create a preset">
      <div className="mb-3"><h2 className="font-semibold">Build a preset</h2><p className="text-sm text-muted-foreground">Pick a mood or tap scenes to combine them. Name is optional — we’ll suggest one.</p></div>
      <QuickBuilder gridClassName="grid-cols-3 sm:grid-cols-5 lg:grid-cols-7" className="max-h-[68vh]" onDone={id => { setBuilding(false); if (id) navigate(`/modify-preset/${encodeURIComponent(id)}`) }} />
    </section>}

    <div className="grid gap-6 xl:grid-cols-[minmax(0,1fr)_20rem]">
      <section className="grid gap-4 sm:grid-cols-2 2xl:grid-cols-3" aria-label="Presets">
        {entries.map(([id, p]) => {
          const on = playing === id
          return <article key={id} className={cn('group overflow-hidden rounded-2xl border bg-card transition hover:shadow-lg', on ? 'border-primary ring-2 ring-primary/30' : 'border-border')}>
            <button onClick={() => void activate(id)} className="block w-full" aria-label={`Play ${p.display_name || id}`}>
              <ThumbStrip names={presetThumbNames(p, scenes ?? [], 3)} size="h-24 w-24" className="justify-center bg-black py-3 -space-x-4" />
            </button>
            <div className="flex items-center gap-1 p-3">
              <div className="min-w-0 flex-1"><div className="truncate font-semibold">{p.display_name || id}</div><div className="text-xs text-muted-foreground">{on ? 'Playing now' : `${p.scenes.length} scene${p.scenes.length === 1 ? '' : 's'}`}</div></div>
              {!on && <Button size="icon" variant="ghost" className="h-8 w-8 text-primary" onClick={() => void activate(id)} aria-label="Play"><Play className="h-4 w-4" /></Button>}
              <Button size="icon" variant="ghost" className="h-8 w-8" onClick={() => navigate(`/modify-preset/${encodeURIComponent(id)}`)} aria-label="Edit"><Pencil className="h-4 w-4" /></Button>
              <PresetMenu id={id} className="h-8 w-8" />
            </div>
          </article>
        })}
        {presets && entries.length === 0 && <p className="col-span-full py-16 text-center text-muted-foreground">{q ? 'No presets match.' : 'No presets yet — press “New preset”.'}</p>}
      </section>

      <aside className="hidden xl:block">
        <div className="sticky top-6 space-y-3">
          <button onClick={() => setPreview(v => !v)} className="text-xs font-semibold uppercase tracking-wider text-muted-foreground">Live view {preview ? '▾' : '▸'}</button>
          {preview && <LiveMatrixPreview apiUrl={apiUrl ?? ''} fallbackSceneName={now.firstScene} fallbackHasPreview={now.firstScene ? sceneByName.get(now.firstScene)?.has_preview : false} label={now.label} />}
          <ModeSwitch />
        </div>
      </aside>
    </div>
  </div>
}
