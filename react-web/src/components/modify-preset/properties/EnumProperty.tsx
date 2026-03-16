import { useEffect } from 'react'
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
  // Support both 'values' and 'enum_values' keys for enum options
  let options: string[] = [];
  let displayNames: Record<string, string> = {};
  if (property.additional?.enum_values) {
    // enum_values is expected to be an array of objects with 'value' and 'display_name'
    options = property.additional.enum_values.map((item: any) => item.value);
    property.additional.enum_values.forEach((item: any) => {
      displayNames[item.value] = item.display_name ?? item.value;
    });
  } else if (property.additional?.values) {
    options = property.additional.values;
  }
  const normalizedValue = options.includes(value) ? value : (options[0] ?? '')

  useEffect(() => {
    if (normalizedValue && value !== normalizedValue) {
      onChange(normalizedValue)
    }
  }, [normalizedValue, onChange, value])

  return (
    <div className="space-y-1.5">
      <Label>{titleCase(property.name)}</Label>
      <Select value={normalizedValue} onValueChange={onChange}>
        <SelectTrigger>
          <SelectValue placeholder="Select..." />
        </SelectTrigger>
        <SelectContent>
          {options.map(opt => (
            <SelectItem key={opt} value={opt}>{displayNames[opt] ?? opt}</SelectItem>
          ))}
        </SelectContent>
      </Select>
    </div>
  )
}
