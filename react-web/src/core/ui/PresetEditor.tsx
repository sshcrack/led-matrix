import { useState } from 'react'
import { ArrowDown, ArrowUp, Plus, Trash2, Undo2, Blend } from 'lucide-react'
import { useNavigate } from 'react-router-dom'
import { toast } from 'sonner'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Skeleton } from '~/components/ui/skeleton'
import { Dialog, DialogContent, DialogDescription, DialogFooter, DialogHeader, DialogTitle } from '~/components/ui/dialog'
import PropertyList from '~/components/modify-preset/property_list'
import TransitionPicker from '~/components/modify-preset/TransitionPicker'
import PresetTransitionDuration from '~/components/modify-preset/PresetTransitionDuration'
import { sceneArgumentsForVariant, sceneDisplayName } from '~/lib/sceneVariants'
import type { Scene } from '~/apiTypes/list_presets'
import type { ListProviders } from '~/apiTypes/list_scenes'
import { cn } from '~/lib/utils'
import { useMatrix } from '../matrixContext'
import { useResource } from '../useResource'
import { usePresetEditor } from '../usePresetEditor'
import { collectSceneActions, usePluginUis } from '../pluginRegistry'
import { newSceneInstance } from '../scenes'
import { ScenePicker } from './ScenePicker'
import { SceneThumb } from './SceneThumb'
import { SaveBadge } from './controls'

export function AddScenesDialog({ open, onOpenChange, onAdd }: { open: boolean; onOpenChange: (o: boolean) => void; onAdd: (s: Scene[]) => void }) {
  const { sceneByName } = useMatrix()
  const [picked, setPicked] = useState<string[]>([])
  const add = () => {
    onAdd(picked.map(n => sceneByName.get(n)).filter((d): d is NonNullable<typeof d> => !!d).map(d => newSceneInstance(d)))
    setPicked([]); onOpenChange(false)
  }
  return <Dialog open={open} onOpenChange={o => { if (!o) setPicked([]); onOpenChange(o) }}>
    <DialogContent className="flex h-[88vh] max-w-4xl flex-col gap-3 p-4 sm:p-6">
      <DialogHeader><DialogTitle>Add scenes</DialogTitle><DialogDescription>Tap as many as you like, then add them all at once.</DialogDescription></DialogHeader>
      <ScenePicker selected={picked} onToggle={n => setPicked(p => p.includes(n) ? p.filter(x => x !== n) : [...p, n])} autoFocusSearch className="flex-1" />
      <DialogFooter className="items-center gap-2 sm:justify-between">
        <span className="text-sm text-muted-foreground">{picked.length ? `${picked.length} selected` : 'Nothing selected'}</span>
        <Button disabled={!picked.length} onClick={add}><Plus className="h-4 w-4" />Add {picked.length || ''} scene{picked.length === 1 ? '' : 's'}</Button>
      </DialogFooter>
    </DialogContent>
  </Dialog>
}

function SceneSettings({ scene, presetId, onChange }: { scene: Scene; presetId: string; onChange: (s: Scene) => void }) {
  const { sceneByName } = useMatrix()
  const navigate = useNavigate()
  const { data: providers } = useResource<ListProviders[]>('/list_providers')
  const actions = collectSceneActions(usePluginUis())
  const def = sceneByName.get(scene.type)
  if (!def) return <p className="text-sm text-muted-foreground">Unknown scene type “{scene.type}”.</p>
  const variants = def.descriptor?.variants ?? []
  return <div className="space-y-4">
    {variants.length > 0 && <div>
      <div className="mb-1.5 text-xs font-semibold uppercase tracking-wider text-muted-foreground">Look</div>
      <div className="flex flex-wrap gap-1.5">
        {[{ id: '', label: 'Original' }, ...variants].map(v => <button key={v.id} onClick={() => onChange({ ...scene, variant: v.id || undefined, arguments: sceneArgumentsForVariant(def, v.id) })}
          className={cn('rounded-full border px-3 py-1 text-xs font-medium transition', (scene.variant ?? '') === v.id ? 'border-primary bg-primary text-primary-foreground' : 'border-border hover:bg-secondary')}>{v.label}</button>)}
      </div>
      <p className="mt-1.5 text-xs text-muted-foreground">Switching look resets this scene’s settings to that baseline.</p>
    </div>}
    <PropertyList properties={def.properties} arguments={scene.arguments} providers={providers ?? []} onChange={args => onChange({ ...scene, arguments: args })} />
    {actions.filter(a => a.when?.({ presetId, scene, definition: def, navigate }) ?? true).map(a =>
      <Button key={a.id} variant="outline" className="w-full" onClick={() => a.run({ presetId, scene, definition: def, navigate })}>{a.icon}{a.label}</Button>)}
  </div>
}

/**
 * The preset editing surface shared by all variants (layout differs, behaviour doesn't):
 * autosave, multi-add, reorder, delete-with-undo, inline rename, transition settings.
 * `split` = scene list beside the inspector; `stack` = inspector expands under the selected scene.
 */
