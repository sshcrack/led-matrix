import DurationInput from '~/components/ui/duration-input'

interface PresetTransitionDurationProps {
  value: number
  onChange: (value: number) => void
}

export default function PresetTransitionDuration({ value, onChange }: PresetTransitionDurationProps) {
  return (
    <DurationInput
      id="preset-transition-duration"
      label="Transition duration"
      value={value}
      min={0}
      max={10_000}
      presets={[150, 250, 500, 750, 1000, 2000]}
      description="How long the blend between scenes lasts. Type 0s to switch immediately."
      onChange={onChange}
    />
  )
}
