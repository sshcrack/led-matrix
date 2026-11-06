import { useState } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { ArrowLeft, Save, Loader2 } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '~/components/ui/button'
import { Skeleton } from '~/components/ui/skeleton'
import useFetch from '~/useFetch'
import { useDraftSave } from '~/lib/draftSave'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import ImageProviders from '~/components/modify-providers/ImageProviders'
import AddProviderButton from '~/components/modify-providers/AddProviderButton'
import type { RawPreset } from '~/apiTypes/list_presets'
import { arrayToObjectPresets, objectToArrayPresets } from '~/apiTypes/list_presets'
import type { ListProviders, ProviderValue } from '~/apiTypes/list_scenes'

export default function ModifyProviders() {
  const { preset_id, scene_id } = useParams<{ preset_id: string; scene_id: string }>()
  const presetId = preset_id ? decodeURIComponent(preset_id) : ''
  const sceneId = scene_id ?? ''
  const navigate = useNavigate()
  const apiUrl = useApiUrl()

  const [providers, setProviders] = useState<ProviderValue[]>([])
  const { isDirty, isSaving, edited, loaded, save } = useDraftSave()
  const [preset, setPreset] = useState<ReturnType<typeof arrayToObjectPresets> | null>(null)

  const { data: rawPreset, isLoading } = useFetch<RawPreset>(`/presets?id=${encodeURIComponent(presetId)}`)
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
      // Find provider properties
      const providerArgs: ProviderValue[] = []
      for (const [, val] of Object.entries(scene.arguments)) {
        if (val && typeof val === 'object' && 'type' in val && 'uuid' in val) {
          const ptype = (val as ProviderValue).type
          if (ptype === 'collection' || ptype === 'pages') {
            providerArgs.push(val as ProviderValue)
          }
        }
      }
      setProviders(providerArgs)
    }
  }

  const handleSave = async () => {
    if (!apiUrl || !preset) return
    try {
      const saved = await save(async () => {
        // Update the scene's provider arguments
        const scene = preset.scenes[sceneId]
        if (!scene) throw new Error('Scene not found')

        // Map providers back into scene arguments
        const updatedArgs = { ...scene.arguments }
        let providerIdx = 0
        for (const [key, val] of Object.entries(scene.arguments)) {
          if (val && typeof val === 'object' && 'type' in val && 'uuid' in val) {
            const ptype = (val as ProviderValue).type
            if (ptype === 'collection' || ptype === 'pages') {
              updatedArgs[key] = providers[providerIdx] ?? val
              providerIdx++
            }
          }
        }

        const updatedPreset = {
          ...preset,
          scenes: {
            ...preset.scenes,
            [sceneId]: { ...scene, arguments: updatedArgs },
          },
        }
        const rawToSave = objectToArrayPresets(updatedPreset)
        const res = await fetch(`${apiUrl}/preset?id=${encodeURIComponent(presetId)}`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(rawToSave),
        })
        if (!res.ok) throw new Error('Save failed')
      })
      if (saved) toast.success('Providers saved')
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
            <h1 className="font-semibold text-sm">Scene sources</h1>
            <p className="text-xs text-muted-foreground">Choose and combine the content inputs for this scene</p>
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

      <div className="max-w-4xl mx-auto p-4 sm:p-6 space-y-5">
        {isLoading ? (
          <div className="space-y-3">
            <Skeleton className="h-40 w-full" />
          </div>
        ) : (
          <>
            <div className="rounded-2xl border border-border bg-card/80 p-4 sm:p-5"><div className="mb-4"><h2 className="font-semibold">Active sources</h2><p className="text-sm text-muted-foreground">Sources are used in order. Add several to create a richer rotation.</p></div><ImageProviders
              providers={providers}
              onChange={handleProvidersChange}
            /></div>
            <AddProviderButton
              providerDefinitions={providerDefs ?? []}
              providerType="image"
              onAdd={(p) => handleProvidersChange([...providers, p])}
            />
          </>
        )}
      </div>
    </div>
  )
}
