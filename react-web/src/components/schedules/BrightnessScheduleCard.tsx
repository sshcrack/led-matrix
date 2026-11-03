import { useState } from 'react'
import { Moon } from 'lucide-react'
import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { Card, CardContent } from '~/components/ui/card'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Switch } from '~/components/ui/switch'
import { Skeleton } from '~/components/ui/skeleton'

/** Matches the server's ConfigData::BrightnessSchedule struct */
export interface BrightnessSchedule {
  enabled: boolean
  day_brightness: number
  night_brightness: number
  night_start_hour: number
  night_start_minute: number
  night_end_hour: number
  night_end_minute: number
  fade_minutes: number
}

type BrightnessScheduleResponse = BrightnessSchedule & { current_brightness: number }

const pad = (n: number) => String(n).padStart(2, '0')
const toTime = (hour: number, minute: number) => `${pad(hour)}:${pad(minute)}`
const fromTime = (value: string): [number, number] => {
  const [h, m] = value.split(':').map((part) => Number.parseInt(part, 10))
  return [Number.isFinite(h) ? h : 0, Number.isFinite(m) ? m : 0]
}
const clamp = (value: number, min: number, max: number) => Math.min(max, Math.max(min, Number.isFinite(value) ? value : min))

function BrightnessScheduleForm({ initial, currentBrightness, onSaved }: {
  initial: BrightnessSchedule
  currentBrightness: number
  onSaved: (saved: BrightnessSchedule) => void
}) {
  const apiUrl = useApiUrl()
  const [draft, setDraft] = useState<BrightnessSchedule>(initial)
  const [saving, setSaving] = useState(false)
  const update = (patch: Partial<BrightnessSchedule>) => setDraft((prev) => ({ ...prev, ...patch }))

  const save = async (next: BrightnessSchedule) => {
    if (!apiUrl) return
    setSaving(true)
    try {
      const res = await fetch(`${apiUrl}/brightness_schedule`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(next),
      })
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const saved = (await res.json()) as BrightnessSchedule
      setDraft(saved)
      onSaved(saved)
      toast.success('Brightness schedule saved')
    } catch {
      toast.error('Failed to save brightness schedule')
    } finally {
      setSaving(false)
    }
  }

  const dirty = JSON.stringify(draft) !== JSON.stringify(initial)

  return (
    <CardContent className="p-6 space-y-4">
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div className="w-10 h-10 rounded-xl bg-primary/10 flex items-center justify-center">
            <Moon className="h-5 w-5 text-primary" />
          </div>
          <div>
            <p className="font-medium">Night Brightness</p>
            <p className="text-sm text-muted-foreground">
              Dim the panel overnight with a gentle fade
              {initial.enabled && currentBrightness > 0 ? ` · currently ${currentBrightness}%` : ''}
            </p>
          </div>
        </div>
        <Switch
          checked={draft.enabled}
          disabled={saving}
          onCheckedChange={(enabled) => {
            const next = { ...draft, enabled }
            setDraft(next)
            void save(next)
          }}
        />
      </div>

      {draft.enabled && (
        <div className="grid grid-cols-2 gap-4 sm:grid-cols-3">
          <div className="space-y-1.5">
            <Label htmlFor="night-start">Night starts</Label>
            <Input
              id="night-start"
              type="time"
              value={toTime(draft.night_start_hour, draft.night_start_minute)}
              onChange={(e) => {
                const [h, m] = fromTime(e.target.value)
                update({ night_start_hour: h, night_start_minute: m })
              }}
            />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="night-end">Night ends</Label>
            <Input
              id="night-end"
              type="time"
              value={toTime(draft.night_end_hour, draft.night_end_minute)}
              onChange={(e) => {
                const [h, m] = fromTime(e.target.value)
                update({ night_end_hour: h, night_end_minute: m })
              }}
            />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="fade-minutes">Fade (min)</Label>
            <Input
              id="fade-minutes"
              type="number"
              min={0}
              max={180}
              value={draft.fade_minutes}
              onChange={(e) => update({ fade_minutes: clamp(e.target.valueAsNumber, 0, 180) })}
            />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="day-brightness">Day brightness (%)</Label>
            <Input
              id="day-brightness"
              type="number"
              min={1}
              max={100}
              value={draft.day_brightness}
              onChange={(e) => update({ day_brightness: clamp(e.target.valueAsNumber, 1, 100) })}
            />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="night-brightness">Night brightness (%)</Label>
            <Input
              id="night-brightness"
              type="number"
              min={1}
              max={100}
              value={draft.night_brightness}
              onChange={(e) => update({ night_brightness: clamp(e.target.valueAsNumber, 1, 100) })}
            />
          </div>
          <div className="flex items-end">
            <Button className="w-full" disabled={!dirty || saving} onClick={() => void save(draft)}>
              {saving ? 'Saving…' : 'Save'}
            </Button>
          </div>
        </div>
      )}
    </CardContent>
  )
}

export default function BrightnessScheduleCard() {
  const { data, error, isLoading, setData } = useFetch<BrightnessScheduleResponse>('/brightness_schedule')

  return (
    <Card>
      {isLoading && !data ? (
        <CardContent className="p-6"><Skeleton className="h-10 w-full" /></CardContent>
      ) : error || !data ? (
        <CardContent className="p-6 text-sm text-muted-foreground">Could not load brightness schedule.</CardContent>
      ) : (
        <BrightnessScheduleForm
          key={JSON.stringify(data)}
          initial={data}
          currentBrightness={data.current_brightness}
          onSaved={(saved) => setData({ ...saved, current_brightness: data.current_brightness })}
        />
      )}
    </Card>
  )
}
