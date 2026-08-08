import { Plus, Trash2 } from 'lucide-react'
import type { Property, TypeId } from '~/apiTypes/list_scenes'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import { Switch } from '~/components/ui/switch'
import { propertyDescription, propertyLabel } from '../propertyUi'

type AudioBinding = {
  property: string
  feature: string
  min: number
  max: number
  smoothing?: number
  curve?: number
  invert?: boolean
}

const numericTypes = new Set<TypeId>(['int', 'double', 'float', 'int16_t', 'uint8_t'])
const reservedProperties = new Set(['weight', 'duration', 'transition_duration', 'audio_modulations'])

const audioFeatures = [
  ['rms', 'RMS'], ['peak', 'Peak'], ['loudness', 'Loudness'],
  ['loudness_fast', 'Loudness (fast)'], ['loudness_slow', 'Loudness (slow)'],
  ['sub_bass', 'Sub bass'], ['bass', 'Bass'], ['low_mid', 'Low mids'], ['mid', 'Mids'],
  ['high_mid', 'High mids'], ['treble', 'Treble'], ['air', 'Air'],
  ['spectral_centroid', 'Spectral centroid'], ['spectral_rolloff', 'Spectral rolloff'],
  ['spectral_flatness', 'Spectral flatness'], ['spectral_flux', 'Spectral flux'],
  ['onset_strength', 'Onset strength'], ['kick', 'Kick'], ['snare', 'Snare'], ['hihat', 'Hi-hat'],
  ['stereo_width', 'Stereo width'], ['stereo_balance', 'Stereo balance'],
  ['stereo_correlation', 'Stereo correlation'], ['energy_trend', 'Energy trend'],
  ['section_change', 'Section change'], ['drop', 'Drop'], ['bpm', 'BPM'],
  ['beat_phase', 'Beat phase'], ['beat_confidence', 'Beat confidence'],
  ['beat_strength', 'Beat strength'], ['tempo_stability', 'Tempo stability'], ['silence', 'Silence'],
] as const

function asBindings(value: unknown): AudioBinding[] {
  if (!Array.isArray(value)) return []
  return value.flatMap((item): AudioBinding[] => {
    if (typeof item !== 'object' || item === null || Array.isArray(item)) return []
    const record = item as Record<string, unknown>
    if (typeof record.property !== 'string' || typeof record.feature !== 'string') return []
    const min = Number(record.min)
    const max = Number(record.max)
    if (!Number.isFinite(min) || !Number.isFinite(max)) return []
    return [{
      property: record.property,
      feature: record.feature,
      min,
      max,
      smoothing: Number.isFinite(Number(record.smoothing)) ? Number(record.smoothing) : 0.12,
      curve: Number.isFinite(Number(record.curve)) ? Number(record.curve) : 1,
      invert: Boolean(record.invert),
    }]
  })
}

function targetRange(property: Property<unknown>) {
  const fallback = typeof property.default_value === 'number' ? property.default_value : 1
  const configuredMin = typeof property.additional?.min === 'number' ? property.additional.min : undefined
  const configuredMax = typeof property.additional?.max === 'number' ? property.additional.max : undefined
  const low = configuredMin ?? (fallback === 0 ? 0 : fallback * 0.5)
  const high = configuredMax ?? (fallback === 0 ? 1 : fallback * 1.5)
  return { low, high }
}

interface AudioModulationPropertyProps {
  property: Property<unknown>
  value: unknown
  properties: Property<unknown>[]
  onChange: (value: AudioBinding[]) => void
}

