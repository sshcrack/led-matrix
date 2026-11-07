import { useState } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { ArrowLeft, Save, Loader2 } from 'lucide-react'
import { toast } from 'sonner'
import { Button } from '~/components/ui/button'
import { Skeleton } from '~/components/ui/skeleton'
import useFetch from '~/useFetch'
import { useDraftSave } from '~/lib/draftSave'
import ImageProviders from '~/components/modify-providers/ImageProviders'
import ShaderUrlProviders from '~/components/modify-providers/ShaderUrlProviders'
import AddProviderButton from '~/components/modify-providers/AddProviderButton'
import type { RawPreset } from '~/apiTypes/list_presets'
import type { ListProviders, ProviderValue } from '~/apiTypes/list_scenes'
import { useMatrix } from '~/core/matrixContext'
import { useLoadedPreset } from '~/core/useLoadedPreset'
import { providersRoute, readSources, sourceOptions, sourcesProperty, writeSources, type SourceKind } from '~/core/sceneSources'

const titles: Record<SourceKind, string> = { image: 'Scene sources', shader: 'Shader sources' }

/** Edits the sources property (`providers` / `shader_providers`) of one scene in a preset. */
export default function ModifySources({ kind }: { kind: SourceKind }) {
  const { preset_id, scene_id } = useParams<{ preset_id: string; scene_id: string }>()
  const presetId = preset_id ? decodeURIComponent(preset_id) : ''
  const sceneId = scene_id ?? ''
  const navigate = useNavigate()
  const { persistence, sceneByName } = useMatrix()

  const [sources, setSources] = useState<ProviderValue[]>([])
  const { isDirty, isSaving, edited, loaded, save } = useDraftSave()
  const { data: rawPreset, isLoading } = useLoadedPreset(presetId)
  const { data: providerDefs } = useFetch<ListProviders[]>(providersRoute(kind))

  const scene = rawPreset?.scenes.find(s => s.uuid === sceneId)
  const target = sourcesProperty(scene ? sceneByName.get(scene.type) : undefined)
  const property = target?.kind === kind ? target.property : null

  // Re-derive the editing state whenever the loaded preset, scene or definition changes.
  const [syncedFrom, setSyncedFrom] = useState<{ raw: RawPreset | null; sceneId: string; property: string | null }>({ raw: null, sceneId, property: null })
  if (rawPreset && property && (syncedFrom.raw !== rawPreset || syncedFrom.sceneId !== sceneId || syncedFrom.property !== property.name)) {
    setSyncedFrom({ raw: rawPreset, sceneId, property: property.name })
    setSources(readSources(scene?.arguments[property.name] ?? property.default_value))
    loaded()
  }

  const handleSave = async () => {
    if (!property) return
    try {
      const saved = await save(async () => {
        // Read-modify-write: only this scene's sources change, whatever else was edited meanwhile.
        await persistence.update(presetId, preset => {
          if (!preset.scenes.some(s => s.uuid === sceneId)) throw new Error('Scene not found')
          return {
            ...preset,
            scenes: preset.scenes.map(s => s.uuid === sceneId
              ? { ...s, arguments: { ...s.arguments, [property.name]: writeSources(sources) } }
              : s),
          }
        })
      })
      if (saved) toast.success('Sources saved')
    } catch {
      toast.error('Failed to save sources')
    }
  }

  const change = (next: ProviderValue[]) => { setSources(next); edited() }
  const List = kind === 'shader' ? ShaderUrlProviders : ImageProviders
  const missing = !isLoading && rawPreset && sceneByName.size > 0 && !property

  return (
    <div className="min-h-screen bg-background">
      <header className="sticky top-0 z-10 bg-card border-b border-border px-4 py-3 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <Button variant="ghost" size="icon" aria-label="Back" onClick={() => navigate(-1)}>
            <ArrowLeft className="h-5 w-5" />
          </Button>
          <div>
            <h1 className="font-semibold text-sm">{titles[kind]}</h1>
            <p className="text-xs text-muted-foreground">Choose and combine the content inputs for this scene</p>
          </div>
        </div>
        <Button size="sm" onClick={handleSave} disabled={!isDirty || isSaving || !property} className="gap-2">
          {isSaving ? <Loader2 className="h-4 w-4 animate-spin" /> : <Save className="h-4 w-4" />}
          Save
        </Button>
      </header>

      <div className="max-w-4xl mx-auto p-4 sm:p-6 space-y-5">
        {missing ? (
          <p className="text-sm text-muted-foreground">This scene has no {kind} sources to configure.</p>
        ) : isLoading || !property ? (
          <Skeleton className="h-40 w-full" />
        ) : (
          <>
            <div className="rounded-2xl border border-border bg-card/80 p-4 sm:p-5">
              <div className="mb-4">
                <h2 className="font-semibold">Active sources</h2>
                <p className="text-sm text-muted-foreground">Sources are used in order. Add several to create a richer rotation.</p>
              </div>
              <List providers={sources} onChange={change} />
            </div>
            <AddProviderButton options={sourceOptions(kind, providerDefs)} onAdd={p => change([...sources, p])} />
          </>
        )}
      </div>
    </div>
  )
}
