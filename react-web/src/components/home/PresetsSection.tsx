import { Skeleton } from '~/components/ui/skeleton'
import PresetCard from './Preset'
import AddPresetButton from './AddPresetButton'
import type { ListPresets } from '~/apiTypes/list_presets'

interface PresetsSectionProps {
  presets: ListPresets | null
  isLoading: boolean
  activePreset: string | null
  onActivate: (name: string) => void
  onDelete: (name: string) => void
  onCreated: () => void
}

export default function PresetsSection({
  presets, isLoading, activePreset, onActivate, onDelete, onCreated
}: PresetsSectionProps) {
  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between">
        <h2 className="font-semibold">Presets</h2>
        <span className="text-sm text-muted-foreground">
          {presets ? Object.keys(presets).length : 0} total
        </span>
      </div>

      {isLoading ? (
        <div className="space-y-2">
          {[1, 2, 3].map((i) => (
            <Skeleton key={i} className="h-20 w-full rounded-xl" />
          ))}
        </div>
      ) : presets && Object.keys(presets).length > 0 ? (
        <div className="space-y-2">
          {Object.entries(presets).map(([name, preset]) => (
            <PresetCard
              key={name}
              name={name}
              preset={preset}
              isActive={name === activePreset}
              onActivate={onActivate}
              onDelete={onDelete}
            />
          ))}
        </div>
      ) : (
        <div className="text-center py-8 text-muted-foreground text-sm">
          No presets yet. Create your first one!
        </div>
      )}

      <AddPresetButton onCreated={onCreated} />
    </div>
  )
}
