import { useState } from 'react'
import { Plus } from 'lucide-react'
import { Button } from '~/components/ui/button'
import {
  Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription
} from '~/components/ui/dialog'
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '~/components/ui/select'
import type { ProviderValue, ListProviders } from '~/apiTypes/list_scenes'
import { v4 as uuidv4 } from 'uuid'

interface AddProviderButtonProps {
  providerDefinitions: ListProviders[]
  onAdd: (provider: ProviderValue) => void
  providerType?: 'image' | 'shader'
}

export default function AddProviderButton({ providerDefinitions, onAdd, providerType }: AddProviderButtonProps) {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState<string>('')

  const handleAdd = () => {
    if (!selected) return

    let provider: ProviderValue
    switch (selected) {
      case 'collection':
        provider = { type: 'collection', uuid: uuidv4(), arguments: { images: [] } }
        break
      case 'pages':
        provider = { type: 'pages', uuid: uuidv4(), arguments: { begin: 0, end: 10 } }
        break
      case 'random':
        provider = { type: 'random', uuid: uuidv4(), arguments: { min_page: 0, max_page: 10 } }
        break
      case 'shader_collection':
        provider = { type: 'shader_collection', uuid: uuidv4(), arguments: { urls: [] } }
        break
      default: {
        const def = providerDefinitions.find(p => p.name === selected)
        const args: Record<string, any> = {}
        for (const prop of def?.properties ?? []) {
          args[prop.name] = prop.default_value
        }
        provider = { type: selected, uuid: uuidv4(), arguments: args }
      }
    }

    onAdd(provider)
    setSelected('')
    setOpen(false)
  }

  const typeOptions =
    providerType === 'shader'
      ? [{ label: 'Random Shader', value: 'random' }, { label: 'Shader Collection', value: 'shader_collection' }]
      : providerType === 'image'
      ? [{ label: 'Image Collection', value: 'collection' }, { label: 'Pages', value: 'pages' }]
      : [
          { label: 'Image Collection', value: 'collection' },
          { label: 'Pages', value: 'pages' },
          { label: 'Random Shader', value: 'random' },
          { label: 'Shader Collection', value: 'shader_collection' },
        ]

  return (
    <>
      <Button variant="outline" className="gap-2 w-full" onClick={() => setOpen(true)}>
        <Plus className="h-4 w-4" />
        Add Provider
      </Button>

      <Dialog open={open} onOpenChange={setOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Add Provider</DialogTitle>
            <DialogDescription>Select the type of provider to add.</DialogDescription>
          </DialogHeader>
          <Select value={selected} onValueChange={setSelected}>
            <SelectTrigger>
              <SelectValue placeholder="Select provider type..." />
            </SelectTrigger>
            <SelectContent>
              {typeOptions.map(opt => (
                <SelectItem key={opt.value} value={opt.value}>{opt.label}</SelectItem>
              ))}
              {providerDefinitions.map(p => (
                <SelectItem key={p.name} value={p.name}>{p.name}</SelectItem>
              ))}
            </SelectContent>
          </Select>
          <DialogFooter>
            <Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button>
            <Button onClick={handleAdd} disabled={!selected}>Add</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  )
}
