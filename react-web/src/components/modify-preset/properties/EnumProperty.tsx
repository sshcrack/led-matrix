import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import { Label } from '~/components/ui/label'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface EnumPropertyProps {
  property: Property<string>
  value: string
  onChange: (value: string) => void
}

export default function EnumProperty({ property, value, onChange }: EnumPropertyProps) {
  const options: string[] = property.additional?.values ?? []

  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <Select value={String(value)} onValueChange={onChange}>
        <SelectTrigger>
          <SelectValue placeholder="Select..." />
        </SelectTrigger>
        <SelectContent>
          {options.map(opt => (
            <SelectItem key={opt} value={opt}>{opt}</SelectItem>
          ))}
        </SelectContent>
      </Select>
    </div>
  )
}
