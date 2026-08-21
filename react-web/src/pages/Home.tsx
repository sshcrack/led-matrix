import { toast } from 'sonner'
import { Activity, Layers3, Radio } from 'lucide-react'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import StatusCard from '~/components/home/StatusCard'
import PresetsSection from '~/components/home/PresetsSection'
import ErrorCard from '~/components/home/ErrorCard'
import LiveMatrixPreview from '~/components/scene-browser/LiveMatrixPreview'
import { Badge } from '~/components/ui/badge'
import type { Status } from '~/apiTypes/status'
import type { ListPresets } from '~/apiTypes/list_presets'
import type { ListScenes } from '~/apiTypes/list_scenes'

export default function Home() {
  const apiUrl = useApiUrl()
  const { data: status, isLoading: statusLoading, error: statusError, setRetry: retryStatus, setData: setStatus } = useFetch<Status>('/status')
  const { data: presets, isLoading: presetsLoading, error: presetsError, setRetry: retryPresets } = useFetch<ListPresets>('/list_presets')
  const { data: definitions } = useFetch<ListScenes[]>('/list_scenes')

  const handleToggle = async (enabled: boolean) => {
    if (!apiUrl) return
    try { const res = await fetch(`${apiUrl}/set_enabled?enabled=${enabled}`); if (!res.ok) throw new Error(); setStatus(prev => prev ? { ...prev, turned_off: !enabled } : null); toast.success(enabled ? 'Matrix turned on' : 'Matrix turned off') }
    catch { toast.error('Failed to toggle matrix'); retryStatus(r => r + 1) }
  }
  const handleActivate = async (id: string, name: string) => {
    if (!apiUrl) return
    try { const res = await fetch(`${apiUrl}/set_active?id=${encodeURIComponent(id)}`); if (!res.ok) throw new Error(); setStatus(prev => prev ? { ...prev, current: id } : null); toast.success(`Activated “${name}”`) } catch { toast.error('Failed to activate preset') }
  }
  const handleDelete = async (id: string, name: string) => { if (!apiUrl) return; try { const res = await fetch(`${apiUrl}/preset?id=${encodeURIComponent(id)}`, { method: 'DELETE' }); if (!res.ok) throw new Error(); toast.success(`Deleted “${name}”`); retryPresets(r => r + 1) } catch { toast.error('Failed to delete preset') } }
  const handleRename = async (id: string, name: string) => { if (!apiUrl) return; try { const res = await fetch(`${apiUrl}/preset_display_name?id=${encodeURIComponent(id)}`, { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({display_name:name}) }); if (!res.ok) throw new Error(); toast.success(`Renamed to “${name}”`); retryPresets(r=>r+1) } catch { toast.error('Failed to rename preset') } }

  const activePreset = status?.current ? presets?.[status.current] : null
  const activeLabel = activePreset?.display_name ?? status?.current ?? null
  const activeScene = activePreset?.scenes?.[0]
  const activeDefinition = definitions?.find(d => d.name === activeScene?.type)

  return <div className="space-y-7 pb-24 lg:pb-6">
    <div><Badge variant="outline" className="mb-2 gap-1.5"><Radio className="h-3 w-3" />Live control</Badge><h1 className="text-3xl font-bold tracking-tight">Your matrix at a glance</h1><p className="mt-1 text-sm text-muted-foreground">Power, preview, and presets without digging through menus.</p></div>

    <div className="grid gap-5 xl:grid-cols-[minmax(0,1fr)_420px]">
      <div className="space-y-5">
        {statusError ? <ErrorCard error={statusError} onRetry={() => retryStatus(r => r + 1)} /> : <StatusCard status={status} currentPresetLabel={activeLabel} isLoading={statusLoading} onToggle={handleToggle} />}
        <div className="grid grid-cols-2 gap-3">
          <div className="glass-panel rounded-2xl p-4"><Layers3 className="mb-3 h-5 w-5 text-primary" /><div className="text-2xl font-bold">{activePreset?.scenes.length ?? 0}</div><div className="text-xs text-muted-foreground">Scenes in active preset</div></div>
          <div className="glass-panel rounded-2xl p-4"><Activity className="mb-3 h-5 w-5 text-sky-500" /><div className="text-2xl font-bold">{presets ? Object.keys(presets).length : 0}</div><div className="text-xs text-muted-foreground">Available presets</div></div>
        </div>
      </div>
      <LiveMatrixPreview apiUrl={apiUrl ?? ''} fallbackSceneName={activeScene?.type} fallbackHasPreview={activeDefinition?.has_preview} label={activeLabel ? `Now playing · ${activeLabel}` : 'Live matrix'} />
    </div>

    <section className="glass-panel rounded-2xl p-4 sm:p-5">{presetsError ? <ErrorCard error={presetsError} onRetry={() => retryPresets(r => r + 1)} /> : <PresetsSection presets={presets} isLoading={presetsLoading} activePresetId={status?.current ?? null} onActivate={handleActivate} onDelete={handleDelete} onRename={handleRename} onCreated={() => retryPresets(r => r + 1)} />}</section>
  </div>
}
