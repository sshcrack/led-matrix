import { useState } from 'react'
import { useSearchParams } from 'react-router-dom'
import { Pencil, Play, Plus, Search, X } from 'lucide-react'
import { cn } from '~/lib/utils'
import { Button } from '~/components/ui/button'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'
import { useMatrix } from '~/core/matrixContext'
import { useNowPlaying } from '~/core/nowPlaying'
import { presetThumbNames } from '~/core/scenes'
import { ThumbStrip } from '~/core/ui/SceneThumb'
import { ModeSwitch } from '~/core/ui/controls'
import { PresetMenu } from '~/core/ui/PresetMenu'
import { PresetEditor } from '~/core/ui/PresetEditor'
import { QuickBuilder } from '~/core/ui/QuickBuilder'

export default function ConsoleHome() {
  const { apiUrl, presets, scenes, status, activate, loading, error, refreshAll, sceneByName } = useMatrix()
  const now = useNowPlaying()
  const [params, setParams] = useSearchParams()
  const [filter, setFilter] = useState('')
  const editId = params.get('edit')
  const building = params.get('new') === '1'
  const open = (next: Record<string, string> | null) => setParams(next ?? {}, { replace: true })

  const entries = Object.entries(presets ?? {}).filter(([id, p]) => (p.display_name || id).toLowerCase().includes(filter.toLowerCase()))
  const playing = status?.operation_mode === 'manual' ? status.current : null

  if (error && !presets) return <div className="mx-auto mt-20 max-w-sm space-y-3 text-center"><p className="font-medium">Can’t reach the matrix.</p><p className="text-sm text-muted-foreground">{error.message}</p><Button onClick={refreshAll}>Retry</Button></div>

  return <div className="grid gap-4 lg:grid-cols-[17rem_minmax(0,1fr)] xl:grid-cols-[17rem_minmax(0,1fr)_28rem]">
    {/* Presets rail */}
    <aside className="flex min-h-0 flex-col gap-2 lg:sticky lg:top-16 lg:max-h-[calc(100vh-5rem)]" aria-label="Presets">
      <div className="flex items-center gap-2">
        <div className="relative flex-1"><Search className="absolute left-2.5 top-1/2 h-3.5 w-3.5 -translate-y-1/2 text-muted-foreground" /><input value={filter} onChange={e => setFilter(e.target.value)} placeholder="Filter presets" aria-label="Filter presets" className="h-9 w-full rounded-md border border-input bg-card pl-8 pr-2 text-sm outline-none focus:ring-2 focus:ring-ring" /></div>
        <Button size="icon" onClick={() => open({ new: '1' })} aria-label="New preset" title="New preset"><Plus className="h-4 w-4" /></Button>
      </div>
      <ul className="min-h-0 flex-1 space-y-1 overflow-y-auto pr-0.5">
        {loading && !presets && Array.from({ length: 5 }, (_, i) => <li key={i} className="h-14 animate-pulse rounded-lg bg-secondary" />)}
        {entries.map(([id, p]) => {
          const on = playing === id
          return <li key={id} className={cn('group flex items-center gap-1 rounded-lg border pr-1 transition', on ? 'border-primary/60 bg-primary/10' : 'border-transparent hover:border-border hover:bg-card', editId === id && 'ring-1 ring-ring')}>
            <button onClick={() => void activate(id)} className="flex min-w-0 flex-1 items-center gap-3 p-2 text-left" aria-label={`Play ${p.display_name || id}`}>
              <ThumbStrip names={presetThumbNames(p, scenes ?? [], 2)} size="h-9 w-9" />
              <span className="min-w-0"><span className="block truncate text-sm font-medium">{p.display_name || id}</span><span className="block text-[11px] text-muted-foreground">{p.scenes.length} scene{p.scenes.length === 1 ? '' : 's'}{on && ' · playing'}</span></span>
            </button>
            <Button variant="ghost" size="icon" className="h-8 w-8 opacity-60 group-hover:opacity-100" onClick={() => open({ edit: id })} aria-label={`Edit ${p.display_name || id}`}><Pencil className="h-3.5 w-3.5" /></Button>
            <PresetMenu id={id} onEdit={id => open({ edit: id })} className="h-8 w-8" />
          </li>
        })}
        {presets && entries.length === 0 && <li className="p-4 text-center text-sm text-muted-foreground">{filter ? 'No match.' : 'No presets yet — press + to build one in a few taps.'}</li>}
      </ul>
    </aside>

    {/* Stage */}
    <section className="min-w-0 space-y-3">
      <LiveMatrixPreview apiUrl={apiUrl ?? ''} fallbackSceneName={now.firstScene} fallbackHasPreview={now.firstScene ? sceneByName.get(now.firstScene)?.has_preview : false} label={`Now · ${now.label}`} className="mx-auto max-w-[min(100%,calc(100vh-14rem))]" />
      <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-border bg-card px-4 py-3">
        <div className="min-w-0"><div className="font-mono text-[10px] uppercase tracking-widest text-muted-foreground">{now.off ? 'Display off' : now.automatic ? 'Director picks scenes' : 'Playing preset'}</div><div className="truncate font-semibold">{now.label}</div></div>
        <ModeSwitch />
      </div>
    </section>

    {/* Inspector */}
    <section className="min-w-0 lg:col-span-2 xl:col-span-1 xl:sticky xl:top-16 xl:max-h-[calc(100vh-5rem)] xl:self-start xl:overflow-y-auto" aria-label="Inspector">
      {editId && presets?.[editId] ? <div className="rounded-xl border border-border bg-card p-3">
        <div className="mb-2 flex items-center justify-between"><span className="font-mono text-[10px] uppercase tracking-widest text-muted-foreground">Editing</span>
          <div className="flex gap-1"><Button size="sm" variant="outline" onClick={() => void activate(editId)}><Play className="h-3.5 w-3.5" />Play</Button><Button size="icon" variant="ghost" className="h-8 w-8" onClick={() => open(null)} aria-label="Close editor"><X className="h-4 w-4" /></Button></div></div>
        <PresetEditor presetId={editId} layout="stack" />
      </div>
        : building ? <div className="rounded-xl border border-border bg-card p-3">
          <div className="mb-2 flex items-center justify-between"><span className="font-mono text-[10px] uppercase tracking-widest text-muted-foreground">New preset</span><Button size="icon" variant="ghost" className="h-8 w-8" onClick={() => open(null)} aria-label="Close"><X className="h-4 w-4" /></Button></div>
          <QuickBuilder gridClassName="grid-cols-3" className="max-h-[70vh]" onDone={id => open(id ? { edit: id } : null)} />
        </div>
          : <div className="rounded-xl border border-dashed border-border p-6 text-center text-sm text-muted-foreground">
            <p>Click a preset to <b className="text-foreground">play</b> it. Use <Pencil className="inline h-3 w-3" /> to edit — changes save automatically.</p>
            <Button className="mt-4" onClick={() => open({ new: '1' })}><Plus className="h-4 w-4" />Build a preset</Button>
          </div>}
    </section>
  </div>
}
