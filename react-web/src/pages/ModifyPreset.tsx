import { useEffect } from 'react'
import { useParams, useNavigate } from 'react-router-dom'
import { ArrowLeft } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Skeleton } from '~/components/ui/skeleton'
import useFetch from '~/useFetch'
import { ConfigProvider, useConfig } from '~/components/configShare/ConfigProvider'
import { SaveButton } from '~/components/configShare/SaveButton'
import { SceneContext } from '~/components/modify-preset/SceneContext'
import Scene from '~/components/modify-preset/Scene'
import AddScene from '~/components/modify-preset/AddScene'
import TransitionPicker from '~/components/modify-preset/TransitionPicker'
import PresetTransitionDuration from '~/components/modify-preset/PresetTransitionDuration'
import ResetApiUrl from '~/components/modify-preset/ResetApiUrl'
import type { RawPreset, Scene as SceneType } from '~/apiTypes/list_presets'
import { arrayToObjectPresets } from '~/apiTypes/list_presets'
import type { ListScenes, ListProviders } from '~/apiTypes/list_scenes'

function ModifyPresetInner({ presetId }: { presetId: string }) {
  const navigate = useNavigate()
  const { preset, setPreset } = useConfig()

  const { data: rawPreset, isLoading: presetLoading } = useFetch<RawPreset>(`/presets?id=${encodeURIComponent(presetId)}`)
  const { data: sceneDefinitions, isLoading: scenesLoading } = useFetch<ListScenes[]>('/list_scenes')
  const { data: transitions } = useFetch<string[]>('/list_transitions')
  const { data: providers } = useFetch<ListProviders[]>('/list_providers')

  useEffect(() => {
    if (rawPreset) {
      setPreset(arrayToObjectPresets(rawPreset))
    }
  }, [rawPreset])

  const isLoading = presetLoading || scenesLoading

  const handleUpdateScene = (uuid: string, scene: SceneType) => {
    setPreset(prev => prev ? { ...prev, scenes: { ...prev.scenes, [uuid]: scene } } : prev)
  }

  const handleDeleteScene = (uuid: string) => {
    setPreset(prev => {
      if (!prev) return prev
      const scenes = { ...prev.scenes }
      delete scenes[uuid]
      return { ...prev, scenes }
    })
  }

  const handleAddScene = (scene: SceneType) => {
    setPreset(prev => prev ? { ...prev, scenes: { ...prev.scenes, [scene.uuid]: scene } } : prev)
  }

  if (isLoading) {
    return (
      <div className="space-y-4 p-4">
        <Skeleton className="h-10 w-full" />
        <Skeleton className="h-24 w-full" />
        <Skeleton className="h-24 w-full" />
      </div>
    )
  }

  return (
    <SceneContext.Provider value={{ updateScene: handleUpdateScene, deleteScene: handleDeleteScene }}>
      <div className="min-h-screen bg-background">
        {/* Header */}
        <header className="sticky top-0 z-10 bg-card border-b border-border px-4 py-3 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <Button variant="ghost" size="icon" onClick={() => navigate('/')}>
              <ArrowLeft className="h-5 w-5" />
            </Button>
            <div>
              <h1 className="font-semibold text-sm">{rawPreset?.display_name ?? presetId}</h1>
              <p className="text-xs text-muted-foreground">Edit preset</p>
            </div>
          </div>
          <SaveButton />
        </header>

        <div className="max-w-2xl mx-auto p-4 space-y-6">
          {/* Transition settings */}
          {preset && (
            <div className="space-y-3 p-4 rounded-xl border border-border bg-card">
              <h2 className="font-medium text-sm">Transition</h2>
              {transitions && (
                <TransitionPicker
                  value={preset.transition_name}
                  transitions={transitions}
                  onChange={(v) => setPreset(p => p ? { ...p, transition_name: v } : p)}
                />
              )}
              <PresetTransitionDuration
                value={preset.transition_duration}
                onChange={(v) => setPreset(p => p ? { ...p, transition_duration: v } : p)}
              />
            </div>
          )}

          {/* Scenes */}
          <div className="space-y-3">
            <h2 className="font-medium text-sm">Scenes ({Object.keys(preset?.scenes ?? {}).length})</h2>
            {preset && Object.values(preset.scenes).map(scene => (
              <Scene
                key={scene.uuid}
                scene={scene}
                sceneDefinitions={sceneDefinitions ?? []}
                providers={providers ?? []}
                presetId={presetId}
              />
            ))}
            <AddScene
              sceneDefinitions={sceneDefinitions ?? []}
              onAdd={handleAddScene}
            />
          </div>

          <div className="flex justify-center pb-4">
            <ResetApiUrl />
          </div>
        </div>
      </div>
    </SceneContext.Provider>
  )
}

export default function ModifyPreset() {
  const { preset_id } = useParams<{ preset_id: string }>()
  const presetId = preset_id ? decodeURIComponent(preset_id) : ''

  return (
    <ConfigProvider presetId={presetId}>
      <ModifyPresetInner presetId={presetId} />
    </ConfigProvider>
  )
}
