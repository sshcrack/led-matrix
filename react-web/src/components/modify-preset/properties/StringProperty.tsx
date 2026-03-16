import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface StringPropertyProps {
  property: Property<string>
  value: string
  onChange: (value: string) => void
}

export default function StringProperty({ property, value, onChange }: StringPropertyProps) {
  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <Input
        value={value ?? ''}
        onChange={(e) => onChange(e.target.value)}
        placeholder={`Enter ${property.name}...`}
      />
    </div>
  )
}
