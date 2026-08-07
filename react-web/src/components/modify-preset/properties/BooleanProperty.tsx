import { Switch } from '~/components/ui/switch'
import { Label } from '~/components/ui/label'
import { propertyDescription, propertyLabel } from '../propertyUi'
import type { Property } from '~/apiTypes/list_scenes'

interface BooleanPropertyProps {
  property: Property<boolean>
  value: boolean
  onChange: (value: boolean) => void
}

export default function BooleanProperty({ property, value, onChange }: BooleanPropertyProps) {
  return (
    <div className="flex items-start justify-between gap-4 py-1">
      <div className="space-y-1">
        <Label className="cursor-pointer">{propertyLabel(property)}</Label>
        {propertyDescription(property) && <p className="text-xs leading-relaxed text-muted-foreground">{propertyDescription(property)}</p>}
      </div>
      <Switch checked={!!value} onCheckedChange={onChange} />
    </div>
  )
}
