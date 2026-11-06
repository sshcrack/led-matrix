import { useEffect, useMemo, useState } from 'react'
import { Command, Play, Plus, Power, Sparkles } from 'lucide-react'
import { useNavigate } from 'react-router-dom'
import { Dialog, DialogContent, DialogDescription, DialogTitle } from '~/components/ui/dialog'
import { cn } from '~/lib/utils'
import { useMatrix } from '../matrixContext'
import { NAV } from '../nav'

interface Cmd { id: string; label: string; hint: string; icon: React.ReactNode; run: () => void }

/** ⌘K / Ctrl+K: jump to any preset, page or action by typing. */
export function CommandPalette({ open, onOpenChange, onNewPreset, onEditPreset }: { open: boolean; onOpenChange: (o: boolean) => void; onNewPreset?: () => void; onEditPreset?: (id: string) => void }) {
  const { presets, status, activate, setPower, setMode } = useMatrix()
  const navigate = useNavigate()
  const [q, setQ] = useState('')
  const [index, setIndex] = useState(0)

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => { if ((e.metaKey || e.ctrlKey) && e.key.toLowerCase() === 'k') { e.preventDefault(); onOpenChange(!open) } }
    window.addEventListener('keydown', onKey)
    return () => window.removeEventListener('keydown', onKey)
  }, [open, onOpenChange])

  const all = useMemo<Cmd[]>(() => [
    ...Object.entries(presets ?? {}).flatMap(([id, p]) => {
      const name = p.display_name || id
      const out: Cmd[] = [{ id: `play:${id}`, label: `Play ${name}`, hint: 'Preset', icon: <Play className="h-4 w-4" />, run: () => void activate(id) }]
      if (onEditPreset) out.push({ id: `edit:${id}`, label: `Edit ${name}`, hint: 'Preset', icon: <Command className="h-4 w-4" />, run: () => onEditPreset(id) })
      return out
    }),
    ...(onNewPreset ? [{ id: 'new', label: 'New preset…', hint: 'Action', icon: <Plus className="h-4 w-4" />, run: onNewPreset }] : []),
    { id: 'power', label: status?.turned_off ? 'Turn matrix on' : 'Turn matrix off', hint: 'Action', icon: <Power className="h-4 w-4" />, run: () => void setPower(!!status?.turned_off) },
    { id: 'mode', label: status?.operation_mode === 'automatic' ? 'Switch to manual' : 'Switch to automatic director', hint: 'Action', icon: <Sparkles className="h-4 w-4" />, run: () => void setMode(status?.operation_mode === 'automatic' ? 'manual' : 'automatic') },
    ...NAV.map(n => ({ id: `go:${n.to}`, label: `Go to ${n.label}`, hint: n.description, icon: n.icon, run: () => navigate(n.to) })),
  ], [presets, status, activate, setPower, setMode, navigate, onNewPreset, onEditPreset])

  const results = all.filter(c => c.label.toLowerCase().includes(q.trim().toLowerCase())).slice(0, 12)
  const run = (c?: Cmd) => { if (!c) return; onOpenChange(false); setQ(''); c.run() }

  return <Dialog open={open} onOpenChange={o => { onOpenChange(o); if (!o) { setQ(''); setIndex(0) } }}>
    <DialogContent className="top-[20%] max-w-lg translate-y-0 gap-0 overflow-hidden p-0">
      <DialogTitle className="sr-only">Command palette</DialogTitle><DialogDescription className="sr-only">Search presets, pages and actions</DialogDescription>
      <input autoFocus value={q} onChange={e => { setQ(e.target.value); setIndex(0) }} placeholder="Type a preset, page or action…" aria-label="Search commands"
        onKeyDown={e => { if (e.key === 'ArrowDown') { e.preventDefault(); setIndex(i => Math.min(i + 1, results.length - 1)) } else if (e.key === 'ArrowUp') { e.preventDefault(); setIndex(i => Math.max(i - 1, 0)) } else if (e.key === 'Enter') run(results[index]) }}
        className="h-12 border-b border-border bg-transparent px-4 text-sm outline-none" />
      <ul className="max-h-80 overflow-y-auto p-1.5" role="listbox">
        {results.map((c, i) => <li key={c.id} role="option" aria-selected={i === index}>
          <button onMouseEnter={() => setIndex(i)} onClick={() => run(c)} className={cn('flex w-full items-center gap-3 rounded-lg px-3 py-2 text-left text-sm', i === index ? 'bg-primary/10 text-foreground' : 'text-muted-foreground')}>
            <span className="text-primary">{c.icon}</span><span className="flex-1 truncate text-foreground">{c.label}</span><span className="text-xs">{c.hint}</span>
          </button></li>)}
        {results.length === 0 && <li className="px-3 py-6 text-center text-sm text-muted-foreground">Nothing matches.</li>}
      </ul>
    </DialogContent>
  </Dialog>
}
