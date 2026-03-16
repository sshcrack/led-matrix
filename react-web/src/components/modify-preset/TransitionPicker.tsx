import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import { Label } from '~/components/ui/label'

interface TransitionPickerProps {
  value: string
  transitions: string[]
  onChange: (value: string) => void
}

export default function TransitionPicker({ value, transitions, onChange }: TransitionPickerProps) {
  return (
    <div className="space-y-1.5">
      <Label>Transition</Label>
      <Select value={value} onValueChange={onChange}>
        <SelectTrigger>
          <SelectValue placeholder="Select transition..." />
        </SelectTrigger>
        <SelectContent>
          {transitions.map(t => (
            <SelectItem key={t} value={t}>{t}</SelectItem>
          ))}
        </SelectContent>
      </Select>
    </div>
  )
}
