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
  let options: string[] = [];
  let displayNames: Record<string, string> = {};
  const additional = property.additional as Record<string, unknown> | undefined;
  const enumValues = additional?.enum_values as Array<{ value: string; display_name?: string }> | undefined;
  const values = additional?.values as string[] | undefined;
  if (enumValues) {
    options = enumValues.map(item => item.value);
    enumValues.forEach(item => {
      displayNames[item.value] = item.display_name ?? item.value;
    });
  } else if (values) {
    options = values;
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
