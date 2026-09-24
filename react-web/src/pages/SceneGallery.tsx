import { useMemo, useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Activity, Check, Cloud, EyeOff, Headphones, ImageOff, Monitor, Plus, Search, SlidersHorizontal, Star } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { Skeleton } from '~/components/ui/skeleton'
import { Input } from '~/components/ui/input'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import MatrixPreview from '~/components/scene-browser/MatrixPreview'
import type { ListScenes } from '~/apiTypes/list_scenes'
import { costLabel, intensityLabel, sceneArgumentsForVariant, sceneDisplayName } from '~/lib/sceneVariants'
import type { ListPresets, RawPreset, Scene } from '~/apiTypes/list_presets'
import { v4 as uuidv4 } from 'uuid'
import { toast } from 'sonner'
import AutomaticCuration from '~/components/scene-browser/AutomaticCuration'
import { automaticLooks, preferenceFor, type AutomaticLook, type AutomaticPreferences, type AutomaticState } from '~/lib/automaticCuration'

type RotationFilter = 'all' | 'rotation' | 'favorites' | 'off'
const rotationFilters: { value: RotationFilter; label: string }[] = [
  { value: 'all', label: 'Everything' },
  { value: 'rotation', label: 'In rotation' },
  { value: 'favorites', label: 'Favorites' },
  { value: 'off', label: 'Off in auto' },
]

