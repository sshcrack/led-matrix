import { useEffect, useRef, useState } from 'react'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import DurationInput from '~/components/ui/duration-input'
import { propertyDescription, propertyLabel } from '../propertyUi'
import type { Property, TypeId } from '~/apiTypes/list_scenes'

interface NumberPropertyProps {
  property: Property<number>
  value: number
  onChange: (value: number) => void
}

function getStep(typeId: TypeId): number {
  switch (typeId) {
    case 'int':
    case 'int16_t':
    case 'uint8_t':
    case 'millis':
      return 1
    default:
      return 0.01
  }
}

function getMin(typeId: TypeId): number | undefined {
  if (typeId === 'uint8_t') return 0
  return undefined
}

function getMax(typeId: TypeId): number | undefined {
  if (typeId === 'uint8_t') return 255
  if (typeId === 'int16_t') return 32767
  return undefined
}

function isIntegerType(typeId: TypeId) {
  return typeId === 'int' || typeId === 'int16_t' || typeId === 'uint8_t'
}

export default function NumberProperty({ property, value, onChange }: NumberPropertyProps) {
  const numericValue = Number.isFinite(value) ? value : 0

  if (property.type_id === 'millis') {
    const presets = property.name === 'transition_duration'
      ? [150, 250, 500, 750, 1000, 2000]
      : [1000, 5000, 10_000, 30_000, 60_000]

    return (
      <DurationInput
        id={`property-${property.name}`}
        label={propertyLabel(property)}
        value={numericValue}
        onChange={onChange}
        presets={(property.additional?.presets as number[] | undefined) ?? presets}
        description={propertyDescription(property)}
      />
    )
  }

  return (
    <DraftNumberInput
      property={property}
      value={numericValue}
      onChange={onChange}
    />
  )
}

function DraftNumberInput({ property, value, onChange }: NumberPropertyProps) {
  const [draft, setDraft] = useState(String(value))
  const [error, setError] = useState<string | null>(null)
  const focused = useRef(false)
  const min = typeof property.additional?.min === 'number' ? property.additional.min : getMin(property.type_id)
  const max = typeof property.additional?.max === 'number' ? property.additional.max : getMax(property.type_id)
  const step = typeof property.additional?.step === 'number' ? property.additional.step : getStep(property.type_id)
  const description = propertyDescription(property)
  const unit = property.additional?.unit

  useEffect(() => {
    if (!focused.current) setDraft(String(value))
  }, [value])

  const commit = () => {
    if (draft.trim() === '') {
      setError('Enter a number.')
      return false
    }

    let next = Number(draft.replace(',', '.'))
    if (!Number.isFinite(next)) {
      setError('Enter a valid number.')
      return false
    }

    if (isIntegerType(property.type_id)) next = Math.round(next)
    if (min !== undefined) next = Math.max(min, next)
    if (max !== undefined) next = Math.min(max, next)

    onChange(next)
    setDraft(String(next))
    setError(null)
    return true
  }

  return (
    <div className="space-y-1.5">
      <div className="flex items-center justify-between gap-3">
        <Label htmlFor={`property-${property.name}`}>{propertyLabel(property)}</Label>
        {(min !== undefined || max !== undefined) && (
          <span className="text-[11px] text-muted-foreground">
            {min !== undefined ? min : '−∞'} – {max !== undefined ? max : '∞'}
          </span>
        )}
      </div>
      <Input
        id={`property-${property.name}`}
        type="text"
        inputMode={isIntegerType(property.type_id) ? 'numeric' : 'decimal'}
        value={draft}
        aria-invalid={Boolean(error)}
        step={step}
        className={error ? 'border-destructive focus-visible:ring-destructive' : undefined}
        onFocus={() => { focused.current = true }}
        onChange={(event) => {
          setDraft(event.target.value)
          if (error) setError(null)
        }}
        onBlur={() => {
          focused.current = false
          if (!commit()) setDraft(String(value))
        }}
        onKeyDown={(event) => {
          if (event.key === 'Enter') {
            event.preventDefault()
            commit()
            event.currentTarget.blur()
          } else if (event.key === 'Escape') {
            setDraft(String(value))
            setError(null)
            event.currentTarget.blur()
          }
        }}
      />
      {error ? <p className="text-xs text-destructive">{error}</p> : description ? <p className="text-xs text-muted-foreground">{description}</p> : null}
      {unit && <p className="text-[11px] text-muted-foreground">Unit: {unit}</p>}
    </div>
  )
}