export function PresetEditor({ presetId, layout = 'split', header = true }: { presetId: string; layout?: 'split' | 'stack'; header?: boolean }) {
  const ed = usePresetEditor(presetId)
  const { preset } = ed
  const { sceneByName } = useMatrix()
  const { data: transitions } = useResource<string[]>('/list_transitions')
  const [selectedId, setSelectedId] = useState<string | null>(null)
  const [adding, setAdding] = useState(false)
  const [dismissedFor, setDismissedFor] = useState<string | null>(null)
  const [showTransition, setShowTransition] = useState(false)

  const [lastPreset, setLastPreset] = useState(presetId)
  if (lastPreset !== presetId) { setLastPreset(presetId); setSelectedId(null) }
  const selected = preset?.scenes.find(s => s.uuid === selectedId) ?? (layout === 'split' ? preset?.scenes[0] : undefined)

  // An empty preset opens the scene picker straight away (until dismissed) instead of showing a dead end.
  const showAdd = adding || (preset?.scenes.length === 0 && dismissedFor !== presetId)

  if (ed.loadError) return <div className="p-6 text-sm text-destructive">Could not load this preset.</div>
  if (!preset) return <div className="space-y-3 p-4"><Skeleton className="h-10" /><Skeleton className="h-24" /><Skeleton className="h-24" /></div>

  const remove = (s: Scene, index: number) => {
    ed.removeScene(s.uuid)
    toast(`Removed ${sceneDisplayName(s.type)}`, { action: { label: 'Undo', onClick: () => ed.restoreScene(s, index) }, icon: <Undo2 className="h-4 w-4" /> })
  }

  const list = <ul className="space-y-1.5">
    {preset.scenes.map((s, i) => {
      const on = s.uuid === selected?.uuid
      const def = sceneByName.get(s.type)
      return <li key={s.uuid} className={cn('rounded-xl border transition', on ? 'border-primary/60 bg-primary/5' : 'border-border bg-card')}>
        <div className="flex items-center gap-2 p-2">
          <button className="flex min-w-0 flex-1 items-center gap-3 text-left" onClick={() => setSelectedId(on && layout === 'stack' ? null : s.uuid)} aria-expanded={on}>
            <SceneThumb name={s.type} className="h-11 w-11 shrink-0 rounded-lg" />
            <span className="min-w-0"><span className="block truncate text-sm font-semibold">{sceneDisplayName(s.type)}</span>
              <span className="block truncate text-xs text-muted-foreground">{def?.descriptor?.variants.find(v => v.id === s.variant)?.label ?? def?.category ?? 'Unknown'}</span></span>
          </button>
          <div className="flex shrink-0 items-center">
            <Button variant="ghost" size="icon" className="h-8 w-8" disabled={i === 0} onClick={() => ed.moveScene(s.uuid, -1)} aria-label="Move up"><ArrowUp className="h-4 w-4" /></Button>
            <Button variant="ghost" size="icon" className="h-8 w-8" disabled={i === preset.scenes.length - 1} onClick={() => ed.moveScene(s.uuid, 1)} aria-label="Move down"><ArrowDown className="h-4 w-4" /></Button>
            <Button variant="ghost" size="icon" className="h-8 w-8 hover:text-destructive" onClick={() => remove(s, i)} aria-label={`Remove ${s.type}`}><Trash2 className="h-4 w-4" /></Button>
          </div>
        </div>
        {layout === 'stack' && on && <div className="border-t border-border/60 p-3"><SceneSettings scene={s} presetId={presetId} onChange={ed.updateScene} /></div>}
      </li>
    })}
    <li><Button variant="outline" className="h-11 w-full border-dashed" onClick={() => setAdding(true)}><Plus className="h-4 w-4" />Add scenes</Button></li>
  </ul>

  const transition = <div className="rounded-xl border border-border bg-card">
    <button className="flex w-full items-center gap-2 p-3 text-left text-sm font-medium" onClick={() => setShowTransition(v => !v)} aria-expanded={showTransition}>
      <Blend className="h-4 w-4 text-muted-foreground" />Transitions<span className="ml-auto text-xs font-normal text-muted-foreground">{preset.transition_name ?? 'blend'} · {((preset.transition_duration ?? 750) / 1000).toFixed(2)}s</span>
    </button>
    {showTransition && <div className="grid gap-4 border-t border-border/60 p-3 sm:grid-cols-2">
      {transitions && <TransitionPicker value={preset.transition_name ?? 'blend'} transitions={transitions} onChange={v => ed.setTransition({ transition_name: v })} />}
      <PresetTransitionDuration value={preset.transition_duration ?? 750} onChange={v => ed.setTransition({ transition_duration: v })} />
    </div>}
  </div>

  return <div className="flex min-h-0 flex-col gap-4">
    {header && <div className="flex items-center gap-3">
      <Input value={preset.display_name ?? ''} onChange={e => ed.setName(e.target.value)} aria-label="Preset name" className="h-10 border-transparent bg-transparent px-2 text-lg font-semibold hover:border-border focus-visible:border-input" />
      <SaveBadge state={ed.saveState} onRetry={() => void ed.retry()} className="shrink-0" />
    </div>}
    {layout === 'split'
      ? <div className="grid min-h-0 gap-4 lg:grid-cols-[minmax(0,20rem)_minmax(0,1fr)]">
        <div className="space-y-3">{list}{transition}</div>
        <div className="min-w-0 rounded-xl border border-border bg-card p-4">
          {selected ? <>
            <div className="mb-4 flex items-center gap-3"><SceneThumb name={selected.type} className="h-12 w-12 rounded-lg" /><div><h3 className="font-semibold">{sceneDisplayName(selected.type)}</h3><p className="text-xs text-muted-foreground">Changes apply as you go.</p></div></div>
            <SceneSettings key={selected.uuid} scene={selected} presetId={presetId} onChange={ed.updateScene} />
          </> : <p className="py-10 text-center text-sm text-muted-foreground">Add a scene to start.</p>}
        </div>
      </div>
      : <div className="space-y-3">{list}{transition}</div>}
    <AddScenesDialog open={showAdd} onOpenChange={o => { setAdding(o); if (!o) setDismissedFor(presetId) }} onAdd={scenes => { ed.addScenes(scenes); setSelectedId(scenes[0]?.uuid ?? null) }} />
  </div>
}
