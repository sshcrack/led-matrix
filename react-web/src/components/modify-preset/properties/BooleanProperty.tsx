import { Switch } from '~/components/ui/switch'
import { Label } from '~/components/ui/label'
import { titleCase } from '~/lib/utils'
import type { Property } from '~/apiTypes/list_scenes'

interface BooleanPropertyProps {
  property: Property<boolean>
  value: boolean
  onChange: (value: boolean) => void
}

export default function BooleanProperty({ property, value, onChange }: BooleanPropertyProps) {
  return (
    <div className="flex items-center justify-between py-1">
      <Label className="cursor-pointer">{titleCase(property.name)}</Label>
      <Switch checked={!!value} onCheckedChange={onChange} />
    </div>
  )
}
