import { useCallback, useEffect, useRef, useState } from 'react'
import { ChevronDown, LayoutGrid } from 'lucide-react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Switch } from '~/components/ui/switch'
import { Skeleton } from '~/components/ui/skeleton'
import { Collapsible, CollapsibleContent, CollapsibleTrigger } from '~/components/ui/collapsible'
import { Sheet } from '~/core/ui/Sheet'
import {
  GPIO_MAPPINGS, LABELS, LAYOUTS, PANEL_TYPES, RANGES, RGB_SEQUENCES, ROTATIONS,
  describe, layoutId, sameOptions, validate, type HardwareStatus, type NumberKey, type PanelOptions,
} from './panelHardwareModel'

const selectClass = 'flex h-10 w-full rounded-md border border-input bg-background px-3 text-base sm:text-sm'
const inputClass = 'h-10 text-base sm:text-sm'

/** Shows the `panel_test` pattern through Scene Lab while `on`; the session ends when switched off or unmounted. */
function useTestPattern(apiUrl: string | null, on: boolean, epoch: number) {
  useEffect(() => {
    if (!apiUrl || !on) return
    let session: string | null = null
    let cancelled = false
    const post = (path: string, body: object) => fetch(`${apiUrl}${path}`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
    })
    void post('/scene_lab/start', { scene: 'panel_test', fps: 10 })
      .then(async res => {
        if (!res.ok) throw new Error(await res.text())
        const state = await res.json() as { session_id?: string }
        session = state.session_id ?? null
        if (cancelled && session) void post('/scene_lab/stop', { session_id: session })
      })
      .catch(() => { if (!cancelled) toast.error('Could not show the test pattern') })
    const beat = window.setInterval(() => { if (session) void post('/scene_lab/heartbeat', { session_id: session }).catch(() => {}) }, 15000)
    return () => {
      cancelled = true
      window.clearInterval(beat)
      if (session) void post('/scene_lab/stop', { session_id: session }).catch(() => {})
    }
  }, [apiUrl, on, epoch])
}

function NumberField({ k, value, onChange }: { k: NumberKey; value: number; onChange: (v: number) => void }) {
  const [low, high] = RANGES[k]
  return <div className="space-y-1.5">
    <Label htmlFor={`hw-${k}`}>{LABELS[k]}</Label>
    <Input id={`hw-${k}`} type="number" inputMode="numeric" min={low} max={high} step={1} className={inputClass}
      value={Number.isNaN(value) ? '' : value} onChange={e => onChange(e.target.value === '' ? NaN : Number(e.target.value))} />
  </div>
}

