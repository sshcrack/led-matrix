import { useEffect, useMemo, useState } from 'react'
import { Activity, AudioLines, BrainCircuit, CircleAlert, Network, PlugZap, RefreshCw, Shuffle } from 'lucide-react'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import { Badge } from '~/components/ui/badge'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { toast } from 'sonner'

interface DiagnosticsData {
  uptime_seconds: number
  desktop_connections: number
  renderer: {
    active_scene: string
    frames: number
    fps: number
    render_ms_average: number
    render_ms_max: number
    slow_frames: number
    scene_errors: Record<string, { count: number; last_error: string }>
    scene_performance: Record<string, {
      frames: number
      render_ms_average: number
      render_ms_p50: number
      render_ms_p95: number
      render_ms_p99: number
      render_ms_max: number
      target_fps: number
      frame_budget_ms: number
      p95_budget_ratio: number
      offload_pressure: boolean
      slow_frames: number
      quality_scale: number
    }>
  }
  udp: {
    datagrams: number
    packets: number
    bytes: number
    datagrams_per_second: number
    bytes_per_second: number
    malformed: number
    unhandled: number
  }
  audio_transport: { packets: number; sequence_gaps: number; decode_errors: number; last_sequence: number | null }
  audio: {
    available: boolean
    fresh: boolean
    age_seconds: number
    sequence: number
    bpm: number
    beat_phase: number
    beat_confidence: number
    beat_strength: number
    tempo_stability: number
    loudness: number
    rms: number
    kick: number
    snare: number
    hihat: number
    onset: number
    stereo_width: number
    stereo_balance: number
    spectral_centroid: number
    energy_trend: number
    drop: number
    section_change: number
    bands: Record<string, number>
  }
  registry: { ok: boolean; errors: string[]; warnings: string[] }
  plugins: { name: string; location: string }[]
  operation_mode: string
  automatic_mode: boolean
  configured_director_seed: string
  director?: {
    seed: string
    decision_count: number
    render_quality: number
    last_scene: string
    last_variant: string
    last_score: number
    last_reasons: string[]
    context: {
      audio_available: boolean
      spotify_available: boolean
      loudness: number
      target_intensity: number
      performance_budget: number
      excluded_scene: string
    }
    history: Array<{ scene: string; family: string; variant: string }>
    candidates: Array<{ scene: string; variant: string; score: number; reasons: string[] }>
  }
  render_placement?: {
    scene?: string
    placement?: 'local' | 'desktop_pending' | 'desktop' | 'local_fallback' | string
    reason?: string
    frame_budget_ms?: number
    local_render_ms?: number
    quality_scale?: number
  }
  remote_render?: {
    requested: boolean
    frame_fresh: boolean
    session: number
    last_sequence: number
    frame_age_ms: number
    scene: string
    renderer: string
  }
  transition?: {
    from?: string
    from_variant?: string
    to?: string
    to_variant?: string
    name?: string
    duration_ms?: number
    start_delay_ms?: number
    beat_synchronized?: boolean
    reason?: string
  }
  scene_lab?: { active: boolean; scene: string; variant: string; fps: number; missing_inputs: string[] }
}

function number(value: number | undefined, digits = 1) {
  return Number.isFinite(value) ? Number(value).toFixed(digits) : '—'
}
function bytes(value: number) {
  if (value > 1024 * 1024) return `${number(value / (1024 * 1024), 2)} MB/s`
  if (value > 1024) return `${number(value / 1024, 1)} KB/s`
  return `${number(value, 0)} B/s`
}

function Metric({ label, value, detail }: { label: string; value: string; detail?: string }) {
  return <div className="rounded-xl border border-border/70 bg-background/55 p-4">
    <div className="text-xs font-medium uppercase tracking-[0.12em] text-muted-foreground">{label}</div>
    <div className="mt-1 text-2xl font-bold tabular-nums">{value}</div>
    {detail && <div className="mt-1 text-xs text-muted-foreground">{detail}</div>}
  </div>
}

