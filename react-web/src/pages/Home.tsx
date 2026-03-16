import { toast } from 'sonner'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import StatusCard from '~/components/home/StatusCard'
import PresetsSection from '~/components/home/PresetsSection'
import ErrorCard from '~/components/home/ErrorCard'
import type { Status } from '~/apiTypes/status'
import type { ListPresets } from '~/apiTypes/list_presets'

export default function Home() {
  const apiUrl = useApiUrl()

  const {
    data: status, isLoading: statusLoading, error: statusError,
    setRetry: retryStatus, setData: setStatus
  } = useFetch<Status>('/status')

  const {
    data: presets, isLoading: presetsLoading, error: presetsError,
    setRetry: retryPresets
  } = useFetch<ListPresets>('/list_presets')

  const handleToggle = async (enabled: boolean) => {
    if (!apiUrl) return
    try {
      await fetch(`${apiUrl}/set_enabled?enabled=${enabled}`)
      setStatus(prev => prev ? { ...prev, turned_off: !enabled } : null)
      toast.success(enabled ? 'Matrix turned on' : 'Matrix turned off')
    } catch {
      toast.error('Failed to toggle matrix')
      retryStatus(r => r + 1)
    }
  }

  const handleActivate = async (id: string, displayName: string) => {
    if (!apiUrl) return
    try {
      await fetch(`${apiUrl}/set_active?id=${encodeURIComponent(id)}`)
      setStatus(prev => prev ? { ...prev, current: id } : null)
      toast.success(`Activated "${displayName}"`)
    } catch {
      toast.error('Failed to activate preset')
    }
  }

  const handleDelete = async (id: string, displayName: string) => {
    if (!apiUrl) return
    try {
      await fetch(`${apiUrl}/preset?id=${encodeURIComponent(id)}`, { method: 'DELETE' })
      toast.success(`Deleted "${displayName}"`)
      retryPresets(r => r + 1)
    } catch {
      toast.error('Failed to delete preset')
    }
  }

  const handleRename = async (id: string, displayName: string) => {
    if (!apiUrl) return
    try {
      const res = await fetch(`${apiUrl}/preset_display_name?id=${encodeURIComponent(id)}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ display_name: displayName }),
      })
      if (!res.ok) throw new Error('Failed to rename preset')
      toast.success(`Renamed to "${displayName}"`)
      retryPresets(r => r + 1)
    } catch {
      toast.error('Failed to rename preset')
    }
  }

  const activePresetLabel = status?.current && presets?.[status.current]
    ? (presets[status.current].display_name ?? status.current)
    : (status?.current ?? null)

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold">Dashboard</h1>
        <p className="text-muted-foreground text-sm mt-1">Manage your LED matrix</p>
      </div>

      {statusError ? (
        <ErrorCard error={statusError} onRetry={() => retryStatus(r => r + 1)} />
      ) : (
        <StatusCard
          status={status}
          currentPresetLabel={activePresetLabel}
          isLoading={statusLoading}
          onToggle={handleToggle}
        />
      )}

      {presetsError ? (
        <ErrorCard error={presetsError} onRetry={() => retryPresets(r => r + 1)} />
      ) : (
        <PresetsSection
          presets={presets}
          isLoading={presetsLoading}
          activePresetId={status?.current ?? null}
          onActivate={handleActivate}
          onDelete={handleDelete}
          onRename={handleRename}
          onCreated={() => retryPresets(r => r + 1)}
        />
      )}
    </div>
  )
}
