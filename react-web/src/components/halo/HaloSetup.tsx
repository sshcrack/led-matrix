import { useRef, useState } from 'react'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import { initialHaloAlignment, maxLeds, wrap, type HaloAlignmentLayout } from './useHaloAlignment'
import type { HaloSettings } from './HaloCard'

export type HaloSetupSettings = Pick<HaloSettings,
  'led_count' | 'reverse' | 'first_led' | 'sample_columns' | 'sample_depth'>

const isIntegerIn = (value: number, min: number, max: number) => Number.isInteger(value) && value >= min && value <= max

// Slots run clockwise from the top-left corner of the display, as seen from the front.
function onFrame(index: number, count: number): [number, number] {
  const at = 4 * index / count
  if (at < 1) return [at, 0]
  if (at < 2) return [1, at - 1]
  if (at < 3) return [3 - at, 1]
  return [0, 4 - at]
}

export function HaloSetup({ settings, saving, testStatus, livePosition, onAlignmentChange, onSave }: {
  settings: HaloSettings
  saving: boolean
  testStatus: 'starting' | 'active' | 'unavailable' | null
  livePosition: boolean
  onAlignmentChange: (layout: HaloAlignmentLayout) => void
  onSave: (layout: HaloSetupSettings) => void
}) {
  const supportsSlots = settings.first_led !== undefined
  const { led_count: initialCount, first_led: initialSlot } = initialHaloAlignment(settings)
  const [layoutCount, setLayoutCount] = useState(initialCount)
  const [position, setPosition] = useState(initialSlot)
  const [selectedSlot, setSelectedSlot] = useState(settings.first_led !== undefined && settings.first_led >= 0)
  const dotButtons = useRef<(HTMLButtonElement | null)[]>([])
  const supportsSampling = settings.sample_columns !== undefined && settings.sample_depth !== undefined
  const [columns, setColumns] = useState(String(settings.sample_columns ?? 8))
  const [depth, setDepth] = useState(String(settings.sample_depth ?? 6))
  const [count, setCount] = useState(String(settings.led_count))
  const [reverse, setReverse] = useState(settings.reverse)
  const ledCount = Number(count)
  const sampleColumns = Number(columns)
  const sampleDepth = Number(depth)
  const validSampling = isIntegerIn(sampleColumns, 1, 16) && isIntegerIn(sampleDepth, 3, 24)
  const validCount = isIntegerIn(ledCount, 1, maxLeds)
  const valid = validCount && (!supportsSampling || validSampling)
  const next = wrap(position + (reverse ? -1 : 1), layoutCount)
  const selectSlot = (slot: number, focus = false) => {
    const index = wrap(slot, layoutCount)
    setPosition(index)
    setSelectedSlot(true)
    onAlignmentChange({ led_count: layoutCount, first_led: index, reverse })
    if (focus) dotButtons.current[index]?.focus()
  }
  const updateCount = (value: string) => {
    setCount(value)
    const nextCount = Number(value)
    if (!isIntegerIn(nextCount, 1, maxLeds) || nextCount === layoutCount) return
    const nextPosition = wrap(Math.round(position * nextCount / layoutCount), nextCount)
    setPosition(nextPosition)
    setLayoutCount(nextCount)
    onAlignmentChange({ led_count: nextCount, first_led: nextPosition, reverse })
  }

  return (
    <form className="space-y-5" onSubmit={e => {
      e.preventDefault()
      if (!valid || saving) return
      onSave({
        led_count: ledCount,
        reverse,
        ...(supportsSlots && selectedSlot ? { first_led: position } : {}),
        ...(supportsSampling ? { sample_columns: sampleColumns, sample_depth: sampleDepth } : {}),
      })
    }}>
      <div className="grid gap-5 sm:grid-cols-[auto_1fr]">
        <figure className="mx-auto w-full max-w-64 space-y-3 px-4 pt-3 sm:w-56">
          <div className="relative grid aspect-square w-full place-items-center rounded-sm border border-border bg-background"
            role="group" aria-label="First LED position">
            <span className="text-xs text-muted-foreground">Display front</span>
            {livePosition && <span aria-hidden="true" className="pointer-events-none absolute left-0 top-0 size-5 -translate-x-1/2 -translate-y-1/2 rounded-full ring-2 ring-blue-500" />}
            {Array.from({ length: layoutCount }, (_, index) => {
              const [x, y] = onFrame(index, layoutCount)
              const selected = index === position
              const size = Math.min(14, 275 / layoutCount) + '%'
              return (
                <button key={index} type="button" ref={el => { dotButtons.current[index] = el }}
                  aria-label={`LED position ${index + 1} of ${layoutCount}`} aria-pressed={selected}
                  tabIndex={selected ? 0 : -1} disabled={!supportsSlots}
                  style={{ left: x * 100 + '%', top: y * 100 + '%', width: size, height: size }}
                  className="absolute flex -translate-x-1/2 -translate-y-1/2 items-center justify-center rounded-full outline-none focus-visible:ring-2 focus-visible:ring-ring"
                  onClick={() => selectSlot(index)}
                  onKeyDown={e => {
                    if (!['ArrowRight', 'ArrowDown', 'ArrowLeft', 'ArrowUp'].includes(e.key)) return
                    e.preventDefault()
                    selectSlot(index + (e.key === 'ArrowRight' || e.key === 'ArrowDown' ? 1 : -1), true)
                  }}>
                  <span className={selected ? (livePosition ? 'size-2.5 shrink-0 rounded-full bg-red-500 ring-2 ring-red-500/30'
                    : 'size-2.5 shrink-0 rounded-full bg-primary ring-2 ring-primary/30')
                    : index === next ? (livePosition ? 'size-2 shrink-0 rounded-full bg-green-500'
                      : 'size-2 shrink-0 rounded-full bg-primary/60')
                      : 'size-1 shrink-0 rounded-full bg-muted-foreground/55'} />
                </button>
              )
            })}
          </div>
          <figcaption className="text-center text-xs text-muted-foreground">
            {supportsSlots ? 'Select where LED 0 sits.' : 'Update the daemon to select individual LED positions.'}
          </figcaption>
          {supportsSlots && (
            <div className="flex items-center justify-center gap-2">
              <Button type="button" variant="outline" className="h-10 w-10 px-0" aria-label="Previous LED position"
                onClick={() => selectSlot(position - 1)}>−</Button>
              <span className="min-w-14 text-center text-sm tabular-nums" aria-live="polite">{position + 1} / {layoutCount}</span>
              <Button type="button" variant="outline" className="h-10 w-10 px-0" aria-label="Next LED position"
                onClick={() => selectSlot(position + 1)}>+</Button>
            </div>
          )}
        </figure>
        <div className="space-y-4">
          <div className="space-y-1.5">
            <Label htmlFor="halo-led-count">LED count</Label>
            <Input id="halo-led-count" type="number" inputMode="numeric" required min={1} max={maxLeds} step={1}
              value={count} onChange={e => updateCount(e.target.value)} className="h-10 text-base sm:text-sm" />
            <p className="text-xs text-muted-foreground">Count every LED connected to the strip input. Up to 120.</p>
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="halo-direction">Strip direction, viewed from the front</Label>
            <Select value={reverse ? 'reverse' : 'forward'} onValueChange={value => {
              const nextReverse = value === 'reverse'
              setReverse(nextReverse)
              setSelectedSlot(true)
              onAlignmentChange({ led_count: layoutCount, first_led: position, reverse: nextReverse })
            }}>
              <SelectTrigger id="halo-direction" className="h-10 text-base sm:text-sm"><SelectValue /></SelectTrigger>
              <SelectContent>
                <SelectItem value="forward">Clockwise</SelectItem>
                <SelectItem value="reverse">Counterclockwise</SelectItem>
              </SelectContent>
            </Select>
          </div>
        </div>
      </div>
      {testStatus && (
        <p className="text-sm text-muted-foreground" aria-live="polite">
          {testStatus === 'active' ? (livePosition
            ? 'Red marks LED 0, green shows the strip direction, and blue marks the top-left corner. Pick a dot to move the blue light.'
            : 'Alignment is on. White marks LED 0. Green shows the strip direction.')
            : testStatus === 'unavailable' ? 'Alignment test unavailable. Reconnecting…'
              : 'Starting the alignment test…'}
        </p>
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
