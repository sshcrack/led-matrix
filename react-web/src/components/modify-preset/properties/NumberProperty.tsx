import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Button } from '~/components/ui/button'
import { titleCase } from '~/lib/utils'
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

export default function NumberProperty({ property, value, onChange }: NumberPropertyProps) {
  const numericValue = Number.isFinite(value) ? value : 0

  if (property.type_id === 'millis') {
    const presets = [250, 500, 750, 1000, 2000, 5000]

    return (
      <div className="space-y-2">
        <Label>{titleCase(property.name)}</Label>
        <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
          <div className="space-y-1">
            <Label className="text-xs text-muted-foreground">Seconds</Label>
            <Input
              type="number"
              min={0}
              step={0.1}
              value={(numericValue / 1000).toFixed(2)}
              onChange={(e) => {
                const seconds = Number(e.target.value)
                onChange(Number.isFinite(seconds) ? Math.max(0, Math.round(seconds * 1000)) : 0)
              }}
            />
          </div>
          <div className="space-y-1">
            <Label className="text-xs text-muted-foreground">Milliseconds</Label>
            <Input
              type="number"
              min={0}
              step={1}
              value={numericValue}
              onChange={(e) => onChange(Math.max(0, Number(e.target.value) || 0))}
            />
          </div>
        </div>
        <div className="flex flex-wrap gap-2">
          {presets.map((preset) => (
            <Button
              key={preset}
              type="button"
              variant="outline"
              size="sm"
              onClick={() => onChange(preset)}
            >
              {preset >= 1000 ? `${preset / 1000}s` : `${preset}ms`}
            </Button>
          ))}
        </div>
      </div>
    )
  }

  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <Input
        type="number"
        value={numericValue}
        step={getStep(property.type_id)}
        min={getMin(property.type_id)}
        max={getMax(property.type_id)}
        onChange={(e) => onChange(Number(e.target.value))}
      />
    </div>
  )
}
