import { useState } from 'react'
import { Plus, ImageOff } from 'lucide-react'
import { Button } from '~/components/ui/button'
import {
  Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription
} from '~/components/ui/dialog'
import { Select, SelectContent, SelectGroup, SelectItem, SelectLabel, SelectTrigger, SelectValue } from '~/components/ui/select'
import type { ListScenes } from '~/apiTypes/list_scenes'
import type { Scene } from '~/apiTypes/list_presets'
import { v4 as uuidv4 } from 'uuid'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'

interface AddSceneProps {
  sceneDefinitions: ListScenes[]
  onAdd: (scene: Scene) => void
}

export default function AddScene({ sceneDefinitions, onAdd }: AddSceneProps) {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState<string>('')
  const [imgError, setImgError] = useState(false)
  const apiUrl = useApiUrl()

  const selectedDef = sceneDefinitions.find(s => s.name === selected)
  const groupedScenes = sceneDefinitions.reduce<Record<string, ListScenes[]>>((acc, scene) => {
    const category = scene.category ?? 'General'
    if (!acc[category]) acc[category] = []
    acc[category].push(scene)
    return acc
  }, {})

  const handleAdd = () => {
    const def = sceneDefinitions.find(s => s.name === selected)
    if (!def) return
    const args: Record<string, any> = {}
    for (const prop of def.properties) {
      args[prop.name] = prop.default_value
    }
    const scene: Scene = {
      uuid: uuidv4(),
      type: selected,
      arguments: args,
    }
    onAdd(scene)
    setSelected('')
    setOpen(false)
  }

  const handleSelectChange = (value: string) => {
    setSelected(value)
    setImgError(false)
  }

  return (
    <>
      <Button variant="outline" className="gap-2 w-full" onClick={() => setOpen(true)}>
        <Plus className="h-4 w-4" />
        Add Scene
      </Button>

      <Dialog open={open} onOpenChange={setOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Add Scene</DialogTitle>
            <DialogDescription>Choose the type of scene to add.</DialogDescription>
          </DialogHeader>
          <Select value={selected} onValueChange={handleSelectChange}>
            <SelectTrigger>
              <SelectValue placeholder="Select scene type..." />
            </SelectTrigger>
            <SelectContent>
              {Object.entries(groupedScenes).map(([category, scenes]) => (
                <SelectGroup key={category}>
                  <SelectLabel>{category}</SelectLabel>
                  {scenes.map(s => (
                    <SelectItem key={s.name} value={s.name}>{s.name}</SelectItem>
                  ))}
                </SelectGroup>
              ))}
            </SelectContent>
          </Select>
          {selected && (
            <div className="rounded-lg border border-border bg-muted overflow-hidden w-full aspect-video flex items-center justify-center">
              {selectedDef?.has_preview && !imgError ? (
                <img
                  src={`${apiUrl}/scene_preview?name=${encodeURIComponent(selected)}`}
                  alt={`${selected} preview`}
                  className="w-full h-full object-contain"
                  onError={() => setImgError(true)}
                />
              ) : (
                <div className="flex flex-col items-center gap-2 text-muted-foreground p-4">
                  <ImageOff className="h-6 w-6" />
                  <span className="text-xs">No preview available</span>
                </div>
              )}
            </div>
          )}
          <DialogFooter>
            <Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button>
            <Button onClick={handleAdd} disabled={!selected}>Add</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  )
}