export default function Diagnostics() {
  const apiUrl = useApiUrl()
  const [data, setData] = useState<DiagnosticsData | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [paused, setPaused] = useState(false)
  const [seedInput, setSeedInput] = useState('')
  const [seedEdited, setSeedEdited] = useState(false)

  useEffect(() => {
    if (data?.configured_director_seed && !seedEdited) setSeedInput(data.configured_director_seed)
  }, [data?.configured_director_seed, seedEdited])

  useEffect(() => {
    if (!apiUrl || paused) return
    let disposed = false
    let timer: ReturnType<typeof setTimeout> | undefined
    const poll = async () => {
      try {
        const response = await fetch(`${apiUrl}/diagnostics`, { cache: 'no-store' })
        if (!response.ok) throw new Error(`HTTP ${response.status}`)
        const next = await response.json() as DiagnosticsData
        if (!disposed) { setData(next); setError(null) }
      } catch (e) {
        if (!disposed) setError(e instanceof Error ? e.message : String(e))
      } finally {
        if (!disposed) timer = setTimeout(poll, 1000)
      }
    }
    void poll()
    return () => { disposed = true; if (timer) clearTimeout(timer) }
  }, [apiUrl, paused])

  const sceneErrors = useMemo(() => data ? Object.entries(data.renderer.scene_errors) : [], [data])
  const scenePerformance = useMemo(() => data
    ? Object.entries(data.renderer.scene_performance ?? {}).sort(([, a], [, b]) => b.render_ms_p95 - a.render_ms_p95)
    : [], [data])

  const setDirectorSeed = async (seed?: string) => {
    if (!apiUrl) return
    try {
      const response = await fetch(`${apiUrl}/automatic_director/seed`, {
        method: 'POST', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(seed ? { seed } : {}),
      })
      const body = await response.json() as { seed?: string; error?: string }
      if (!response.ok || !body.seed) throw new Error(body.error || `HTTP ${response.status}`)
      setSeedInput(body.seed); setSeedEdited(false)
      toast.success(seed ? 'Director seed applied' : 'New Director seed generated')
    } catch (e) { toast.error(e instanceof Error ? e.message : 'Could not update Director seed') }
  }

  return <div className="space-y-6 pb-24 lg:pb-8">
    <div className="flex flex-wrap items-center justify-between gap-3">
      <div>
        <div className="flex items-center gap-2"><Activity className="h-5 w-5 text-primary" /><h1 className="text-2xl font-bold">Diagnostics</h1></div>
        <p className="mt-1 text-sm text-muted-foreground">Live renderer, transport, audio-analysis and plugin health.</p>
      </div>
      <div className="flex items-center gap-2">
        {data && <Badge variant={data.registry.ok ? 'secondary' : 'destructive'}>{data.registry.ok ? 'Registry healthy' : 'Registry errors'}</Badge>}
        <Button variant="outline" size="sm" onClick={() => setPaused(value => !value)}><RefreshCw className={`mr-2 h-4 w-4 ${!paused ? 'animate-spin [animation-duration:3s]' : ''}`} />{paused ? 'Resume' : 'Pause'}</Button>
      </div>
    </div>

    {error && <div className="rounded-xl border border-destructive/40 bg-destructive/10 p-4 text-sm text-destructive">Could not read diagnostics: {error}</div>}
    {!data && !error && <div className="glass-panel rounded-2xl p-6 text-sm text-muted-foreground">Waiting for matrix diagnostics…</div>}

    {data && <>
      <section className="grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
        <Metric label="Renderer" value={`${number(data.renderer.fps)} FPS`} detail={`${number(data.renderer.render_ms_average, 2)} ms avg · ${number(data.renderer.render_ms_max, 2)} ms max`} />
        <Metric label="Active scene" value={data.renderer.active_scene || 'None'} detail={`${data.render_placement?.placement?.replace('_', ' ') ?? 'local'} · ${data.renderer.slow_frames} slow frames`} />
        <Metric label="UDP" value={`${number(data.udp.datagrams_per_second)} pkt/s`} detail={`${bytes(data.udp.bytes_per_second)} · ${data.udp.malformed} malformed`} />
        <Metric label="Desktop" value={`${data.desktop_connections} client${data.desktop_connections === 1 ? '' : 's'}`} detail={`${number(data.uptime_seconds / 60, 1)} min uptime`} />
      </section>

      <section className="grid gap-4 xl:grid-cols-[1.35fr_.65fr]">
        <div className="glass-panel rounded-2xl p-5">
          <div className="mb-4 flex flex-wrap items-center justify-between gap-2">
            <div className="flex items-center gap-2 font-semibold"><BrainCircuit className="h-4 w-4 text-primary" />Automatic Director</div>
            <Badge variant={data.scene_lab?.active ? 'outline' : data.automatic_mode ? 'secondary' : 'outline'}>{data.scene_lab?.active ? 'Scene Lab override' : data.automatic_mode ? 'Directing' : data.operation_mode === 'manual' ? 'Manual mode' : 'Paused by explicit automation'}</Badge>
          </div>
          {data.director?.seed ? <>
            <div className="grid gap-3 sm:grid-cols-3">
              <Metric label="Last choice" value={data.director.last_scene || 'None'} detail={data.director.last_variant || 'scene defaults'} />
              <Metric label="Decision score" value={number(data.director.last_score, 2)} detail={`${data.director.decision_count} seeded decisions`} />
              <Metric label="Pi headroom" value={`${number(data.director.render_quality * 100, 0)}%`} detail={`budget ${number(data.director.context?.performance_budget * 100, 0)}%`} />
            </div>
            {data.director.last_reasons?.length > 0 && <div className="mt-4 flex flex-wrap gap-2">{data.director.last_reasons.map(reason => <Badge key={reason} variant="outline">{reason}</Badge>)}</div>}
            <div className="mt-4 overflow-x-auto">
              <div className="min-w-[620px] space-y-1 text-xs">
                <div className="grid grid-cols-[1.2fr_.9fr_.55fr_2fr] gap-3 px-3 py-1 font-medium uppercase tracking-[0.08em] text-muted-foreground"><span>Candidate</span><span>Variant</span><span>Score</span><span>Why</span></div>
                {(data.director.candidates ?? []).slice(0, 6).map(candidate => <div key={`${candidate.scene}:${candidate.variant}`} className="grid grid-cols-[1.2fr_.9fr_.55fr_2fr] gap-3 rounded-lg bg-secondary/50 px-3 py-2"><span className="font-medium">{candidate.scene}</span><span>{candidate.variant || 'default'}</span><span className="tabular-nums">{number(candidate.score, 2)}</span><span className="truncate text-muted-foreground" title={candidate.reasons.join(' · ')}>{candidate.reasons.join(' · ')}</span></div>)}
              </div>
            </div>
          </> : <div className="text-sm text-muted-foreground">No Automatic Director decision has been made in this process yet.</div>}
        </div>

        <div className="space-y-4">
          <div className="glass-panel rounded-2xl p-5">
            <div className="mb-3 flex items-center gap-2 font-semibold"><Shuffle className="h-4 w-4 text-primary" />Reproduce sequence</div>
            <p className="mb-3 text-xs text-muted-foreground">The seed is persisted. Reusing it resets the Director to the same random sequence for the same runtime context.</p>
            <Input value={seedInput} inputMode="numeric" onChange={event => { setSeedInput(event.target.value.replace(/\D/g, '')); setSeedEdited(true) }} aria-label="Automatic Director seed" />
            <div className="mt-2 grid grid-cols-2 gap-2"><Button size="sm" variant="outline" disabled={!seedInput || seedInput === '0'} onClick={() => void setDirectorSeed(seedInput)}>Apply seed</Button><Button size="sm" variant="outline" onClick={() => void setDirectorSeed()}><Shuffle className="mr-1.5 h-3.5 w-3.5" />New seed</Button></div>
          </div>
          <div className="glass-panel rounded-2xl p-5 text-sm">
            <div className="mb-3 font-semibold">Last handoff</div>
            {data.transition?.name ? <div className="space-y-2"><div><span className="font-medium">{data.transition.from}{data.transition.from_variant ? ` · ${data.transition.from_variant}` : ''}</span><span className="text-muted-foreground"> → </span><span className="font-medium">{data.transition.to}{data.transition.to_variant ? ` · ${data.transition.to_variant}` : ''}</span></div><div className="text-muted-foreground">{data.transition.name} · {data.transition.duration_ms} ms{data.transition.start_delay_ms ? ` · ${data.transition.start_delay_ms} ms beat wait` : ''}</div><div className="text-xs text-muted-foreground">{data.transition.reason}</div></div> : <div className="text-muted-foreground">No automatic transition recorded yet.</div>}
            {data.scene_lab?.active && <div className="mt-4 rounded-lg border border-primary/25 bg-primary/5 px-3 py-2 text-xs">Scene Lab owns playback: {data.scene_lab.scene}{data.scene_lab.variant ? ` · ${data.scene_lab.variant}` : ''} at {data.scene_lab.fps} FPS.</div>}
          </div>
        </div>
      </section>

      <section className="glass-panel rounded-2xl p-5">
        <div className="flex flex-wrap items-start justify-between gap-3">
          <div>
            <div className="font-semibold">Render placement</div>
            <p className="mt-1 max-w-3xl text-sm text-muted-foreground">{data.render_placement?.reason ?? 'Waiting for the renderer to make a placement decision.'}</p>
          </div>
          <Badge variant={data.render_placement?.placement === 'desktop' ? 'secondary' : 'outline'}>
            {data.render_placement?.placement?.replace('_', ' ') ?? 'local'}
          </Badge>
        </div>
        <div className="mt-4 grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
          <Metric label="Frame budget" value={`${number(data.render_placement?.frame_budget_ms, 2)} ms`} detail="Auto offload starts at sustained 72% pressure" />
          <Metric label="Local quality" value={`${number((data.render_placement?.quality_scale ?? 1) * 100, 0)}%`} detail="Adaptive before/while desktop warms" />
          <Metric label="Remote frame" value={data.remote_render?.requested ? (data.remote_render.frame_fresh ? 'Fresh' : 'Waiting') : 'Inactive'} detail={data.remote_render?.requested ? `${number(data.remote_render.frame_age_ms, 0)} ms old · seq ${data.remote_render.last_sequence}` : 'Latest-frame-wins, no queued latency'} />
          <Metric label="Remote renderer" value={data.remote_render?.renderer || 'None'} detail={data.remote_render?.scene ? `Scene ${data.remote_render.scene} · session ${data.remote_render.session}` : 'Desktop is used only when measured load warrants it'} />
        </div>
      </section>

      {scenePerformance.length > 0 && <section className="glass-panel rounded-2xl p-5">
        <div className="mb-3 flex items-center justify-between gap-3">
          <div className="font-semibold">Scene performance</div>
          <div className="text-xs text-muted-foreground">Sorted by measured Pi p95; orange pressure means Auto may offload when a desktop is connected.</div>
        </div>
        <div className="overflow-x-auto">
          <div className="min-w-[640px] space-y-1 text-xs">
            <div className="grid grid-cols-[1.5fr_repeat(6,.7fr)] gap-3 px-3 py-1 font-medium uppercase tracking-[0.08em] text-muted-foreground">
              <span>Scene</span><span>P50</span><span>P95</span><span>P99</span><span>Max</span><span>Budget load</span><span>Quality</span>
            </div>
            {scenePerformance.map(([scene, stats]) => <div key={scene} className={`grid grid-cols-[1.5fr_repeat(6,.7fr)] gap-3 rounded-lg px-3 py-2 tabular-nums ${stats.offload_pressure ? 'bg-amber-500/10 ring-1 ring-inset ring-amber-500/20' : 'bg-secondary/50'}`}>
              <span className="truncate font-medium" title={scene}>{scene}</span>
              <span>{number(stats.render_ms_p50, 2)} ms</span>
              <span>{number(stats.render_ms_p95, 2)} ms</span>
              <span>{number(stats.render_ms_p99, 2)} ms</span>
              <span>{number(stats.render_ms_max, 2)} ms</span>
              <span title={`${number(stats.frame_budget_ms, 2)} ms budget @ ${stats.target_fps} FPS`}>{number(stats.p95_budget_ratio * 100, 0)}%{stats.offload_pressure ? ' · pressure' : ''}</span>
              <span>{number(stats.quality_scale * 100, 0)}%{stats.slow_frames > 0 ? ` · ${stats.slow_frames} slow` : ''}</span>
            </div>)}
          </div>
        </div>
      </section>}

      <section className="grid gap-4 xl:grid-cols-[1.25fr_.75fr]">
        <div className="glass-panel rounded-2xl p-5">
          <div className="mb-4 flex items-center justify-between"><div className="flex items-center gap-2 font-semibold"><AudioLines className="h-4 w-4 text-primary" />Music analysis</div><Badge variant={data.audio.fresh ? 'secondary' : 'outline'}>{data.audio.fresh ? 'Live' : data.audio.available ? 'Stale' : 'No signal'}</Badge></div>
          <div className="grid gap-3 sm:grid-cols-3">
            <Metric label="Tempo" value={`${number(data.audio.bpm)} BPM`} detail={`${number(data.audio.beat_confidence * 100, 0)}% confidence · ${number(data.audio.tempo_stability * 100, 0)}% stable`} />
            <Metric label="Beat phase" value={`${number(data.audio.beat_phase * 100, 0)}%`} detail={`strength ${number(data.audio.beat_strength, 2)}`} />
            <Metric label="Audio age" value={`${number(data.audio.age_seconds * 1000, 0)} ms`} detail={`seq ${data.audio.sequence}`} />
          </div>
          <div className="mt-5 grid grid-cols-7 gap-2">
            {Object.entries(data.audio.bands).map(([name, value]) => <div key={name} className="min-w-0 text-center">
              <div className="relative mx-auto h-28 w-full max-w-9 overflow-hidden rounded-full bg-secondary"><div className="absolute inset-x-0 bottom-0 rounded-full bg-primary transition-[height] duration-150" style={{ height: `${Math.min(100, Math.max(2, value * 100))}%` }} /></div>
              <div className="mt-2 truncate text-[10px] text-muted-foreground" title={name}>{name.replace('_', ' ')}</div>
            </div>)}
          </div>
          <div className="mt-5 grid grid-cols-2 gap-3 sm:grid-cols-4">
            <Metric label="Kick" value={number(data.audio.kick, 2)} />
            <Metric label="Snare" value={number(data.audio.snare, 2)} />
            <Metric label="Hi-hat" value={number(data.audio.hihat, 2)} />
            <Metric label="Onset" value={number(data.audio.onset, 2)} />
          </div>
        </div>

        <div className="space-y-4">
          <div className="glass-panel rounded-2xl p-5">
            <div className="mb-3 flex items-center gap-2 font-semibold"><Network className="h-4 w-4 text-primary" />Transport</div>
            <div className="space-y-2 text-sm">
              <div className="flex justify-between"><span className="text-muted-foreground">Audio packets</span><span className="tabular-nums">{data.audio_transport.packets}</span></div>
              <div className="flex justify-between"><span className="text-muted-foreground">Sequence gaps</span><span className="tabular-nums">{data.audio_transport.sequence_gaps}</span></div>
              <div className="flex justify-between"><span className="text-muted-foreground">Decode errors</span><span className="tabular-nums">{data.audio_transport.decode_errors}</span></div>
              <div className="flex justify-between"><span className="text-muted-foreground">Unhandled UDP</span><span className="tabular-nums">{data.udp.unhandled}</span></div>
            </div>
          </div>
          <div className="glass-panel rounded-2xl p-5">
            <div className="mb-3 flex items-center gap-2 font-semibold"><PlugZap className="h-4 w-4 text-primary" />Plugins <Badge variant="outline">{data.plugins.length}</Badge></div>
            <div className="max-h-48 space-y-1 overflow-y-auto pr-1 text-xs">{data.plugins.map(plugin => <div key={plugin.name} className="rounded-lg bg-secondary/60 px-3 py-2"><div className="font-medium">{plugin.name}</div><div className="truncate text-muted-foreground" title={plugin.location}>{plugin.location}</div></div>)}</div>
          </div>
        </div>
      </section>

      {(data.registry.errors.length > 0 || data.registry.warnings.length > 0 || sceneErrors.length > 0) && <section className="glass-panel rounded-2xl p-5">
        <div className="mb-3 flex items-center gap-2 font-semibold"><CircleAlert className="h-4 w-4 text-amber-500" />Issues</div>
        <div className="space-y-2 text-sm">
          {data.registry.errors.map((item, i) => <div key={`e${i}`} className="rounded-lg border border-destructive/30 bg-destructive/10 px-3 py-2 text-destructive">{item}</div>)}
          {data.registry.warnings.map((item, i) => <div key={`w${i}`} className="rounded-lg border border-amber-500/25 bg-amber-500/10 px-3 py-2">{item}</div>)}
          {sceneErrors.map(([scene, info]) => <div key={scene} className="rounded-lg border border-border px-3 py-2"><span className="font-semibold">{scene}</span> <span className="text-muted-foreground">({info.count} errors)</span><div className="mt-1 text-xs text-muted-foreground">{info.last_error}</div></div>)}
        </div>
      </section>}
    </>}
  </div>
}
