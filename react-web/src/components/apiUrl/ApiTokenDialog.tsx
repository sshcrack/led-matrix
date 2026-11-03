import { useEffect, useState } from 'react'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription, DialogFooter } from '~/components/ui/dialog'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'
import { setApiToken, UNAUTHORIZED_EVENT } from '~/lib/apiToken'

export default function ApiTokenDialog() {
  const [open, setOpen] = useState(false)
  const [value, setValue] = useState('')

  useEffect(() => {
    const show = () => setOpen(true)
    window.addEventListener(UNAUTHORIZED_EVENT, show)
    return () => window.removeEventListener(UNAUTHORIZED_EVENT, show)
  }, [])

  const save = () => {
    const token = value.trim()
    if (!token) return
    setApiToken(token)
    window.location.reload()
  }

  return (
    <Dialog open={open} onOpenChange={setOpen}>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>API token required</DialogTitle>
          <DialogDescription>
            This matrix requires an API token. Enter the value of LED_MATRIX_API_TOKEN from /etc/default/led-matrix.
          </DialogDescription>
        </DialogHeader>
        <div className="space-y-2">
          <Label htmlFor="api-token">API token</Label>
          <Input
            id="api-token"
            type="password"
            autoComplete="current-password"
            value={value}
            onChange={(e) => setValue(e.target.value)}
            onKeyDown={(e) => e.key === 'Enter' && save()}
          />
        </div>
        <DialogFooter>
          <Button onClick={save}>Save</Button>
        </DialogFooter>
      </DialogContent>
    </Dialog>
  )
}
