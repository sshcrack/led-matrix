import { useState } from 'react'
import { Link } from 'react-router-dom'
import { Check, Plus, Sparkles } from 'lucide-react'
import { cn } from '~/lib/utils'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'
import { useMatrix } from '~/core/matrixContext'
import { useNowPlaying } from '~/core/nowPlaying'
import { useResource } from '~/core/useResource'
import { presetThumbNames } from '~/core/scenes'
import type { Pack } from '~/core/useMatrix'
import { ThumbStrip } from '~/core/ui/SceneThumb'
import { ModeSwitch } from '~/core/ui/controls'
import { Sheet } from '~/core/ui/Sheet'
import { QuickBuilder } from '~/core/ui/QuickBuilder'

export default function DeckHome() {
  const { apiUrl, presets, scenes, status, activate, addPack, sceneByName, error, refreshAll } = useMatrix()
  const now = useNowPlaying()
  const { data: packs } = useResource<Pack[]>('/ambiance_packs')
  const [building, setBuilding] = useState(false)
  const playing = status?.operation_mode === 'manual' ? status.current : null

  if (error && !presets) return <div className="mt-20 space-y-3 text-center"><p className="font-semibold">Can’t reach the matrix</p><button className="rounded-full bg-primary px-5 py-2 font-semibold text-primary-foreground" onClick={refreshAll}>Try again</button></div>

  return <div className="space-y-6">
    <div className="relative">
      <LiveMatrixPreview apiUrl={apiUrl ?? ''} fallbackSceneName={now.firstScene} fallbackHasPreview={now.firstScene ? sceneByName.get(now.firstScene)?.has_preview : false} label={now.label} className="mx-auto max-w-[min(100%,26rem)] rounded-[2rem]" />
    </div>

    <div className="flex items-center justify-between gap-3 rounded-3xl bg-card p-3 shadow-sm ring-1 ring-border">
      <div className="px-2"><div className="text-xs font-medium text-muted-foreground">Mode</div><div className="text-sm font-bold">{now.automatic ? 'Director is in charge' : 'You pick the scene'}</div></div>
      <ModeSwitch />
    </div>

    <section aria-labelledby="quick">
      <div className="mb-2 flex items-end justify-between"><h2 id="quick" className="text-lg font-extrabold tracking-tight">Quick play</h2><Link to="/presets" className="text-sm font-semibold text-primary">All presets</Link></div>
      <div className="-mx-4 flex snap-x gap-3 overflow-x-auto px-4 pb-2">
        {Object.entries(presets ?? {}).map(([id, p]) => {
          const on = playing === id
          return <button key={id} onClick={() => void activate(id)} aria-pressed={on} className={cn('relative w-36 shrink-0 snap-start rounded-3xl p-3 text-left ring-1 transition active:scale-[0.97]', on ? 'bg-primary text-primary-foreground ring-primary shadow-lg shadow-primary/30' : 'bg-card ring-border')}>
            <ThumbStrip names={presetThumbNames(p, scenes ?? [], 3)} size="h-12 w-12" className="mb-3" />
            <span className="block truncate text-sm font-bold">{p.display_name || id}</span>
            <span className={cn('block text-xs', on ? 'text-primary-foreground/75' : 'text-muted-foreground')}>{on ? 'Playing' : `${p.scenes.length} scene${p.scenes.length === 1 ? '' : 's'}`}</span>
            {on && <Check className="absolute right-3 top-3 h-4 w-4" />}
          </button>
        })}
        <button onClick={() => setBuilding(true)} className="grid w-36 shrink-0 snap-start place-items-center rounded-3xl border-2 border-dashed border-border p-3 text-center text-sm font-bold text-muted-foreground active:scale-[0.97]"><span><Plus className="mx-auto mb-1 h-6 w-6 text-primary" />New preset</span></button>
      </div>
    </section>

    {packs && packs.length > 0 && <section aria-labelledby="moods">
      <h2 id="moods" className="mb-2 flex items-center gap-2 text-lg font-extrabold tracking-tight"><Sparkles className="h-4 w-4 text-primary" />Set the mood</h2>
      <div className="grid grid-cols-2 gap-3">
        {packs.filter(p => p.available).slice(0, 6).map(p => <button key={p.id} onClick={() => void addPack(p.id, true)} className="overflow-hidden rounded-3xl bg-card text-left ring-1 ring-border transition active:scale-[0.97]">
          <span className="flex h-12">{p.colors.map(c => <span key={c} className="flex-1" style={{ background: c }} />)}</span>
          <span className="block p-3"><span className="block text-sm font-bold">{p.name}</span><span className="block text-xs text-muted-foreground">Tap to play</span></span>
        </button>)}
      </div>
    </section>}

    <Sheet open={building} onOpenChange={setBuilding} title="New preset" description="Pick one scene or several. One tap creates it and starts playing.">
      <QuickBuilder gridClassName="grid-cols-3" onDone={() => setBuilding(false)} />
    </Sheet>
  </div>
}
