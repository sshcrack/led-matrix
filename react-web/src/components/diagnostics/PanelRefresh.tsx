import { useState } from 'react'
import { Gauge } from 'lucide-react'
import { toast } from 'sonner'
import { Badge } from '~/components/ui/badge'
import { Switch } from '~/components/ui/switch'

export interface RefreshWindow {
  seconds: number
  refreshes?: number
  refresh_hz?: number
  median_us?: number
  p99_us?: number
  max_us?: number
  late_per_1000?: number
  interrupts_per_second?: { device_and_timer: number[] }
}

export interface RefreshState {
  refresh_cpu: number
  timing_available: boolean
  windows: Record<string, RefreshWindow>
  steady_brightness?: {
    enabled: boolean
    state: 'off' | 'calibrating' | 'on' | 'too_slow' | 'unavailable'
    limit_hz?: number
    fastest_refresh_us?: number
  }
}

const STATE_TEXT: Record<string, string> = {
  calibrating: 'Measuring the panel…',
  too_slow: 'This panel refreshes too slowly to pace it without visible flicker.',
  unavailable: 'Needs the LED panel (not available in the emulator).',
}

function ms(us: number | undefined) {
  return us == null ? '—' : `${(us / 1000).toFixed(2)} ms`
}

/** LED refresh health (/diagnostics → refresh) and the steady brightness switch. */
export function PanelRefresh({ apiUrl, refresh }: { apiUrl: string; refresh?: RefreshState }) {
  const [pending, setPending] = useState<boolean | null>(null)
  if (!refresh) return null
  const window = refresh.windows['60s'] ?? refresh.windows['10s']
  const steady = refresh.steady_brightness
  const enabled = pending ?? steady?.enabled ?? false

  const toggle = async (next: boolean) => {
    setPending(next)
    try {
      const response = await fetch(`${apiUrl}/steady_brightness`, {
        method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ enabled: next }),
      })
      if (!response.ok) throw new Error(`HTTP ${response.status}`)
    } catch (e) {
      toast.error(e instanceof Error ? e.message : 'Could not change steady brightness')
    } finally {
      // The diagnostics poll shows the monitor's state within a second or two.
      setTimeout(() => setPending(null), 2500)
    }
  }

  const note = steady ? (steady.state === 'on' && steady.limit_hz
    ? `Paced to ${steady.limit_hz} Hz; the fastest refresh took ${ms(steady.fastest_refresh_us)}.`
    : STATE_TEXT[steady.state]) : undefined

  return <section className="glass-panel rounded-2xl p-5">
    <div className="flex flex-wrap items-start justify-between gap-3">
      <div>
        <div className="flex items-center gap-2 font-semibold"><Gauge className="h-4 w-4 text-primary" />Panel refresh</div>
        <p className="mt-1 max-w-3xl text-sm text-muted-foreground">How evenly CPU {refresh.refresh_cpu} refreshes the LEDs{window ? ` over the last ${Math.round(window.seconds)} s` : ''}. A slower refresh shows as a dimmer panel, a single long one as a flicker.</p>
      </div>
      {window?.late_per_1000 != null && <Badge variant={window.late_per_1000 > 1 ? 'destructive' : 'secondary'}>{window.late_per_1000 > 1 ? 'Flicker' : 'Steady'}</Badge>}
    </div>
    {refresh.timing_available && window?.refreshes != null ? <div className="mt-4 grid gap-3 sm:grid-cols-2 xl:grid-cols-4">
      <Metric label="Refresh rate" value={`${window.refresh_hz?.toFixed(1)} Hz`} detail={`${window.refreshes} refreshes`} />
      <Metric label="Median" value={ms(window.median_us)} detail={`p99 ${ms(window.p99_us)}`} />
      <Metric label="Longest" value={ms(window.max_us)} detail={`${window.late_per_1000?.toFixed(2)} per 1000 over 1.5× median`} />
      <Metric label={`CPU ${refresh.refresh_cpu} interrupts`} value={`${Math.round(window.interrupts_per_second?.device_and_timer[refresh.refresh_cpu] ?? 0)}/s`} detail="device and timer" />
    </div> : <div className="mt-4 text-sm text-muted-foreground">{refresh.timing_available ? 'Collecting the first 10 seconds…' : 'No hardware refresh thread (emulator).'}</div>}
    <label className="mt-4 flex min-h-10 cursor-pointer items-start justify-between gap-4 rounded-xl border border-border/70 bg-background/55 p-4">
      <span>
        <span className="block text-sm font-medium">Steady brightness</span>
        <span className="mt-1 block text-xs text-muted-foreground">Uploads and SD card writes slow the refresh down, which dims the panel for a moment. This paces every refresh to a fixed rate a little below the fastest one, so the brightness stays even. The panel is about 15 % dimmer overall.</span>
        {enabled && note && <span className="mt-1 block text-xs">{note}</span>}
      </span>
      <Switch checked={enabled} onCheckedChange={next => void toggle(next)} aria-label="Steady brightness" />
    </label>
  </section>
}

function Metric({ label, value, detail }: { label: string; value: string; detail?: string }) {
  return <div className="rounded-xl border border-border/70 bg-background/55 p-4">
    <div className="text-xs font-medium uppercase tracking-[0.12em] text-muted-foreground">{label}</div>
    <div className="mt-1 text-2xl font-bold tabular-nums">{value}</div>
    {detail && <div className="mt-1 text-xs text-muted-foreground">{detail}</div>}
  </div>
}
