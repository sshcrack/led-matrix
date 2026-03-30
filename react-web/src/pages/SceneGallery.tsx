import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { Plus } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Skeleton } from '~/components/ui/skeleton'
import { Badge } from '~/components/ui/badge'
import {
  Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription
} from '~/components/ui/dialog'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import useFetch from '~/useFetch'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import type { ListScenes } from '~/apiTypes/list_scenes'
import type { ListPresets, RawPreset, Scene } from '~/apiTypes/list_presets'
import { v4 as uuidv4 } from 'uuid'
import WasmScenePreview from '~/components/WasmScenePreview'

function ScenePreviewCard({ scene, apiUrl }: { scene: ListScenes; apiUrl: string }) {
  return (
    <div className="rounded-xl border border-border bg-card overflow-hidden flex flex-col">
      <div className="relative bg-muted w-full aspect-square flex items-center justify-center">
        <WasmScenePreview
          sceneName={scene.name}
          hasPreview={scene.has_preview}
          apiUrl={apiUrl}
          className="w-full h-full"
        />
      </div>
      <div className="p-3">
        <p className="text-sm font-medium truncate" title={scene.name}>{scene.name}</p>
        <p className="text-xs text-muted-foreground mt-0.5">
          {scene.properties.length} {scene.properties.length === 1 ? 'property' : 'properties'}
        </p>
      </div>
    </div>
  )
}

function AddToPresetDialog({
  scene,
  presets,
  onAdd,
  open,
  onOpenChange,
}: {
  scene: ListScenes | null
  presets: ListPresets | null
  onAdd: (presetId: string, scene: Scene) => void
  open: boolean
  onOpenChange: (open: boolean) => void
}) {
  const [selectedPreset, setSelectedPreset] = useState<string>('')

  const handleAdd = () => {
    if (!scene || !selectedPreset) return
    const args: Record<string, any> = {}
    for (const prop of scene.properties) {
      args[prop.name] = prop.default_value
    }
    const newScene: Scene = {
      uuid: uuidv4(),
      type: scene.name,
      arguments: args,
    }
    onAdd(selectedPreset, newScene)
    setSelectedPreset('')
    onOpenChange(false)
  }

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Add "{scene?.name}" to Preset</DialogTitle>
          <DialogDescription>Choose which preset to add this scene to.</DialogDescription>
        </DialogHeader>
        <Select value={selectedPreset} onValueChange={setSelectedPreset}>
          <SelectTrigger>
            <SelectValue placeholder="Select preset..." />
          </SelectTrigger>
          <SelectContent>
            {presets && Object.entries(presets).map(([name, preset]) => (
              <SelectItem key={name} value={name}>{preset.display_name ?? name}</SelectItem>
            ))}
          </SelectContent>
        </Select>
        <DialogFooter>
          <Button variant="outline" onClick={() => onOpenChange(false)}>Cancel</Button>
          <Button onClick={handleAdd} disabled={!selectedPreset}>Add to Preset</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}

export default function SceneGallery() {
  const apiUrl = useApiUrl()
  const navigate = useNavigate()
  const [filter, setFilter] = useState('')
  const [selectedScene, setSelectedScene] = useState<ListScenes | null>(null)
  const [dialogOpen, setDialogOpen] = useState(false)
  const [showPreviewOnly, setShowPreviewOnly] = useState(false)

  const { data: scenes, isLoading } = useFetch<ListScenes[]>('/list_scenes')
  const { data: presets } = useFetch<ListPresets>('/list_presets')

  const filtered = (scenes ?? []).filter(s => {
    const matchesName = s.name.toLowerCase().includes(filter.toLowerCase())
    const matchesPreview = !showPreviewOnly || s.has_preview
    return matchesName && matchesPreview
  })

  const handleAddToPreset = async (presetId: string, scene: Scene) => {
    if (!apiUrl) return
    try {
      const res = await fetch(`${apiUrl}/presets?id=${encodeURIComponent(presetId)}`)
      const rawPreset: RawPreset = await res.json()
      const updated: RawPreset = {
        ...rawPreset,
        scenes: [...(rawPreset.scenes ?? []), scene],
      }
      await fetch(`${apiUrl}/preset?id=${encodeURIComponent(presetId)}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(updated),
      })
      navigate(`/modify-preset/${encodeURIComponent(presetId)}`)
    } catch {
      // ignore, user can navigate manually
    }
  }

  const previewCount = (scenes ?? []).filter(s => s.has_preview).length

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold">Scene Gallery</h1>
        <p className="text-muted-foreground text-sm mt-1">
          Browse all available scenes
          {scenes ? ` — ${scenes.length} total, ${previewCount} with previews` : ''}
        </p>
      </div>

      {/* Filters */}
      <div className="flex gap-2 flex-wrap">
        <input
          type="text"
          placeholder="Search scenes..."
          value={filter}
          onChange={e => setFilter(e.target.value)}
          className="flex h-9 rounded-md border border-input bg-background px-3 py-1 text-sm shadow-sm transition-colors placeholder:text-muted-foreground focus-visible:outline-none focus-visible:ring-1 focus-visible:ring-ring flex-1 min-w-0"
        />
        <Button
          variant={showPreviewOnly ? 'default' : 'outline'}
          size="sm"
          onClick={() => setShowPreviewOnly(v => !v)}
        >
          <Badge variant={showPreviewOnly ? 'secondary' : 'outline'} className="mr-1 text-xs">
            {previewCount}
          </Badge>
          With preview
        </Button>
      </div>

      {isLoading ? (
        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-3">
          {Array.from({ length: 10 }).map((_, i) => (
            <div key={i} className="rounded-xl border border-border bg-card overflow-hidden">
              <Skeleton className="aspect-square w-full" />
              <div className="p-3 space-y-2">
                <Skeleton className="h-4 w-3/4" />
                <Skeleton className="h-3 w-1/2" />
              </div>
            </div>
          ))}
        </div>
      ) : filtered.length === 0 ? (
        <div className="text-center py-12 text-muted-foreground text-sm">
          {filter || showPreviewOnly ? 'No scenes match your filter.' : 'No scenes available.'}
        </div>
      ) : (
        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-3">
          {filtered.map(scene => (
            <div
              key={scene.name}
              className="group cursor-pointer relative"
              onClick={() => { setSelectedScene(scene); setDialogOpen(true) }}
            >
              <ScenePreviewCard scene={scene} apiUrl={apiUrl ?? ''} />
              <div className="absolute inset-0 rounded-xl bg-primary/10 opacity-0 group-hover:opacity-100 transition-opacity flex items-center justify-center">
                <div className="bg-primary text-primary-foreground rounded-full p-2 shadow-lg">
                  <Plus className="h-4 w-4" />
                </div>
              </div>
            </div>
          ))}
        </div>
      )}

      <AddToPresetDialog
        scene={selectedScene}
        presets={presets}
        onAdd={handleAddToPreset}
        open={dialogOpen}
        onOpenChange={setDialogOpen}
      />
    </div>
  )
}