function HardwareForm({ status, busy, onApply }: {
  status: HardwareStatus; busy: boolean; onApply: (o: PanelOptions) => void
}) {
  const [o, setO] = useState<PanelOptions>(status.running)
  const set = <K extends keyof PanelOptions>(k: K, v: PanelOptions[K]) => setO(prev => ({ ...prev, [k]: v }))
  const error = validate(o)
  const unchanged = sameOptions(o, status.running)
  const layout = layoutId(o)
  const customMapper = !ROTATIONS.includes(o.pixel_mapper)

  return <div className="space-y-5">
    <div className="grid grid-cols-1 gap-4 sm:grid-cols-2">
      <div className="space-y-1.5 sm:col-span-2">
        <Label htmlFor="hw-layout">Panels</Label>
        <select id="hw-layout" className={selectClass} value={layout} onChange={e => {
          const l = LAYOUTS.find(x => x.id === e.target.value)
          if (l) setO(prev => ({ ...prev, chain: l.chain, parallel: l.parallel }))
        }}>
          {LAYOUTS.map(l => <option key={l.id} value={l.id}>{l.label}</option>)}
          {layout === 'custom' && <option value="custom">{o.chain} per chain × {o.parallel} chains</option>}
        </select>
      </div>
      <NumberField k="cols" value={o.cols} onChange={v => set('cols', v)} />
      <NumberField k="rows" value={o.rows} onChange={v => set('rows', v)} />
      <NumberField k="chain" value={o.chain} onChange={v => set('chain', v)} />
      <NumberField k="parallel" value={o.parallel} onChange={v => set('parallel', v)} />
      <div className="space-y-1.5">
        <Label htmlFor="hw-rotation">Rotation</Label>
        <select id="hw-rotation" className={selectClass} value={customMapper ? 'custom' : o.pixel_mapper}
          onChange={e => set('pixel_mapper', e.target.value === 'custom' ? o.pixel_mapper : e.target.value)}>
          {ROTATIONS.map(r => <option key={r} value={r}>{r === '' ? 'None' : `${r.slice(7)}°`}</option>)}
          {customMapper && <option value="custom">Custom mapper</option>}
        </select>
      </div>
      <div className="space-y-1.5">
        <Label htmlFor="hw-gpio">Wiring</Label>
        <select id="hw-gpio" className={selectClass} value={o.gpio_mapping} onChange={e => set('gpio_mapping', e.target.value)}>
          {GPIO_MAPPINGS.map(m => <option key={m.value} value={m.value}>{m.label}</option>)}
        </select>
      </div>
      <NumberField k="slowdown" value={o.slowdown} onChange={v => set('slowdown', v)} />
    </div>

    <Collapsible>
      <CollapsibleTrigger className="group flex min-h-10 items-center gap-2 text-sm font-medium">
        <ChevronDown className="h-4 w-4 transition-transform group-data-[state=open]:rotate-180" />Advanced
      </CollapsibleTrigger>
      <CollapsibleContent className="grid grid-cols-1 gap-4 pt-3 sm:grid-cols-2">
        <div className="space-y-1.5 sm:col-span-2">
          <Label htmlFor="hw-mapper">Pixel mapper</Label>
          <Input id="hw-mapper" className={inputClass} placeholder="e.g. U-mapper;Rotate:90" value={o.pixel_mapper}
            onChange={e => set('pixel_mapper', e.target.value.trim())} />
        </div>
        <NumberField k="multiplexing" value={o.multiplexing} onChange={v => set('multiplexing', v)} />
        <NumberField k="row_addr_type" value={o.row_addr_type} onChange={v => set('row_addr_type', v)} />
        <div className="space-y-1.5">
          <Label htmlFor="hw-panel-type">Panel driver chip</Label>
          <select id="hw-panel-type" className={selectClass} value={o.panel_type} onChange={e => set('panel_type', e.target.value)}>
            {PANEL_TYPES.map(t => <option key={t} value={t}>{t === '' ? 'Standard' : t}</option>)}
          </select>
        </div>
        <div className="space-y-1.5">
          <Label htmlFor="hw-rgb">Colour order</Label>
          <select id="hw-rgb" className={selectClass} value={o.rgb_sequence} onChange={e => set('rgb_sequence', e.target.value)}>
            {RGB_SEQUENCES.map(s => <option key={s} value={s}>{s}</option>)}
          </select>
        </div>
        <NumberField k="pwm_bits" value={o.pwm_bits} onChange={v => set('pwm_bits', v)} />
        <NumberField k="pwm_lsb_nanoseconds" value={o.pwm_lsb_nanoseconds} onChange={v => set('pwm_lsb_nanoseconds', v)} />
        <NumberField k="pwm_dither_bits" value={o.pwm_dither_bits} onChange={v => set('pwm_dither_bits', v)} />
      </CollapsibleContent>
    </Collapsible>

    {error ? <p className="text-sm text-destructive">{error}</p> : <p className="text-sm text-muted-foreground">{describe(o)}</p>}
    <div className="flex flex-wrap gap-2">
      <Button type="button" className="min-h-10" disabled={busy || !!error || unchanged} onClick={() => onApply(o)}>
        Apply and restart
      </Button>
      <Button type="button" variant="outline" className="min-h-10" disabled={busy || unchanged} onClick={() => setO(status.running)}>
        Reset
      </Button>
    </div>
    <p className="text-xs text-muted-foreground">
      The panel restarts with the new settings. If it does not come back within 90 seconds, the previous settings are restored.
    </p>
  </div>
}

