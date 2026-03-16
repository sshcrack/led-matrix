import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface GeneralPropertyProps {
  property: Property<any>
  value: any
  onChange: (value: any) => void
}

export default function GeneralProperty({ property, value, onChange }: GeneralPropertyProps) {
  const displayValue = typeof value === 'object' ? JSON.stringify(value) : String(value ?? '')

  return (
    <div className="space-y-1.5">
      <Label>
        {titleCase(property.name)}
        <span className="ml-1.5 text-xs text-muted-foreground">({property.type_id})</span>
      </Label>
      <Input
        value={displayValue}
        onChange={(e) => {
          try {
            onChange(JSON.parse(e.target.value))
          } catch {
            onChange(e.target.value)
          }
        }}
      />
    </div>
  )
}
