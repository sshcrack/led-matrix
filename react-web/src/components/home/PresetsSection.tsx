import { Skeleton } from '~/components/ui/skeleton'
import PresetCard from './Preset'
import AddPresetButton from './AddPresetButton'
import type { ListPresets } from '~/apiTypes/list_presets'

interface PresetsSectionProps {
  presets: ListPresets | null
  isLoading: boolean
  activePresetId: string | null
  onActivate: (id: string, displayName: string) => void
  onDelete: (id: string, displayName: string) => void
  onRename: (id: string, displayName: string) => void
  onCreated: () => void
}

export default function PresetsSection({
  presets, isLoading, activePresetId, onActivate, onDelete, onRename, onCreated
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
          {Object.entries(presets).map(([id, preset]) => (
            <PresetCard
              key={id}
              id={id}
              displayName={preset.display_name ?? id}
              preset={preset}
              isActive={id === activePresetId}
              onActivate={onActivate}
              onDelete={onDelete}
              onRename={onRename}
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
