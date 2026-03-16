import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'

interface PresetTransitionDurationProps {
  value: number
  onChange: (value: number) => void
}

export default function PresetTransitionDuration({ value, onChange }: PresetTransitionDurationProps) {
  return (
    <div className="space-y-1.5">
      <Label>Transition Duration (ms)</Label>
      <Input
        type="number"
        min={0}
        max={10000}
        value={value}
        onChange={(e) => onChange(Number(e.target.value))}
      />
    </div>
  )
}
