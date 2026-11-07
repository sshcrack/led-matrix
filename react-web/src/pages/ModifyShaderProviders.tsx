import { useState } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { ArrowLeft, Save, Loader2 } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '~/components/ui/button'
import { Skeleton } from '~/components/ui/skeleton'
import useFetch from '~/useFetch'
import { useDraftSave } from '~/lib/draftSave'
import ShaderUrlProviders from '~/components/modify-providers/ShaderUrlProviders'
import AddProviderButton from '~/components/modify-providers/AddProviderButton'
import type { RawPreset } from '~/apiTypes/list_presets'
import { arrayToObjectPresets, objectToArrayPresets } from '~/apiTypes/list_presets'
import { useMatrix } from '~/core/matrixContext'
import { useLoadedPreset } from '~/core/useLoadedPreset'
import type { ListProviders, ProviderValue } from '~/apiTypes/list_scenes'

export default function ModifyShaderProviders() {
  const { preset_id, scene_id } = useParams<{ preset_id: string; scene_id: string }>()
  const presetId = preset_id ? decodeURIComponent(preset_id) : ''
  const sceneId = scene_id ?? ''
  const navigate = useNavigate()
  const { persistence } = useMatrix()

  const [providers, setProviders] = useState<ProviderValue[]>([])
  const { isDirty, isSaving, edited, loaded, save } = useDraftSave()
  const [preset, setPreset] = useState<ReturnType<typeof arrayToObjectPresets> | null>(null)

  const { data: rawPreset, isLoading } = useLoadedPreset(presetId)
  const { data: providerDefs } = useFetch<ListProviders[]>('/list_providers')

  // Re-derive local editing state whenever the fetched preset or scene changes
  // (adjusting state during render rather than in an effect).
  const [syncedFrom, setSyncedFrom] = useState<{ raw: RawPreset | null; sceneId: string }>({ raw: null, sceneId })
  if (rawPreset && (syncedFrom.raw !== rawPreset || syncedFrom.sceneId !== sceneId)) {
    setSyncedFrom({ raw: rawPreset, sceneId })
    const p = arrayToObjectPresets(rawPreset)
    setPreset(p)
    loaded()
    const scene = p.scenes[sceneId]
    if (scene) {
      const providerArgs: ProviderValue[] = []
      for (const [, val] of Object.entries(scene.arguments)) {
        if (val && typeof val === 'object' && 'type' in val && 'uuid' in val) {
          const ptype = (val as ProviderValue).type
          if (ptype === 'random' || ptype === 'shader_collection') {
            providerArgs.push(val as ProviderValue)
          }
        }
      }
      setProviders(providerArgs)
    }
  }

  const handleSave = async () => {
    if (!preset) return
    try {
      const saved = await save(async () => {
        const scene = preset.scenes[sceneId]
        if (!scene) throw new Error('Scene not found')
        const updatedArgs = { ...scene.arguments }
        let providerIdx = 0
        for (const [key, val] of Object.entries(scene.arguments)) {
          if (val && typeof val === 'object' && 'type' in val && 'uuid' in val) {
            const ptype = (val as ProviderValue).type
            if (ptype === 'random' || ptype === 'shader_collection') {
              updatedArgs[key] = providers[providerIdx] ?? val
              providerIdx++
            }
          }
        }
        const updatedPreset = {
          ...preset,
          scenes: { ...preset.scenes, [sceneId]: { ...scene, arguments: updatedArgs } },
        }
        const rawToSave = objectToArrayPresets(updatedPreset)
        // Ordered with the inspector's autosaves by the shared preset persistence.
        await persistence.save(presetId, rawToSave)
      })
      if (saved) toast.success('Shader providers saved')
    } catch {
      toast.error('Failed to save providers')
    }
  }

  const handleProvidersChange = (newProviders: ProviderValue[]) => {
    setProviders(newProviders)
    edited()
  }

  return (
    <div className="min-h-screen bg-background">
      <header className="sticky top-0 z-10 bg-card border-b border-border px-4 py-3 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <Button variant="ghost" size="icon" onClick={() => navigate(-1)}>
            <ArrowLeft className="h-5 w-5" />
          </Button>
          <div>
            <h1 className="font-semibold text-sm">Shader Providers</h1>
            <p className="text-xs text-muted-foreground">{presetId}</p>
          </div>
        </div>
        <Button
          size="sm"
          onClick={handleSave}
          disabled={!isDirty || isSaving}
          className="gap-2"
        >
          {isSaving ? <Loader2 className="h-4 w-4 animate-spin" /> : <Save className="h-4 w-4" />}
          Save
        </Button>
      </header>

      <div className="max-w-2xl mx-auto p-4 space-y-4">
        {isLoading ? (
          <Skeleton className="h-40 w-full" />
        ) : (
          <>
            <ShaderUrlProviders
              providers={providers}
              onChange={handleProvidersChange}
            />
            <AddProviderButton
              providerDefinitions={providerDefs ?? []}
              providerType="shader"
              onAdd={(p) => handleProvidersChange([...providers, p])}
            />
          </>
        )}
      </div>
    </div>
  )
}
