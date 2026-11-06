import { useMemo, useState } from 'react'
import { Play, Plus, Sparkles, Wand2 } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { cn } from '~/lib/utils'
import { useMatrix } from '../matrixContext'
import { useResource } from '../useResource'
import { collectTemplates, usePluginUis } from '../pluginRegistry'
import { newSceneInstance, suggestPresetName } from '../scenes'
import type { Pack } from '../useMatrix'
import { ScenePicker } from './ScenePicker'

/**
 * Fast preset setup: pick scenes (or a mood / plugin template) → one tap creates
 * the preset *and* starts it. Replaces: New preset dialog → name → create → open
 * editor → Add scene dialog → select → add → repeat → Save → go back → Activate.
 */
export function QuickBuilder({ onDone, gridClassName, className }: { onDone: (presetId: string | null) => void; gridClassName?: string; className?: string }) {
  const { scenes, sceneByName, createPreset, addPack } = useMatrix()
  const { data: packs } = useResource<Pack[]>('/ambiance_packs')
  const templates = collectTemplates(usePluginUis())
  const [picked, setPicked] = useState<string[]>([])
  const [name, setName] = useState('')
  const [busy, setBusy] = useState(false)
  const suggestion = suggestPresetName(picked)
  const toggle = (n: string) => setPicked(p => p.includes(n) ? p.filter(x => x !== n) : [...p, n])

  const create = async (play: boolean, sceneNames = picked, title = name.trim() || suggestion, variants?: Record<string, string>) => {
    const defs = sceneNames.map(n => sceneByName.get(n)).filter((d): d is NonNullable<typeof d> => !!d)
    if (!defs.length) return
    setBusy(true)
    const id = await createPreset({ name: title, scenes: defs.map(d => newSceneInstance(d, variants?.[d.name])), play })
    setBusy(false)
    if (id) onDone(id)
  }

  const quick = useMemo(() => [
    ...templates.map(t => ({ key: `t:${t.id}`, title: t.name, subtitle: t.description, colors: [] as string[], disabled: !t.scenes.some(s => sceneByName.has(s)), run: () => create(true, t.scenes, t.name, t.variants) })),
    ...(packs ?? []).map(p => ({ key: `p:${p.id}`, title: p.name, subtitle: p.description, colors: p.colors, disabled: !p.available, run: async () => { setBusy(true); await addPack(p.id, true); setBusy(false); onDone(null) } })),
  // eslint-disable-next-line react-hooks/exhaustive-deps
  ], [templates, packs, sceneByName])

  return <div className={cn('flex min-h-0 flex-col gap-4', className)}>
    {quick.length > 0 && <section aria-label="One-tap starting points">
      <h3 className="mb-2 flex items-center gap-1.5 text-xs font-semibold uppercase tracking-wider text-muted-foreground"><Sparkles className="h-3.5 w-3.5" />One tap</h3>
      <div className="-mx-1 flex gap-2 overflow-x-auto px-1 pb-1">
        {quick.map(q => <button key={q.key} disabled={q.disabled || busy} onClick={q.run} title={q.subtitle}
          className="group flex w-40 shrink-0 flex-col overflow-hidden rounded-xl border border-border bg-card text-left transition hover:border-primary/60 disabled:opacity-50">
          <span className="flex h-9">{q.colors.length ? q.colors.map(c => <span key={c} className="flex-1" style={{ background: c }} />) : <span className="flex flex-1 items-center justify-center bg-secondary"><Wand2 className="h-4 w-4 text-muted-foreground" /></span>}</span>
          <span className="flex items-center justify-between gap-2 px-2.5 py-2"><span className="truncate text-xs font-semibold">{q.title}</span><Play className="h-3 w-3 shrink-0 text-primary opacity-0 transition group-hover:opacity-100" /></span>
        </button>)}
      </div>
    </section>}

    <section className="flex min-h-0 flex-1 flex-col" aria-label="Build from scenes">
      <h3 className="mb-2 text-xs font-semibold uppercase tracking-wider text-muted-foreground">Or pick scenes <span className="font-normal normal-case">· tap several to combine</span></h3>
      <ScenePicker selected={picked} onToggle={toggle} gridClassName={gridClassName} className="flex-1" />
    </section>

    <div className="sticky bottom-0 -mx-1 flex flex-wrap items-center gap-2 rounded-xl border border-border bg-card/95 p-2.5 shadow-lg backdrop-blur">
      <Input value={name} onChange={e => setName(e.target.value)} placeholder={picked.length ? suggestion : 'Pick scenes to start'} disabled={!picked.length} className="h-9 min-w-[10rem] flex-1" aria-label="Preset name"
        onKeyDown={e => e.key === 'Enter' && void create(true)} />
      <Button variant="outline" size="sm" disabled={!picked.length || busy} onClick={() => void create(false)}><Plus className="h-4 w-4" />Save</Button>
      <Button size="sm" disabled={!picked.length || busy || !scenes} onClick={() => void create(true)}><Play className="h-4 w-4" />Create &amp; play{picked.length > 1 ? ` (${picked.length})` : ''}</Button>
    </div>
  </div>
}
