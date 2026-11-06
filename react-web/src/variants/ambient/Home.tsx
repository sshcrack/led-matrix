import { useState } from 'react'
import { Plus, Sparkles } from 'lucide-react'
import { cn } from '~/lib/utils'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'
import { useMatrix } from '~/core/matrixContext'
import { useNowPlaying } from '~/core/nowPlaying'
import { useResource } from '~/core/useResource'
import type { Pack } from '~/core/useMatrix'
import { ModeSwitch } from '~/core/ui/controls'
import { Sheet } from '~/core/ui/Sheet'
import { QuickBuilder } from '~/core/ui/QuickBuilder'

export default function AmbientHome() {
  const { apiUrl, presets, status, activate, addPack, sceneByName, error, refreshAll } = useMatrix()
  const now = useNowPlaying()
  const { data: packs } = useResource<Pack[]>('/ambiance_packs')
  const [building, setBuilding] = useState(false)
  const playing = status?.operation_mode === 'manual' ? status.current : null

  if (error && !presets) return <div className="grid min-h-screen place-items-center"><button onClick={refreshAll} className="rounded-full bg-primary px-5 py-2 text-primary-foreground">Can’t reach the matrix — retry</button></div>

  return <div className="mx-auto flex min-h-screen max-w-3xl flex-col items-center justify-center gap-6 px-4 pb-10 pt-20">
    <div className={cn('w-full max-w-[min(100%,calc(100vh-26rem))] rounded-2xl transition-opacity duration-700', now.off && 'opacity-30')} style={{ boxShadow: now.off ? 'none' : '0 0 8rem -1rem rgb(var(--primary) / 0.45)' }}>
      <LiveMatrixPreview apiUrl={apiUrl ?? ''} fallbackSceneName={now.firstScene} fallbackHasPreview={now.firstScene ? sceneByName.get(now.firstScene)?.has_preview : false} label=" " className="rounded-2xl border-white/5" />
    </div>
    <div className="text-center"><h1 className="text-3xl sm:text-4xl">{now.label}</h1><p className="mt-1 text-sm text-muted-foreground">{now.off ? 'Resting' : now.automatic ? 'Evolving on its own' : 'Holding this look'}</p></div>
    <ModeSwitch />
    <div className="-mx-4 flex w-screen max-w-3xl gap-2 overflow-x-auto px-4 pb-1 sm:mx-0 sm:w-full sm:flex-wrap sm:justify-center sm:overflow-visible" role="group" aria-label="Moods">
      {Object.entries(presets ?? {}).map(([id, p]) => <button key={id} onClick={() => void activate(id)} aria-pressed={playing === id} className={cn('shrink-0 rounded-full border px-4 py-2 text-sm transition', playing === id ? 'border-primary bg-primary text-primary-foreground shadow-lg shadow-primary/30' : 'border-border bg-card/70 backdrop-blur hover:border-primary/60')}>{p.display_name || id}</button>)}
      {(packs ?? []).filter(p => p.available).slice(0, 4).map(p => <button key={p.id} onClick={() => void addPack(p.id, true)} className="flex shrink-0 items-center gap-2 rounded-full border border-dashed border-border px-4 py-2 text-sm text-muted-foreground transition hover:border-primary/60 hover:text-foreground"><span className="flex -space-x-1">{p.colors.slice(0, 3).map(c => <span key={c} className="h-3 w-3 rounded-full ring-1 ring-background" style={{ background: c }} />)}</span><Sparkles className="h-3 w-3" />{p.name}</button>)}
      <button onClick={() => setBuilding(true)} className="flex shrink-0 items-center gap-1.5 rounded-full px-4 py-2 text-sm font-medium text-primary hover:bg-primary/10"><Plus className="h-4 w-4" />Create</button>
    </div>
    <Sheet open={building} onOpenChange={setBuilding} title="Create something" description="Pick a mood or combine scenes. It starts playing right away.">
      <QuickBuilder gridClassName="grid-cols-3" onDone={() => setBuilding(false)} />
    </Sheet>
  </div>
}
