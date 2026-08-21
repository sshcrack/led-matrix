import { useMemo, useState } from 'react'
import { Check, Images, Layers3, Plus, Shuffle, FileStack } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription } from '~/components/ui/dialog'
import type { ProviderValue, ListProviders } from '~/apiTypes/list_scenes'
import { v4 as uuidv4 } from 'uuid'
import { cn } from '~/lib/utils'

interface AddProviderButtonProps {
  providerDefinitions: ListProviders[]
  onAdd: (provider: ProviderValue) => void
  providerType?: 'image' | 'shader'
}

const icons: Record<string, React.ReactNode> = {
  collection: <Images className="h-5 w-5" />,
  pages: <FileStack className="h-5 w-5" />,
  random: <Shuffle className="h-5 w-5" />,
  shader_collection: <Layers3 className="h-5 w-5" />,
}

export default function AddProviderButton({ providerDefinitions, onAdd, providerType }: AddProviderButtonProps) {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState('')
  const options = useMemo(() => {
    const builtIn = providerType === 'shader'
      ? [{ label: 'Random shader', value: 'random', description: 'Pick a shader from a configurable page range.' }, { label: 'Shader collection', value: 'shader_collection', description: 'Use a curated set of shader URLs.' }]
      : providerType === 'image'
        ? [{ label: 'Image collection', value: 'collection', description: 'Select multiple uploaded images as one source.' }, { label: 'Page range', value: 'pages', description: 'Use a contiguous range from the image library.' }]
        : []
    return [...builtIn, ...providerDefinitions.map(p => ({ label: p.name, value: p.name, description: `${p.properties.length} configurable options` }))]
  }, [providerDefinitions, providerType])

  const handleAdd = () => {
    if (!selected) return
    let provider: ProviderValue
    switch (selected) {
      case 'collection': provider = { type: 'collection', uuid: uuidv4(), arguments: { images: [] } }; break
      case 'pages': provider = { type: 'pages', uuid: uuidv4(), arguments: { begin: 0, end: 10 } }; break
      case 'random': provider = { type: 'random', uuid: uuidv4(), arguments: { min_page: 0, max_page: 10 } }; break
      case 'shader_collection': provider = { type: 'shader_collection', uuid: uuidv4(), arguments: { urls: [] } }; break
      default: {
        const def = providerDefinitions.find(p => p.name === selected)
        provider = { type: selected, uuid: uuidv4(), arguments: Object.fromEntries((def?.properties ?? []).map(prop => [prop.name, prop.default_value])) }
      }
    }
    onAdd(provider); setSelected(''); setOpen(false)
  }

  return <>
    <Button className="h-12 w-full gap-2 rounded-xl" onClick={() => setOpen(true)}><Plus className="h-4 w-4" />Add source</Button>
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogContent className="max-w-2xl">
        <DialogHeader><DialogTitle>Add a source</DialogTitle><DialogDescription>Sources decide where this scene gets its images or shaders. Add more than one to build a source pool.</DialogDescription></DialogHeader>
        <div className="grid gap-3 sm:grid-cols-2">
          {options.map(option => <button key={option.value} onClick={() => setSelected(option.value)} className={cn('relative rounded-xl border p-4 text-left transition hover:border-primary/50 hover:bg-secondary/50', selected === option.value && 'border-primary bg-primary/5 ring-2 ring-primary/20')}>
            <div className="mb-3 flex items-start justify-between"><span className="grid h-10 w-10 place-items-center rounded-xl bg-secondary text-primary">{icons[option.value] ?? <Layers3 className="h-5 w-5" />}</span>{selected === option.value && <span className="rounded-full bg-primary p-1 text-primary-foreground"><Check className="h-3.5 w-3.5" /></span>}</div>
            <div className="font-semibold">{option.label}</div><p className="mt-1 text-xs leading-relaxed text-muted-foreground">{option.description}</p>
          </button>)}
        </div>
        <DialogFooter><Badge variant="outline" className="mr-auto">{options.length} source types</Badge><Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button><Button disabled={!selected} onClick={handleAdd}>Add source</Button></DialogFooter>
      </DialogContent>
    </Dialog>
  </>
}
