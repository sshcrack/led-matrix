import { useState } from 'react'
import { Plus } from 'lucide-react'
import { Button } from '~/components/ui/button'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription } from '~/components/ui/dialog'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import { toast } from 'sonner'

interface AddPresetButtonProps {
  onCreated: () => void
}

export default function AddPresetButton({ onCreated }: AddPresetButtonProps) {
  const [open, setOpen] = useState(false)
  const [name, setName] = useState('')
  const [loading, setLoading] = useState(false)
  const apiUrl = useApiUrl()

  const handleCreate = async () => {
    if (!name.trim() || !apiUrl) return
    setLoading(true)
    try {
      const res = await fetch(`${apiUrl}/preset?id=${encodeURIComponent(name.trim())}`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ scenes: [], transition_duration: 750, transition_name: 'blend' }),
      })
      if (!res.ok) throw new Error('Failed to create preset')
      toast.success(`Preset "${name.trim()}" created`)
      setName('')
      setOpen(false)
      onCreated()
    } catch (e) {
      toast.error('Failed to create preset')
    } finally {
      setLoading(false)
    }
  }

  return (
    <>
      <Button onClick={() => setOpen(true)} variant="outline" className="w-full gap-2">
        <Plus className="h-4 w-4" />
        New Preset
      </Button>

      <Dialog open={open} onOpenChange={setOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Create Preset</DialogTitle>
            <DialogDescription>Enter a name for the new preset.</DialogDescription>
          </DialogHeader>
          <div className="space-y-2">
            <Label htmlFor="preset-name">Preset Name</Label>
            <Input
              id="preset-name"
              placeholder="My Preset"
              value={name}
              onChange={(e) => setName(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleCreate()}
              autoFocus
            />
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button>
            <Button onClick={handleCreate} disabled={!name.trim() || loading}>
              Create
            </Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  )
}
