import { useEffect, useRef, useState } from 'react'
import { Lightbulb } from 'lucide-react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Card, CardContent } from '~/components/ui/card'
import { Label } from '~/components/ui/label'
import { Switch } from '~/components/ui/switch'
import { Button } from '~/components/ui/button'
import { Sheet } from '~/core/ui/Sheet'
import { HaloSetup, type HaloSetupSettings } from './HaloSetup'
import { useHaloAlignment } from './useHaloAlignment'
import { Skeleton } from '~/components/ui/skeleton'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'

/** Matches the daemon's GET /halo. */
export interface HaloSettings {
  enabled: boolean
  mode: 'fixed' | 'match'
  brightness: number
  color: string
  led_count: number
  device: string
  rotation: number
  reverse: boolean
  start_between_zones?: boolean
  first_led?: number
  sample_columns?: number
  sample_depth?: number
}

export interface HaloStatus {
  settings: HaloSettings
  error: string | null
  device_open: boolean
  frames_sent: number
  test_active?: boolean
}

type Patch = Partial<Pick<HaloSettings, 'enabled' | 'mode' | 'brightness' | 'color'> & HaloSetupSettings>

function HaloForm({ status, onSaved }: { status: HaloStatus, onSaved: (next: HaloStatus) => void }) {
  const apiUrl = useApiUrl()
  const s = status.settings
  const [saving, setSaving] = useState(false)
  const [setupOpen, setSetupOpen] = useState(false)
  const [testCount, setTestCount] = useState(s.led_count)
  const canTest = status.test_active !== undefined
  const testStatus = useHaloAlignment(setupOpen && canTest ? apiUrl : null, testCount)
  // Sliders and the colour picker fire continuously: show the local value and send it once it settles.
  const [draftBrightness, setDraftBrightness] = useState<number | null>(null)
  const [draftColor, setDraftColor] = useState<string | null>(null)
  const brightness = draftBrightness ?? s.brightness
  const color = draftColor ?? s.color
  const timer = useRef<ReturnType<typeof setTimeout> | undefined>(undefined)
  useEffect(() => () => clearTimeout(timer.current), [])

  const save = async (patch: Patch) => {
    if (!apiUrl) return
    setSaving(true)
    try {
      const res = await fetch(`${apiUrl}/halo`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(patch),
      })
      if (!res.ok) throw new Error(await res.text())
      onSaved((await res.json()) as HaloStatus)
      setDraftBrightness(null)
      setDraftColor(null)
      return true
    } catch {
      toast.error('Failed to save halo settings')
      return false
    } finally {
      setSaving(false)
    }
  }
  const later = (patch: Patch) => {
    clearTimeout(timer.current)
    timer.current = setTimeout(() => void save(patch), 300)
  }

  return (
    <CardContent className="p-6 space-y-5">
      <div className="flex items-center justify-between gap-3">
        <div className="flex items-center gap-3 min-w-0">
          <div className="w-10 h-10 shrink-0 rounded-xl bg-primary/10 flex items-center justify-center">
            <Lightbulb className="h-5 w-5 text-primary" />
          </div>
          <div className="min-w-0">
            <p className="font-medium">Rear halo light</p>
            <p className="text-sm text-muted-foreground">
              {!s.enabled ? 'Off' : s.mode === 'match' ? 'Following the display' : 'One fixed colour'}
            </p>
          </div>
        </div>
        <Switch checked={s.enabled} disabled={saving} aria-label="Rear halo light"
          onCheckedChange={(enabled) => void save({ enabled })} />
      </div>
      {s.enabled && status.error && <p className="text-sm text-destructive">{status.error}</p>}

      <div className="grid grid-cols-1 gap-4 sm:grid-cols-3">
        <div className="space-y-1.5">
          <Label htmlFor="halo-mode">Mode</Label>
          <Select value={s.mode} onValueChange={(mode) => void save({ mode: mode as HaloSettings['mode'] })} disabled={saving}>
            <SelectTrigger id="halo-mode" className="h-10 text-base sm:text-sm"><SelectValue /></SelectTrigger>
            <SelectContent>
              <SelectItem value="match">Follow the display</SelectItem>
              <SelectItem value="fixed">Fixed colour</SelectItem>
            </SelectContent>
          </Select>
        </div>
        <div className="space-y-1.5">
          <Label htmlFor="halo-brightness">Brightness {brightness}%</Label>
          <input id="halo-brightness" type="range" min={0} max={100} value={brightness}
            className="block h-10 w-full" aria-label="Halo brightness"
            onChange={(e) => { const v = Number(e.target.value); setDraftBrightness(v); later({ brightness: v }) }} />
        </div>
        {s.mode === 'fixed' && (
          <div className="space-y-1.5">
            <Label htmlFor="halo-color">Colour</Label>
            <input id="halo-color" type="color" value={color} className="block h-10 w-full rounded-md border bg-transparent"
              onChange={(e) => { setDraftColor(e.target.value); later({ color: e.target.value }) }} />
          </div>
        )}
      </div>
      <Button type="button" variant="outline" className="min-h-10" disabled={saving}
        onClick={() => { setTestCount(Math.min(s.led_count, 120)); setSetupOpen(true) }}>Set up halo strip</Button>
      <Sheet open={setupOpen} onOpenChange={setSetupOpen} title="Set up halo strip"
        description="Set the strip layout and how the halo samples the display.">
        {setupOpen && <HaloSetup settings={s} saving={saving}
          testStatus={canTest ? testStatus : null} onTestCountChange={setTestCount}
          onSave={async layout => { if (await save(layout)) setSetupOpen(false) }} />}
      </Sheet>
    </CardContent>
  )
}

export default function HaloCard() {
  const { data, error, isLoading, setData } = useFetch<HaloStatus>('/halo')
  if (isLoading && !data) return <Card><CardContent className="p-6"><Skeleton className="h-10 w-full" /></CardContent></Card>
  // An older daemon has no /halo: show nothing instead of an error.
  if (error || !data || !data.settings) return null
  return <Card><HaloForm status={data} onSaved={setData} /></Card>
}