export default function SceneGallery() {
  const apiUrl = useApiUrl()
  const navigate = useNavigate()
  const [query, setQuery] = useState('')
  const [category, setCategory] = useState('All')
  const [selectedName, setSelectedName] = useState<string | null>(null)
  const [targetPreset, setTargetPreset] = useState('')
  const [selectedVariant, setSelectedVariant] = useState('')
  const { data: scenes, isLoading } = useFetch<ListScenes[]>('/list_scenes')
  const { data: presets } = useFetch<ListPresets>('/list_presets')
  const { data: fetchedPreferences, setData: setPreferences } = useFetch<AutomaticPreferences>('/automatic_director/preferences')
  const preferences = useMemo(() => fetchedPreferences ?? {}, [fetchedPreferences])
  const [rotation, setRotation] = useState<RotationFilter>('all')
  const [pendingLook, setPendingLook] = useState<string | null>(null)
  const looksByScene = useMemo(() => new Map((scenes ?? []).map(s => [s.name, automaticLooks(s, preferences)])), [scenes, preferences])
  const allLooks = useMemo(() => Array.from(looksByScene.values()).flat(), [looksByScene])
  const inRotation = allLooks.filter(look => look.state !== 'off').length

  const categories = useMemo(() => ['All', ...Array.from(new Set((scenes ?? []).map(s => s.category ?? 'General'))).sort()], [scenes])
  const filtered = useMemo(() => (scenes ?? []).filter(s =>
    (category === 'All' || (s.category ?? 'General') === category)
    && matchesRotation(looksByScene.get(s.name) ?? [], rotation)
    && [s.name, s.descriptor?.family, ...(s.descriptor?.tags ?? [])].join(' ').toLowerCase().includes(query.trim().toLowerCase())
  ), [scenes, category, query, rotation, looksByScene])
  const selected = (scenes ?? []).find(s => s.name === selectedName) ?? filtered[0] ?? null

  const setAutomaticState = async (look: AutomaticLook, state: AutomaticState) => {
    if (!apiUrl) return
    setPendingLook(look.key)
    try {
      const response = await fetch(`${apiUrl}/automatic_director/preferences`, {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ look: look.key, preference: preferenceFor(look, state) }),
      })
      if (!response.ok) throw new Error(await response.text() || 'Could not save preference')
      setPreferences(await response.json())
    } catch (error) { toast.error(error instanceof Error ? error.message : 'Could not save preference') }
    finally { setPendingLook(null) }
  }

  const addToPreset = async () => {
    if (!apiUrl || !selected || !targetPreset) return
    try {
      const response = await fetch(`${apiUrl}/presets?id=${encodeURIComponent(targetPreset)}`)
      if (!response.ok) throw new Error('Could not load preset')
      const preset: RawPreset = await response.json()
      const args = sceneArgumentsForVariant(selected, selectedVariant)
      const scene: Scene = { uuid: uuidv4(), type: selected.name, arguments: args, variant: selectedVariant || undefined }
      const save = await fetch(`${apiUrl}/preset?id=${encodeURIComponent(targetPreset)}`, {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ...preset, scenes: [...(preset.scenes ?? []), scene] }),
      })
      if (!save.ok) throw new Error('Could not save preset')
      toast.success(`${selected.name} added`)
      navigate(`/modify-preset/${encodeURIComponent(targetPreset)}`)
    } catch (error) { toast.error(error instanceof Error ? error.message : 'Failed to add scene') }
  }

  return <div className="space-y-6 pb-24 lg:pb-6">
    <div className="flex flex-col gap-4 sm:flex-row sm:items-end sm:justify-between">
      <div><Badge variant="outline" className="mb-2">Scene library</Badge><h1 className="text-3xl font-bold tracking-tight">Find your next scene</h1><p className="mt-1 text-sm text-muted-foreground">Browse visually, inspect settings, and add directly to a preset.</p></div>
      <div className="text-right text-sm text-muted-foreground"><div>{scenes?.length ?? 0} installed scenes</div>{allLooks.length > 0 && <div className="text-xs">{inRotation} of {allLooks.length} looks in Automatic rotation</div>}</div>
    </div>

    <div className="grid gap-5 xl:grid-cols-[minmax(0,1fr)_380px]">
      <section className="glass-panel min-w-0 overflow-hidden rounded-2xl">
        <div className="space-y-3 border-b border-border p-4">
          <div className="relative"><Search className="absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" /><Input className="pl-9" placeholder="Search by scene name…" value={query} onChange={e => setQuery(e.target.value)} /></div>
          <div className="flex gap-2 overflow-x-auto pb-1">{categories.map(c => <Button key={c} size="sm" variant={category === c ? 'default' : 'outline'} className="shrink-0 rounded-full" onClick={() => setCategory(c)}>{c}</Button>)}</div>
          <div className="flex gap-2 overflow-x-auto pb-1">{rotationFilters.map(f => <Button key={f.value} size="sm" variant={rotation === f.value ? 'secondary' : 'ghost'} className="shrink-0 rounded-full text-xs" onClick={() => setRotation(f.value)}>{f.label}</Button>)}</div>
        </div>
        <div className="max-h-[calc(100vh-285px)] overflow-y-auto p-4">
          {isLoading ? <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 lg:grid-cols-4">{Array.from({length: 12}).map((_,i) => <Skeleton key={i} className="aspect-[4/5] rounded-xl" />)}</div> :
          <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 lg:grid-cols-4">{filtered.map(scene => {
            const active = selected?.name === scene.name
            return <button key={scene.name} data-selected={active} className="scene-tile" onClick={() => { setSelectedName(scene.name); setSelectedVariant('') }}>
              <div className="relative aspect-square bg-black">{scene.has_preview ? <img src={`${apiUrl}/scene_preview?name=${encodeURIComponent(scene.name)}`} alt="" className="h-full w-full object-contain [image-rendering:pixelated]" /> : <div className="flex h-full items-center justify-center text-white/30"><ImageOff className="h-7 w-7" /></div>}{active && <span className="absolute right-2 top-2 rounded-full bg-primary p-1 text-primary-foreground"><Check className="h-3 w-3" /></span>}<RotationBadge looks={looksByScene.get(scene.name) ?? []} /></div>
              <div className="p-3"><div className="truncate text-sm font-semibold">{sceneDisplayName(scene.name)}</div><div className="mt-1 flex justify-between text-[11px] text-muted-foreground"><span>{scene.descriptor?.family ?? scene.category}</span><span className="flex items-center gap-1"><SlidersHorizontal className="h-3 w-3" />{scene.properties.length}</span></div></div>
            </button>})}</div>}
        </div>
      </section>

      <aside className="space-y-4 xl:sticky xl:top-8 xl:self-start">
        <MatrixPreview apiUrl={apiUrl ?? ''} sceneName={selected?.name} hasPreview={selected?.has_preview} />
        {selected && <AutomaticCuration looks={looksByScene.get(selected.name) ?? []} pending={pendingLook} onChange={setAutomaticState} />}
        <div className="glass-panel rounded-2xl p-4">
          {selected ? <><div className="flex items-start justify-between gap-3"><div><h2 className="text-lg font-bold">{sceneDisplayName(selected.name)}</h2><p className="text-xs text-muted-foreground">{selected.descriptor?.family ?? selected.category}</p></div><Badge>{selected.descriptor?.variants.length ?? 0} looks</Badge></div>
          <div className="mt-3 flex flex-wrap gap-1.5">
            {selected.capabilities?.requires_audio && <Badge variant="secondary" className="gap-1"><Headphones className="h-3 w-3" />Audio</Badge>}
            {selected.capabilities?.requires_desktop && <Badge variant="secondary" className="gap-1"><Monitor className="h-3 w-3" />Desktop</Badge>}
            {selected.capabilities?.requires_network && <Badge variant="secondary" className="gap-1"><Cloud className="h-3 w-3" />Network</Badge>}
            {selected.capabilities?.supports_audio && !selected.capabilities?.requires_audio && <Badge variant="outline" className="gap-1"><Activity className="h-3 w-3" />Audio reactive</Badge>}
          </div>
          {selected.descriptor && <div className="mt-4 grid grid-cols-3 gap-2 text-center text-xs"><div className="rounded-lg bg-secondary/50 p-2"><div className="font-semibold">{intensityLabel(selected.descriptor.intensity)}</div><div className="text-muted-foreground">energy</div></div><div className="rounded-lg bg-secondary/50 p-2"><div className="font-semibold">{Math.round(selected.descriptor.motion * 100)}%</div><div className="text-muted-foreground">motion</div></div><div className="rounded-lg bg-secondary/50 p-2"><div className="font-semibold">{costLabel(selected.descriptor.performance_cost)}</div><div className="text-muted-foreground">Pi load</div></div></div>}
          <div className="mt-3 flex flex-wrap gap-1.5">{(selected.descriptor?.tags ?? []).slice(0,8).map(tag => <Badge key={tag} variant="outline" className="font-normal">{tag}</Badge>)}</div>
          {(selected.descriptor?.variants.length ?? 0) > 0 && <div className="mt-4 space-y-1.5"><label className="text-xs font-medium text-muted-foreground">Curated look</label><select className="flex h-10 w-full rounded-md border border-input bg-background px-3 text-sm" value={selectedVariant} onChange={e => setSelectedVariant(e.target.value)}><option value="">Original</option>{selected.descriptor?.variants.map(variant => <option key={variant.id} value={variant.id}>{variant.label}</option>)}</select>{selectedVariant && <p className="text-xs text-muted-foreground">{selected.descriptor?.variants.find(v => v.id === selectedVariant)?.description}</p>}</div>}
          <div className="mt-5 space-y-2"><label className="text-xs font-medium text-muted-foreground">Add to preset</label><select className="flex h-10 w-full rounded-md border border-input bg-background px-3 text-sm" value={targetPreset} onChange={e => setTargetPreset(e.target.value)}><option value="">Choose a preset…</option>{Object.entries(presets ?? {}).map(([id,p]) => <option key={id} value={id}>{p.display_name ?? id}</option>)}</select><Button className="w-full gap-2" disabled={!targetPreset} onClick={addToPreset}><Plus className="h-4 w-4" />Add curated look</Button></div></> : <p className="text-sm text-muted-foreground">No scenes match this filter.</p>}
        </div>
      </aside>
    </div>
  </div>
}

