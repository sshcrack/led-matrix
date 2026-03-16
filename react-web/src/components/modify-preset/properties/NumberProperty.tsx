import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
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
  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <Input
        type="number"
        value={value ?? 0}
        step={getStep(property.type_id)}
        min={getMin(property.type_id)}
        max={getMax(property.type_id)}
        onChange={(e) => onChange(Number(e.target.value))}
      />
    </div>
  )
}
