import { useState } from 'react'
import { Check, Images, Layers3, Plus, Shuffle, FileStack } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Badge } from '~/components/ui/badge'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription } from '~/components/ui/dialog'
import type { ProviderValue } from '~/apiTypes/list_scenes'
import { newSource, type SourceOption } from '~/core/sceneSources'
import { cn } from '~/lib/utils'

interface AddProviderButtonProps {
  /** One per provider type (`sourceOptions`). */
  options: SourceOption[]
  onAdd: (provider: ProviderValue) => void
}

const icons: Record<string, React.ReactNode> = {
  collection: <Images className="h-5 w-5" />,
  pages: <FileStack className="h-5 w-5" />,
  random: <Shuffle className="h-5 w-5" />,
  shader_collection: <Layers3 className="h-5 w-5" />,
}

export default function AddProviderButton({ options, onAdd }: AddProviderButtonProps) {
  const [open, setOpen] = useState(false)
  const [selected, setSelected] = useState('')

  const handleAdd = () => {
    const option = options.find(o => o.value === selected)
    if (!option) return
    onAdd(newSource(option)); setSelected(''); setOpen(false)
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
