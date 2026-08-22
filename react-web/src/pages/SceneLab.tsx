import { useEffect, useMemo, useRef, useState } from 'react'
import { FlaskConical, Save, Square, WandSparkles } from 'lucide-react'
import { toast } from 'sonner'

import type { ListScenes } from '~/apiTypes/list_scenes'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'
import PropertyList from '~/components/modify-preset/property_list'
import { Badge } from '~/components/ui/badge'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { sceneArgumentsForVariant, sceneDisplayName } from '~/lib/sceneVariants'
import useFetch from '~/useFetch'

type LabStatus = {
  active: boolean
  session_id: number
  generation: number
  scene: string
  variant: string
  properties: Record<string, unknown>
  fps: number
  missing_inputs: string[]
}

export default function SceneLab() {
  const apiUrl = useApiUrl()
  const { data: scenes, setRetry: retryScenes } = useFetch<ListScenes[]>('/list_scenes')
  const { data: initialStatus } = useFetch<LabStatus>('/scene_lab')
  const [sceneName, setSceneName] = useState('')
  const [variant, setVariant] = useState('')
  const [args, setArgs] = useState<Record<string, unknown>>({})
  const [fps, setFps] = useState(20)
  const [active, setActive] = useState(false)
  const [missing, setMissing] = useState<string[]>([])
  const [variantLabel, setVariantLabel] = useState('My look')
  const [presetLabel, setPresetLabel] = useState('Scene Lab look')
  const updateReady = useRef(false)
  const activeRef = useRef(false)
  const sessionIdRef = useRef(0)
  const generationRef = useRef(0)
  const updateRevision = useRef(0)
  const updateChain = useRef<Promise<void>>(Promise.resolve())

  const definition = useMemo(() => scenes?.find(scene => scene.name === sceneName) ?? null, [scenes, sceneName])

  useEffect(() => {
    if (!initialStatus) return
    activeRef.current = initialStatus.active
    sessionIdRef.current = initialStatus.session_id ?? 0
    generationRef.current = initialStatus.generation ?? 0
    setActive(initialStatus.active)
    setMissing(initialStatus.missing_inputs ?? [])
    if (initialStatus.active) {
      setSceneName(initialStatus.scene)
      setVariant(initialStatus.variant ?? '')
      setArgs(initialStatus.properties ?? {})
      setFps(initialStatus.fps ?? 20)
      updateReady.current = true
    }
  }, [initialStatus])

  useEffect(() => {
    if (sceneName || !scenes?.length) return
    const first = scenes.find(scene => scene.descriptor?.automatic_eligible && scene.capabilities?.can_generate_preview !== false) ?? scenes[0]
    if (!first) return
    setSceneName(first.name)
    const firstVariant = first.descriptor?.variants?.[0]?.id ?? ''
    setVariant(firstVariant)
    setArgs(sceneArgumentsForVariant(first, firstVariant))
  }, [scenes, sceneName])

  const selectScene = (name: string) => {
    const next = scenes?.find(scene => scene.name === name)
    if (!next) return
    const nextVariant = next.descriptor?.variants?.[0]?.id ?? ''
    updateReady.current = false
    setSceneName(name)
    setVariant(nextVariant)
    setArgs(sceneArgumentsForVariant(next, nextVariant))
  }

  const selectVariant = (id: string) => {
    if (!definition) return
    updateReady.current = true
    setVariant(id)
    setArgs(sceneArgumentsForVariant(definition, id))
  }

  const post = async (path: string, body: Record<string, unknown> = {}) => {
    if (!apiUrl) throw new Error('Matrix is not connected')
    const response = await fetch(`${apiUrl}${path}`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
    })
    if (!response.ok) throw new Error((await response.text()) || 'Request failed')
    return await response.json()
  }

  const start = async () => {
    if (!definition) return
    try {
      const state = await post('/scene_lab/start', { scene: definition.name, variant, properties: args, fps }) as LabStatus
      activeRef.current = true
      sessionIdRef.current = state.session_id
      generationRef.current = state.generation
      setActive(true); setMissing(state.missing_inputs ?? []); updateReady.current = true
      toast.success('Scene Lab is live on the matrix')
    } catch (error) { toast.error(error instanceof Error ? error.message : 'Could not start Scene Lab') }
  }

  const stop = async () => {
    try {
      await post('/scene_lab/stop', { session_id: sessionIdRef.current })
      activeRef.current = false
      updateRevision.current += 1
      setActive(false); setMissing([]); updateReady.current = false
      toast.success('Normal playback resumed')
    } catch (error) { toast.error(error instanceof Error ? error.message : 'Could not stop Scene Lab') }
  }

  useEffect(() => {
    if (!active || !definition || !updateReady.current) return
    const revision = ++updateRevision.current
    const timer = window.setTimeout(() => {
      updateChain.current = updateChain.current.then(async () => {
        if (!activeRef.current || revision !== updateRevision.current) return
        try {
          const state = await post('/scene_lab/update', {
            variant, properties: args, fps, session_id: sessionIdRef.current,
            expected_generation: generationRef.current,
          }) as LabStatus
          generationRef.current = Math.max(generationRef.current, state.generation ?? 0)
          if (activeRef.current) setMissing(state.missing_inputs ?? [])
        } catch (error) {
          if (activeRef.current && revision === updateRevision.current)
            toast.error(error instanceof Error ? error.message : 'Temporary scene update failed')
        }
      })
    }, 250)
    return () => window.clearTimeout(timer)
  }, [active, definition, variant, args, fps])

  useEffect(() => {
    if (!active || !apiUrl) return
    const heartbeat = async () => {
      try {
        const response = await fetch(`${apiUrl}/scene_lab/heartbeat`, {
          method: 'POST', headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ session_id: sessionIdRef.current }),
        })
        if (response.status === 409) {
          activeRef.current = false
          updateRevision.current += 1
          updateReady.current = false
          setActive(false)
          toast.info('Scene Lab was replaced by another session')
          return
        }
        if (!response.ok) return
        const state = await response.json() as LabStatus
        // Heartbeats and debounced updates can cross in flight. Generation is
        // monotonic, so an older heartbeat response must never roll the client
        // back and make the next update look stale.
        generationRef.current = Math.max(generationRef.current, state.generation ?? 0)
        setMissing(state.missing_inputs ?? [])
        if (!state.active) {
          activeRef.current = false
          updateRevision.current += 1
          updateReady.current = false
          setActive(false)
          toast.info('Scene Lab session expired; normal playback resumed')
        }
      } catch { /* A transient connection loss should not end the local session early. */ }
    }
    void heartbeat()
    const interval = window.setInterval(() => void heartbeat(), 15000)
    return () => window.clearInterval(interval)
  }, [active, apiUrl])

  const saveVariant = async () => {
    try {
      const result = await post('/scene_lab/save_variant', { label: variantLabel, session_id: sessionIdRef.current }) as { generation?: number; variant?: { id?: string } }
      if (result.generation) generationRef.current = Math.max(generationRef.current, result.generation)
      toast.success('Saved as a reusable curated look')
      retryScenes(value => value + 1)
      if (result.variant?.id) setVariant(result.variant.id)
    } catch (error) { toast.error(error instanceof Error ? error.message : 'Could not save look') }
  }

  const savePreset = async () => {
    try {
      await post('/scene_lab/save_preset', { display_name: presetLabel, session_id: sessionIdRef.current })
      toast.success('Saved as a manual preset')
    } catch (error) { toast.error(error instanceof Error ? error.message : 'Could not save preset') }
  }

  return <div className="space-y-6 pb-24 lg:pb-8">
    <div className="flex flex-col gap-3 sm:flex-row sm:items-end sm:justify-between">
      <div>
        <Badge variant="outline" className="mb-2 gap-1.5"><FlaskConical className="h-3 w-3" />Pi renderer</Badge>
        <h1 className="text-3xl font-bold tracking-tight">Scene Lab</h1>
        <p className="mt-1 max-w-2xl text-sm text-muted-foreground">Try curated looks and tune a scene on the real matrix. Lab playback temporarily replaces normal playback and automatically expires if this page disappears.</p>
      </div>
      {active ? <Button variant="outline" className="gap-2" onClick={stop}><Square className="h-4 w-4" />Stop lab & resume</Button> : <Button className="gap-2" onClick={start} disabled={!definition}><WandSparkles className="h-4 w-4" />Run on matrix</Button>}
    </div>

    <div className="grid gap-5 xl:grid-cols-[minmax(0,1fr)_430px]">
      <div className="space-y-5">
        <section className="glass-panel rounded-2xl p-4 sm:p-5">
          <div className="grid gap-4 sm:grid-cols-2">
            <label className="space-y-1.5 text-sm"><span className="font-medium">Scene</span><select className="flex h-10 w-full rounded-md border border-input bg-background px-3 text-sm" value={sceneName} disabled={active} onChange={event => selectScene(event.target.value)}>{(scenes ?? []).map(scene => <option key={scene.name} value={scene.name}>{sceneDisplayName(scene.name)}</option>)}</select></label>
            <label className="space-y-1.5 text-sm"><span className="font-medium">Curated look</span><select className="flex h-10 w-full rounded-md border border-input bg-background px-3 text-sm" value={variant} onChange={event => selectVariant(event.target.value)}><option value="">Scene defaults</option>{definition?.descriptor?.variants.map(item => <option key={item.id} value={item.id}>{item.label}</option>)}</select></label>
            <label className="space-y-1.5 text-sm"><span className="font-medium">Lab render rate</span><select className="flex h-10 w-full rounded-md border border-input bg-background px-3 text-sm" value={fps} onChange={event => setFps(Number(event.target.value))}>{[10,15,20,24,30].map(value => <option key={value} value={value}>{value} FPS</option>)}</select></label>
            <div className="flex items-end"><div className="w-full rounded-xl bg-secondary/45 px-3 py-2.5 text-xs text-muted-foreground">Lab FPS is capped at 30 so experimentation yields to display stability.</div></div>
          </div>
          {missing.length > 0 && <div className="mt-4 rounded-xl border border-amber-500/30 bg-amber-500/10 px-4 py-3 text-sm text-amber-700 dark:text-amber-300">Waiting for: {missing.join(', ')}. The lab session stays safe and resumes rendering when the input returns.</div>}
        </section>

        <section className="glass-panel rounded-2xl p-4 sm:p-5">
          <div className="mb-4"><h2 className="font-semibold">Temporary controls</h2><p className="text-xs text-muted-foreground">Changes are applied to a fresh temporary scene and never touch your presets until you save.</p></div>
          {definition ? <PropertyList properties={definition.properties} arguments={args} providers={[]} onChange={setArgs} /> : <p className="text-sm text-muted-foreground">Choose a scene.</p>}
        </section>
      </div>

      <aside className="space-y-5 xl:sticky xl:top-8 xl:self-start">
        <LiveMatrixPreview apiUrl={apiUrl ?? ''} fallbackSceneName={definition?.name} fallbackHasPreview={definition?.has_preview} label={active ? `Scene Lab · ${sceneDisplayName(sceneName)}` : 'Matrix preview'} />
        <section className="glass-panel space-y-4 rounded-2xl p-4 sm:p-5">
          <div><h2 className="font-semibold">Keep this look</h2><p className="text-xs text-muted-foreground">Save the temporary settings only when you decide they are worth keeping.</p></div>
          <div className="space-y-2"><Input value={variantLabel} onChange={event => setVariantLabel(event.target.value)} placeholder="Look name" /><Button variant="outline" className="w-full gap-2" disabled={!active || !variantLabel.trim()} onClick={saveVariant}><Save className="h-4 w-4" />Save curated look</Button></div>
          <div className="border-t border-border pt-4 space-y-2"><Input value={presetLabel} onChange={event => setPresetLabel(event.target.value)} placeholder="Preset name" /><Button variant="outline" className="w-full gap-2" disabled={!active || !presetLabel.trim()} onClick={savePreset}><Save className="h-4 w-4" />Save as manual preset</Button></div>
        </section>
      </aside>
    </div>
  </div>
}
