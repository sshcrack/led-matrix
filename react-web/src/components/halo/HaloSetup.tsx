import { useState } from 'react'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import type { HaloSettings } from './HaloCard'

export type HaloSetupSettings = Pick<HaloSettings,
  'led_count' | 'rotation' | 'reverse' | 'start_between_zones' | 'sample_columns' | 'sample_depth'>

const positions = [
  ['Top edge, left half', 25, 0], ['Top edge, middle', 50, 0],
  ['Top edge, right half', 75, 0], ['Top-right corner', 100, 0],
  ['Right edge, upper half', 100, 25], ['Right edge, middle', 100, 50],
  ['Right edge, lower half', 100, 75], ['Bottom-right corner', 100, 100],
  ['Bottom edge, right half', 75, 100], ['Bottom edge, middle', 50, 100],
  ['Bottom edge, left half', 25, 100], ['Bottom-left corner', 0, 100],
  ['Left edge, lower half', 0, 75], ['Left edge, middle', 0, 50],
  ['Left edge, upper half', 0, 25], ['Top-left corner', 0, 0],
] as const

const wrap = (position: number) => (position + positions.length) % positions.length
const maxLeds = 120 // Stock Pro Micro firmware limit.

export function HaloSetup({ settings, saving, testing, onTest, onSave }: {
  settings: HaloSettings
  saving: boolean
  testing: boolean
  onTest?: (ledCount: number) => void
  onSave: (layout: HaloSetupSettings) => void
}) {
  const supportsMidpoints = settings.start_between_zones !== undefined
  const startAnchor = 2 * (settings.rotation - (settings.reverse ? 1 : 0))
  const initialPosition = wrap(startAnchor + (settings.start_between_zones ? (settings.reverse ? -1 : 1) : 0))
  const supportsSampling = settings.sample_columns !== undefined && settings.sample_depth !== undefined
  const [columns, setColumns] = useState(String(settings.sample_columns ?? 8))
  const [depth, setDepth] = useState(String(settings.sample_depth ?? 6))
  const [count, setCount] = useState(String(settings.led_count))
  const [position, setPosition] = useState(initialPosition)
  const [reverse, setReverse] = useState(settings.reverse)
  const ledCount = Number(count)
  const isIntegerIn = (value: number, min: number, max: number) => Number.isInteger(value) && value >= min && value <= max
  const sampleColumns = Number(columns)
  const sampleDepth = Number(depth)
  const validSampling = isIntegerIn(sampleColumns, 1, 16) && isIntegerIn(sampleDepth, 3, 24)
  const validCount = isIntegerIn(ledCount, 1, maxLeds)
  const valid = validCount && (!supportsSampling || validSampling)
  const step = supportsMidpoints ? 1 : 2
  const next = wrap(position + (reverse ? -step : step))

  return (
    <form className="space-y-5" onSubmit={e => {
      e.preventDefault()
      if (!valid || saving) return
      const anchor = reverse ? Math.ceil(position / 2) : Math.floor(position / 2)
      onSave({
        led_count: ledCount,
        rotation: (anchor + (reverse ? 1 : 0)) % 8,
        reverse,
        ...(supportsMidpoints ? { start_between_zones: position % 2 === 1 } : {}),
        ...(supportsSampling ? { sample_columns: sampleColumns, sample_depth: sampleDepth } : {}),
      })
    }}>
      <div className="grid gap-5 sm:grid-cols-[auto_1fr]">
        <figure className="mx-auto flex w-36 flex-col items-center gap-3 p-2 sm:mt-1">
          <div className="relative grid aspect-square w-full place-items-center rounded-sm border border-border bg-background" aria-hidden="true">
            <span className="text-xs text-muted-foreground">Display front</span>
            {positions.map(([label, x, y], index) => {
              if (index % step) return null
              return (
                <span key={label} style={{ left: x + '%', top: y + '%' }}
                  className={'absolute flex -translate-x-1/2 -translate-y-1/2 items-center justify-center rounded-full ' +
                    (index === position ? 'z-10 h-6 w-6 bg-primary text-xs font-semibold text-primary-foreground'
                      : index === next ? 'h-2.5 w-2.5 bg-primary/60' : 'h-1.5 w-1.5 bg-muted-foreground/50')}>
                  {index === position ? '0' : null}
                </span>
              )
            })}
          </div>
          <figcaption className="text-xs text-muted-foreground">0 marks the first LED</figcaption>
        </figure>
        <div className="space-y-4">
          <div className="space-y-1.5">
            <Label htmlFor="halo-led-count">LED count</Label>
            <Input id="halo-led-count" type="number" inputMode="numeric" required min={1} max={maxLeds} step={1}
              value={count} onChange={e => setCount(e.target.value)} className="h-10 text-base sm:text-sm" />
            <p className="text-xs text-muted-foreground">Count every LED connected to the strip input. Up to 120.</p>
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="halo-first-led">First LED position</Label>
            <Select value={String(position)} onValueChange={value => setPosition(Number(value))}>
              <SelectTrigger id="halo-first-led" className="h-10 text-base sm:text-sm"><SelectValue /></SelectTrigger>
              <SelectContent>
                {positions.map(([label], index) => index % step === 0
                  ? <SelectItem key={label} value={String(index)}>{label}</SelectItem> : null)}
              </SelectContent>
            </Select>
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="halo-direction">Strip direction, viewed from the front</Label>
            <Select value={reverse ? 'reverse' : 'forward'} onValueChange={value => setReverse(value === 'reverse')}>
              <SelectTrigger id="halo-direction" className="h-10 text-base sm:text-sm"><SelectValue /></SelectTrigger>
              <SelectContent>
                <SelectItem value="forward">Clockwise</SelectItem>
                <SelectItem value="reverse">Counterclockwise</SelectItem>
              </SelectContent>
            </Select>
          </div>
        </div>
      </div>
      {onTest && (
        <div className="flex flex-wrap items-center gap-x-3 gap-y-2">
          <Button type="button" variant="outline" className="min-h-10" disabled={saving || (!validCount && !testing)}
            onClick={() => onTest(testing ? 0 : ledCount)}>{testing ? 'Stop alignment test' : 'Test alignment'}</Button>
          <p className="text-sm text-muted-foreground" aria-live="polite">
            {testing ? 'White marks LED 0. Green shows the strip direction.' : 'Show the first LED and strip direction for 20 seconds.'}
          </p>
        </div>
      )}
      {supportsSampling && (
        <fieldset className="space-y-3 border-t border-border pt-3">
          <legend className="pr-2 text-sm font-medium">Display sampling</legend>
          <p className="text-sm text-muted-foreground">Used in Follow the display mode.</p>
          <div className="grid gap-4 sm:grid-cols-2">
            <div className="space-y-1.5">
              <Label htmlFor="halo-sample-columns">Samples per row</Label>
              <Input id="halo-sample-columns" type="number" inputMode="numeric" required min={1} max={16} step={1}
                value={columns} onChange={e => setColumns(e.target.value)} className="h-10 text-base sm:text-sm" />
              <p className="text-xs text-muted-foreground">Two rows per zone.{isIntegerIn(sampleColumns, 1, 16) && ` ${sampleColumns * 2} pixels total per zone.`}</p>
            </div>
            <div className="space-y-1.5">
              <Label htmlFor="halo-sample-depth">Inner row depth (pixels)</Label>
              <Input id="halo-sample-depth" type="number" inputMode="numeric" required min={3} max={24} step={1}
                value={depth} onChange={e => setDepth(e.target.value)} className="h-10 text-base sm:text-sm" />
              <p className="text-xs text-muted-foreground">Distance from the edge. The outer row stays at 2 pixels.</p>
            </div>
          </div>
        </fieldset>
      )}
      <div className="flex justify-end">
        <Button type="submit" disabled={saving || !valid} className="min-h-10">Save halo setup</Button>
      </div>
    </form>
  )
}
