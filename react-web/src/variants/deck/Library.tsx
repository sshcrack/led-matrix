import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Pencil, Play, Plus } from 'lucide-react'
import { cn } from '~/lib/utils'
import { useMatrix } from '~/core/matrixContext'
import { presetThumbNames } from '~/core/scenes'
import { ThumbStrip } from '~/core/ui/SceneThumb'
import { PresetMenu } from '~/core/ui/PresetMenu'
import { Sheet } from '~/core/ui/Sheet'
import { QuickBuilder } from '~/core/ui/QuickBuilder'

export default function DeckLibrary() {
  const { presets, scenes, status, activate } = useMatrix()
  const navigate = useNavigate()
  const [building, setBuilding] = useState(false)
  const playing = status?.operation_mode === 'manual' ? status.current : null
  return <div className="space-y-4 pt-2">
    <div className="flex items-center justify-between"><h1 className="text-2xl font-extrabold tracking-tight">Presets</h1>
      <button onClick={() => setBuilding(true)} className="flex items-center gap-1.5 rounded-full bg-primary px-4 py-2 text-sm font-bold text-primary-foreground shadow-md shadow-primary/30"><Plus className="h-4 w-4" />New</button></div>
    <ul className="space-y-3">
      {Object.entries(presets ?? {}).map(([id, p]) => <li key={id} className={cn('flex items-center gap-3 rounded-3xl bg-card p-3 ring-1', playing === id ? 'ring-primary' : 'ring-border')}>
        <button onClick={() => void activate(id)} className="flex min-w-0 flex-1 items-center gap-3 text-left" aria-label={`Play ${p.display_name || id}`}>
          <ThumbStrip names={presetThumbNames(p, scenes ?? [], 3)} size="h-12 w-12" />
          <span className="min-w-0"><span className="block truncate font-bold">{p.display_name || id}</span><span className="block text-xs text-muted-foreground">{playing === id ? 'Playing now' : `${p.scenes.length} scene${p.scenes.length === 1 ? '' : 's'}`}</span></span>
        </button>
        {playing !== id && <button onClick={() => void activate(id)} aria-label="Play" className="grid h-10 w-10 place-items-center rounded-full bg-primary/12 text-primary"><Play className="h-4 w-4" /></button>}
        <button onClick={() => navigate(`/modify-preset/${encodeURIComponent(id)}`)} aria-label="Edit" className="grid h-10 w-10 place-items-center rounded-full bg-secondary"><Pencil className="h-4 w-4" /></button>
        <PresetMenu id={id} className="h-10 w-10 rounded-full" />
      </li>)}
    </ul>
    <Sheet open={building} onOpenChange={setBuilding} title="New preset" description="Pick one scene or several. One tap creates it and starts playing.">
      <QuickBuilder gridClassName="grid-cols-3" onDone={() => setBuilding(false)} />
    </Sheet>
  </div>
}