export default function PanelHardwareCard() {
  const apiUrl = useApiUrl()
  const { data, error, isLoading, setData } = useFetch<HardwareStatus>('/hardware')
  const [open, setOpen] = useState(false)
  const [pattern, setPattern] = useState(false)
  const [waiting, setWaiting] = useState(false)
  const [epoch, setEpoch] = useState(0)
  const poll = useRef<number | undefined>(undefined)
  useEffect(() => () => window.clearTimeout(poll.current), [])
  useTestPattern(apiUrl, pattern && !waiting, epoch)

  const post = useCallback(async (options: PanelOptions) => {
    const res = await fetch(`${apiUrl}/hardware`, {
      method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ options }),
    })
    if (!res.ok) throw new Error((await res.text()) || 'Request failed')
    return await res.json() as HardwareStatus
  }, [apiUrl])

  // Follow the restart until the helper has written a new result.
  const waitForResult = useCallback((previousAt: string | undefined) => {
    const deadline = Date.now() + 180_000
    const tick = async () => {
      try {
        const res = await fetch(`${apiUrl}/hardware`)
        if (res.ok) {
          const next = await res.json() as HardwareStatus
          if (!next.pending && next.result && next.result.at !== previousAt) {
            setData(next); setWaiting(false); setEpoch(e => e + 1)
            if (next.result.state === 'applied') toast.success('The panel restarted with the new settings')
            else toast.error(next.result.message)
            return
          }
        }
      } catch { /* the daemon is restarting */ }
      if (Date.now() > deadline) { setWaiting(false); toast.error('The panel did not report back; check it and reload this page'); return }
      poll.current = window.setTimeout(() => void tick(), 2000)
    }
    poll.current = window.setTimeout(() => void tick(), 2000)
  }, [apiUrl, setData])

  const apply = async (options: PanelOptions) => {
    if (!data) return
    setWaiting(true)
    try {
      const next = await post(options)
      setData(next)
      if (!next.restart) { setWaiting(false); return }
      waitForResult(data.result?.at)
    } catch (e) {
      setWaiting(false)
      toast.error(e instanceof Error ? e.message : 'Could not change the panel settings')
    }
  }

  const confirm = async () => {
    if (!data) return
    try { setData(await post(data.running)) } catch { toast.error('Could not save') }
  }

  if (isLoading && !data) return <Card><CardContent className="p-6"><Skeleton className="h-10 w-full" /></CardContent></Card>
  // An older daemon has no /hardware.
  if (error || !data || !data.running) return null

  return <Card id="panel-hardware">
    <CardContent className="p-6 space-y-4">
      <div className="flex items-center gap-3 min-w-0">
        <div className="w-10 h-10 shrink-0 rounded-xl bg-primary/10 flex items-center justify-center">
          <LayoutGrid className="h-5 w-5 text-primary" />
        </div>
        <div className="min-w-0">
          <p className="font-medium">Panel hardware</p>
          <p className="text-sm text-muted-foreground">{describe(data.running)}</p>
        </div>
      </div>
      {data.setup_needed && <p className="rounded-lg border border-primary/40 bg-primary/10 px-3 py-2 text-sm">
        New panel: show the test pattern and check the layout. Red belongs in the top-left corner and the arrow points up.
      </p>}
      {waiting && <p className="text-sm text-muted-foreground">The panel is restarting with the new settings…</p>}
      {!waiting && data.result?.state === 'failed' && <p className="text-sm text-destructive">{data.result.message}</p>}
      <div className="flex items-center justify-between gap-3">
        <Label htmlFor="hw-pattern" className="text-sm">Show test pattern</Label>
        <Switch id="hw-pattern" checked={pattern} onCheckedChange={setPattern} />
      </div>
      {pattern && <p className="text-xs text-muted-foreground">
        Red, green, yellow and blue corners clockwise from the top-left, a white frame around the edge and the arrow pointing up mean the layout is right.
        A broken diagonal points to the multiplexing or row address type; the cyan line sweeps across the chain from left to right.
      </p>}
      <div className="flex flex-wrap gap-2">
        <Button type="button" variant="outline" className="min-h-10" disabled={waiting || !data.supported}
          onClick={() => setOpen(true)}>Change panel settings</Button>
        {data.setup_needed && <Button type="button" className="min-h-10" disabled={waiting} onClick={() => void confirm()}>Looks right</Button>}
      </div>
      {!data.supported && <p className="text-xs text-muted-foreground">{data.unsupported_reason || 'This matrix cannot change its panel settings.'}</p>}
      <Sheet open={open} onOpenChange={setOpen} title="Panel hardware"
        description="How the panels are wired to the Pi. Leave the test pattern on while you change these.">
        {open && <HardwareForm status={data} busy={waiting} onApply={o => { setOpen(false); void apply(o) }} />}
      </Sheet>
    </CardContent>
  </Card>
}
