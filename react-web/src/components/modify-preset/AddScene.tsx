import { useState } from 'react'
import { Plus } from 'lucide-react'
import { Button } from '~/components/ui/button'
import {
  Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription
} from '~/components/ui/dialog'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import type { ListScenes } from '~/apiTypes/list_scenes'
import type { Scene } from '~/apiTypes/list_presets'
import { v4 as uuidv4 } from 'uuid'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import WasmScenePreview from '~/components/WasmScenePreview'

interface AddSceneProps {
  sceneDefinitions: ListScenes[]
  onAdd: (scene: Scene) => void
}

export default function AddScene({ sceneDefinitions, onAdd }: AddSceneProps) {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState<string>('')
  const apiUrl = useApiUrl()

  const selectedDef = sceneDefinitions.find(s => s.name === selected)

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
              {sceneDefinitions.map(s => (
                <SelectItem key={s.name} value={s.name}>{s.name}</SelectItem>
              ))}
            </SelectContent>
          </Select>
          {selected && (
            <div className="rounded-lg border border-border bg-muted overflow-hidden w-full aspect-video flex items-center justify-center">
              <WasmScenePreview
                sceneName={selected}
                hasPreview={selectedDef?.has_preview}
                apiUrl={apiUrl ?? ''}
                className="w-full h-full"
              />
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
