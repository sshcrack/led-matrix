import { useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/ApiUrlProvider'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter, DialogDescription } from '~/components/ui/dialog'
import { Settings } from 'lucide-react'

export default function ResetApiUrl() {
  const [open, setOpen] = useState(false)
  const [newUrl, setNewUrl] = useState('')
  const isDev = import.meta.env.DEV

  if (!isDev) return null

  const handleSave = () => {
    const url = newUrl.trim().replace(/\/$/, '')
    if (url) {
      localStorage.setItem('apiUrl', url)
      window.location.reload()
    }
  }

  return (
    <>
      <button
        onClick={() => setOpen(true)}
        className="text-xs text-muted-foreground hover:text-foreground transition-colors flex items-center gap-1"
      >
        <Settings className="h-3 w-3" />
        Change API URL
      </button>

      <Dialog open={open} onOpenChange={setOpen}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Change API URL</DialogTitle>
            <DialogDescription>Update the base URL for the LED matrix controller.</DialogDescription>
          </DialogHeader>
          <div className="space-y-2">
            <Label>API Base URL</Label>
            <Input
              placeholder={localStorage.getItem('apiUrl') || 'http://192.168.1.100'}
              value={newUrl}
              onChange={e => setNewUrl(e.target.value)}
              onKeyDown={e => e.key === 'Enter' && handleSave()}
            />
          </div>
          <DialogFooter>
            <Button variant="outline" onClick={() => setOpen(false)}>Cancel</Button>
            <Button onClick={handleSave}>Save & Reload</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </>
  )
}