export default function AudioModulationProperty({ property, value, properties, onChange }: AudioModulationPropertyProps) {
  const bindings = asBindings(value)
  const targets = properties.filter(candidate =>
    numericTypes.has(candidate.type_id) && !reservedProperties.has(candidate.name))
  const usedTargets = new Set(bindings.map(binding => binding.property))

  const update = (index: number, patch: Partial<AudioBinding>) => {
    onChange(bindings.map((binding, current) => current === index ? { ...binding, ...patch } : binding))
  }

  const addBinding = () => {
    const target = targets.find(candidate => !usedTargets.has(candidate.name)) ?? targets[0]
    if (!target) return
    const range = targetRange(target)
    onChange([...bindings, {
      property: target.name,
      feature: 'bass',
      min: range.low,
      max: range.high,
      smoothing: 0.12,
      curve: 1,
      invert: false,
    }])
  }

  return <div className="space-y-3">
    <div className="space-y-1">
      <Label>{propertyLabel(property)}</Label>
      {propertyDescription(property) && <p className="text-xs leading-relaxed text-muted-foreground">{propertyDescription(property)}</p>}
      <p className="text-[11px] text-muted-foreground">Audio values are normalized to 0–100%, then mapped between the configured low and high values. One binding per setting.</p>
    </div>

    {targets.length === 0 ? (
      <p className="rounded-lg border border-dashed border-border px-3 py-2 text-xs text-muted-foreground">This scene has no modulatable numeric settings.</p>
    ) : bindings.length === 0 ? (
      <p className="rounded-lg border border-dashed border-border px-3 py-3 text-xs text-muted-foreground">No audio bindings yet. The scene keeps its normal configured values.</p>
    ) : (
      <div className="space-y-3">
        {bindings.map((binding, index) => {
          const target = targets.find(candidate => candidate.name === binding.property)
          const minLimit = typeof target?.additional?.min === 'number' ? target.additional.min : undefined
          const maxLimit = typeof target?.additional?.max === 'number' ? target.additional.max : undefined
          return <div key={`${binding.property}-${index}`} className="space-y-3 rounded-xl border border-border/70 bg-background/55 p-3">
            <div className="grid gap-3 sm:grid-cols-2">
              <div className="space-y-1.5">
                <Label className="text-xs">Scene setting</Label>
                <Select value={binding.property} onValueChange={(next) => {
                  const nextTarget = targets.find(candidate => candidate.name === next)
                  if (!nextTarget) return
                  const range = targetRange(nextTarget)
                  update(index, { property: next, min: range.low, max: range.high })
                }}>
                  <SelectTrigger><SelectValue /></SelectTrigger>
                  <SelectContent>
                    {targets.map(candidate => <SelectItem
                      key={candidate.name}
                      value={candidate.name}
                      disabled={candidate.name !== binding.property && usedTargets.has(candidate.name)}
                    >{candidate.additional?.label ?? candidate.name}</SelectItem>)}
                  </SelectContent>
                </Select>
              </div>
              <div className="space-y-1.5">
                <Label className="text-xs">Audio feature</Label>
                <Select value={binding.feature} onValueChange={(feature) => update(index, { feature })}>
                  <SelectTrigger><SelectValue /></SelectTrigger>
                  <SelectContent>{audioFeatures.map(([feature, label]) => <SelectItem key={feature} value={feature}>{label}</SelectItem>)}</SelectContent>
                </Select>
              </div>
            </div>

            <div className="grid grid-cols-2 gap-3 sm:grid-cols-4">
              <div className="space-y-1.5">
                <Label className="text-xs">Low value</Label>
                <Input type="number" value={binding.min} min={minLimit} max={maxLimit} step="any" onChange={(event) => {
                  const next = Number(event.target.value)
                  if (Number.isFinite(next)) update(index, { min: next })
                }} />
              </div>
              <div className="space-y-1.5">
                <Label className="text-xs">High value</Label>
                <Input type="number" value={binding.max} min={minLimit} max={maxLimit} step="any" onChange={(event) => {
                  const next = Number(event.target.value)
                  if (Number.isFinite(next)) update(index, { max: next })
                }} />
              </div>
              <div className="space-y-1.5">
                <Label className="text-xs">Smoothing (s)</Label>
                <Input type="number" value={binding.smoothing ?? 0.12} min={0} max={3} step={0.01} onChange={(event) => {
                  const next = Number(event.target.value)
                  if (Number.isFinite(next)) update(index, { smoothing: next })
                }} />
              </div>
              <div className="space-y-1.5">
                <Label className="text-xs">Curve</Label>
                <Input type="number" value={binding.curve ?? 1} min={0.15} max={4} step={0.05} onChange={(event) => {
                  const next = Number(event.target.value)
                  if (Number.isFinite(next)) update(index, { curve: next })
                }} />
              </div>
            </div>

            <div className="flex items-center justify-between gap-3">
              <div className="flex items-center gap-2">
                <Switch checked={Boolean(binding.invert)} onCheckedChange={(invert) => update(index, { invert })} />
                <Label className="text-xs">Invert response</Label>
              </div>
              <Button type="button" variant="ghost" size="sm" className="text-destructive hover:text-destructive" onClick={() => onChange(bindings.filter((_, current) => current !== index))}>
                <Trash2 className="mr-1.5 h-3.5 w-3.5" />Remove
              </Button>
            </div>
          </div>
        })}
      </div>
    )}

    <Button type="button" variant="outline" size="sm" className="gap-2" disabled={targets.length === 0 || usedTargets.size >= targets.length} onClick={addBinding}>
      <Plus className="h-4 w-4" />Add audio binding
    </Button>
  </div>
}
