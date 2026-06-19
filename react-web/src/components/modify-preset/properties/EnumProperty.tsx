import { useEffect, useRef } from 'react'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import { Label } from '~/components/ui/label'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface EnumValueItem {
  value: string
  display_name?: string
}

function isRecord(v: unknown): v is Record<string, unknown> {
  return typeof v === 'object' && v !== null && !Array.isArray(v)
}

interface EnumPropertyProps {
  property: Property<string>
  value: string
  onChange: (value: string) => void
}

export default function EnumProperty({ property, value, onChange }: EnumPropertyProps) {
  let options: string[] = [];
  let displayNames: Record<string, string> = {};
  const additional = property.additional;
  if (isRecord(additional)) {
    const enumValues = 'enum_values' in additional ? additional.enum_values : undefined;
    const values = 'values' in additional ? additional.values : undefined;
    if (Array.isArray(enumValues)) {
      options = enumValues.map(item => (isRecord(item) ? String(item.value ?? '') : ''));
      for (const item of enumValues) {
        if (isRecord(item)) {
          const v = String(item.value ?? '');
          displayNames[v] = typeof item.display_name === 'string' ? item.display_name : v;
        }
      }
    } else if (Array.isArray(values)) {
      options = values.map(v => String(v));
    }
  }
  const normalizedValue = options.length > 0
    ? (options.includes(value) ? value : options[0])
    : value

  const onChangeRef = useRef(onChange);
  onChangeRef.current = onChange;

  useEffect(() => {
    if (options.length > 0 && normalizedValue && value !== normalizedValue) {
      onChangeRef.current(normalizedValue)
    }
  }, [normalizedValue, value, options.length])

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
