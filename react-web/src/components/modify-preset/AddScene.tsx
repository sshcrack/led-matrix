import { useMemo, useState } from 'react'
import { Check, ImageOff, Plus, Search, SlidersHorizontal } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { Input } from '~/components/ui/input'
import {
  Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription
} from '~/components/ui/dialog'
import type { ListScenes } from '~/apiTypes/list_scenes'
import type { Scene } from '~/apiTypes/list_presets'
import { v4 as uuidv4 } from 'uuid'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import MatrixPreview from '~/components/scene-browser/MatrixPreview'


interface AddSceneProps {
  sceneDefinitions: ListScenes[]
  onAdd: (scene: Scene) => void
}

export default function AddScene({ sceneDefinitions, onAdd }: AddSceneProps) {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState<string>('')
  const [query, setQuery] = useState('')
  const [category, setCategory] = useState('All')
  const apiUrl = useApiUrl()

  const selectedDef = sceneDefinitions.find(s => s.name === selected)
  const categories = useMemo(() => ['All', ...Array.from(new Set(sceneDefinitions.map(s => s.category ?? 'General'))).sort()], [sceneDefinitions])
  const filtered = useMemo(() => sceneDefinitions.filter(scene => {
    const matchesQuery = scene.name.toLowerCase().includes(query.trim().toLowerCase())
    const matchesCategory = category === 'All' || (scene.category ?? 'General') === category
    return matchesQuery && matchesCategory
  }), [sceneDefinitions, query, category])

  const handleAdd = () => {
    if (!selectedDef) return
    const args: Record<string, any> = {}
    for (const prop of selectedDef.properties) args[prop.name] = prop.default_value
    onAdd({ uuid: uuidv4(), type: selectedDef.name, arguments: args })
    setSelected('')
    setQuery('')
    setOpen(false)
  }

  return (
    <>
      <Button className="h-12 w-full gap-2 rounded-xl" onClick={() => setOpen(true)}>
        <Plus className="h-4 w-4" />
        Add scene
      </Button>

      <Dialog open={open} onOpenChange={setOpen}>
        <DialogContent className="flex h-[90vh] max-w-6xl flex-col overflow-hidden p-0 sm:h-[82vh]">
          <DialogHeader className="border-b border-border px-5 py-4">
            <DialogTitle>Choose a scene</DialogTitle>
            <DialogDescription>Search visually, preview it on the virtual matrix, then add it to this preset.</DialogDescription>
          </DialogHeader>

          <div className="grid min-h-0 flex-1 lg:grid-cols-[minmax(0,1fr)_360px]">
            <div className="flex min-h-0 flex-col border-b border-border lg:border-b-0 lg:border-r">
              <div className="space-y-3 border-b border-border p-4">
                <div className="relative">
                  <Search className="absolute left-3 top-1/2 h-4 w-4 -translate-y-1/2 text-muted-foreground" />
                  <Input value={query} onChange={e => setQuery(e.target.value)} placeholder="Search scenes…" className="pl-9" />
                </div>
                <div className="flex gap-2 overflow-x-auto pb-1">
                  {categories.map(item => (
                    <Button key={item} size="sm" variant={category === item ? 'default' : 'outline'} className="shrink-0 rounded-full" onClick={() => setCategory(item)}>
                      {item}
                    </Button>
                  ))}
                </div>
              </div>

              <div className="min-h-0 flex-1 overflow-y-auto p-4">
                <div className="grid grid-cols-2 gap-3 sm:grid-cols-3 xl:grid-cols-4">
                  {filtered.map(scene => {
                    const active = scene.name === selected
                    return (
                      <button key={scene.name} data-selected={active} className="scene-tile" onClick={() => setSelected(scene.name)}>
                        <div className="relative aspect-square bg-black">
                          {scene.has_preview ? (
                            <img src={`${apiUrl}/scene_preview?name=${encodeURIComponent(scene.name)}`} className="h-full w-full object-contain [image-rendering:pixelated]" alt="" />
                          ) : (
                            <div className="flex h-full items-center justify-center text-white/35"><ImageOff className="h-7 w-7" /></div>
                          )}
                          {active && <span className="absolute right-2 top-2 rounded-full bg-primary p-1.5 text-primary-foreground"><Check className="h-3.5 w-3.5" /></span>}
                        </div>
                        <div className="p-3">
                          <div className="truncate text-sm font-semibold">{scene.name}</div>
                          <div className="mt-1 flex items-center justify-between gap-2 text-[11px] text-muted-foreground">
                            <span>{scene.category ?? 'General'}</span>
                            <span className="flex items-center gap-1"><SlidersHorizontal className="h-3 w-3" />{scene.properties.length}</span>
                          </div>
                        </div>
                      </button>
                    )
                  })}
                </div>
              </div>
            </div>

            <aside className="hidden min-h-0 overflow-y-auto bg-secondary/30 p-4 lg:block">
              <MatrixPreview apiUrl={apiUrl ?? ''} sceneName={selectedDef?.name} hasPreview={selectedDef?.has_preview} />
              {selectedDef ? (
                <div className="mt-4 space-y-3">
                  <div className="flex items-start justify-between gap-3">
                    <div><h3 className="font-semibold">{selectedDef.name}</h3><p className="text-xs text-muted-foreground">{selectedDef.category ?? 'General'}</p></div>
                    <Badge variant="secondary">{selectedDef.properties.length} settings</Badge>
                  </div>
                  <div className="flex flex-wrap gap-1.5">
                    {selectedDef.properties.slice(0, 8).map(prop => <Badge key={prop.name} variant="outline" className="font-normal">{prop.name.replaceAll('_', ' ')}</Badge>)}
                  </div>
                </div>
              ) : <p className="mt-4 text-sm text-muted-foreground">Select a scene to inspect it.</p>}
            </aside>
          </div>

          <DialogFooter className="border-t border-border px-5 py-4">
            <div className="mr-auto truncate text-sm text-muted-foreground">{selectedDef ? `${selectedDef.name} selected` : `${filtered.length} scenes`}</div>
            <Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button>
            <Button onClick={handleAdd} disabled={!selectedDef}>Add scene</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  )
}
