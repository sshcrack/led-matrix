import { useMemo, useState } from 'react'
import { Check, Search } from 'lucide-react'
import { cn } from '~/lib/utils'
import { Input } from '~/components/ui/input'
import { intensityLabel, sceneDisplayName } from '~/lib/sceneVariants'
import { useMatrix } from '../matrixContext'
import { SceneThumb } from './SceneThumb'

interface Props {
  selected: string[]
  onToggle: (sceneName: string) => void
  /** Tailwind grid classes; variants tune density. */
  gridClassName?: string
  autoFocusSearch?: boolean
  className?: string
}

/** Multi-select scene grid with search + category chips. Tap to toggle — no dialogs, no confirm per scene. */
export function ScenePicker({ selected, onToggle, gridClassName = 'grid-cols-3 sm:grid-cols-4 lg:grid-cols-5', autoFocusSearch, className }: Props) {
  const { scenes } = useMatrix()
  const [query, setQuery] = useState('')
  const [category, setCategory] = useState('All')
  const categories = useMemo(() => ['All', ...new Set((scenes ?? []).map(s => s.category ?? 'General'))].sort((a, b) => a === 'All' ? -1 : b === 'All' ? 1 : a.localeCompare(b)), [scenes])
  const q = query.trim().toLowerCase()
  const filtered = useMemo(() => (scenes ?? []).filter(s =>
    (category === 'All' || (s.category ?? 'General') === category)
    && (!q || [s.name, s.descriptor?.family, ...(s.descriptor?.tags ?? [])].join(' ').toLowerCase().includes(q))
  ), [scenes, category, q])
  const picked = new Set(selected)

  return <div className={cn('flex min-h-0 flex-col gap-3', className)}>
    <div className="relative shrink-0">
      <Search className="absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" />
      <Input autoFocus={autoFocusSearch} value={query} onChange={e => setQuery(e.target.value)} placeholder="Search scenes, styles, tags…" className="pl-9" />
    </div>
    <div className="-mx-1 flex shrink-0 gap-1.5 overflow-x-auto px-1 pb-1" role="tablist" aria-label="Scene categories">
      {categories.map(c => <button key={c} role="tab" aria-selected={c === category} onClick={() => setCategory(c)}
        className={cn('shrink-0 rounded-full border px-3 py-1 text-xs font-medium transition', c === category ? 'border-primary bg-primary text-primary-foreground' : 'border-border text-muted-foreground hover:bg-secondary')}>{c}</button>)}
    </div>
    <div className="min-h-0 flex-1 overflow-y-auto">
      <div className={cn('grid gap-2.5', gridClassName)}>
        {filtered.map(s => {
          const on = picked.has(s.name)
          return <button key={s.name} onClick={() => onToggle(s.name)} aria-pressed={on}
            className={cn('group relative overflow-hidden rounded-xl border bg-card text-left transition', on ? 'border-primary ring-2 ring-primary/40' : 'border-border hover:border-primary/50')}>
            <SceneThumb name={s.name} className="w-full" />
            {on && <span className="absolute right-1.5 top-1.5 grid h-5 w-5 place-items-center rounded-full bg-primary text-primary-foreground"><Check className="h-3 w-3" /></span>}
            <span className="block px-2 py-1.5">
              <span className="block truncate text-xs font-semibold">{sceneDisplayName(s.name)}</span>
              <span className="block truncate text-[10px] text-muted-foreground">{s.descriptor ? intensityLabel(s.descriptor.intensity) : s.category}{s.needs_desktop ? ' · desktop' : ''}</span>
            </span>
          </button>
        })}
      </div>
      {filtered.length === 0 && <p className="py-10 text-center text-sm text-muted-foreground">No scenes match “{query}”.</p>}
    </div>
  </div>
}