function matchesRotation(looks: AutomaticLook[], filter: RotationFilter) {
  if (filter === 'all') return true
  if (filter === 'rotation') return looks.some(look => look.state !== 'off')
  if (filter === 'favorites') return looks.some(look => look.state === 'favorite')
  return looks.length > 0 && looks.some(look => look.state === 'off')
}

function RotationBadge({ looks }: { looks: AutomaticLook[] }) {
  if (looks.length === 0) return null
  const favorites = looks.filter(look => look.state === 'favorite').length
  const off = looks.filter(look => look.state === 'off').length
  if (favorites > 0) return <span className="absolute left-2 top-2 flex items-center gap-1 rounded-full bg-black/70 px-1.5 py-0.5 text-[10px] font-medium text-amber-300"><Star className="h-3 w-3 fill-current" />{favorites}</span>
  if (off === looks.length) return <span className="absolute left-2 top-2 flex items-center gap-1 rounded-full bg-black/70 px-1.5 py-0.5 text-[10px] font-medium text-white/70"><EyeOff className="h-3 w-3" />Off in auto</span>
  if (off > 0) return <span className="absolute left-2 top-2 rounded-full bg-black/70 px-1.5 py-0.5 text-[10px] font-medium text-white/70">{looks.length - off}/{looks.length} in auto</span>
  return null
}
