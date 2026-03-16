import React, { createContext, useContext, useState, useEffect } from 'react'
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogDescription, DialogFooter } from '~/components/ui/dialog'
import { Button } from '~/components/ui/button'
import { Input } from '~/components/ui/input'
import { Label } from '~/components/ui/label'

const ApiUrlContext = createContext<string | null>(null)

export function useApiUrl(): string | null {
  return useContext(ApiUrlContext)
}

export function ApiUrlProvider({ children }: { children: React.ReactNode }) {
  const [apiUrl, setApiUrl] = useState<string | null>(null)
  const [showDialog, setShowDialog] = useState(false)
  const [inputValue, setInputValue] = useState('')

  useEffect(() => {
    const isDev = import.meta.env.DEV
    if (!isDev) {
      setApiUrl(window.location.protocol + '//' + window.location.host)
    } else {
      const stored = localStorage.getItem('apiUrl')
      if (stored) {
        setApiUrl(stored)
      } else {
        setShowDialog(true)
      }
    }
  }, [])

  const handleSave = () => {
    const url = inputValue.trim().replace(/\/$/, '')
    if (url) {
      localStorage.setItem('apiUrl', url)
      setApiUrl(url)
      setShowDialog(false)
    }
  }

  return (
    <ApiUrlContext.Provider value={apiUrl}>
      {children}
      <Dialog open={showDialog} onOpenChange={setShowDialog}>
        <DialogContent>
          <DialogHeader>
            <DialogTitle>Configure API URL</DialogTitle>
            <DialogDescription>
              Enter the base URL of the LED matrix controller (e.g., http://192.168.1.100).
            </DialogDescription>
          </DialogHeader>
          <div className="space-y-2">
            <Label htmlFor="api-url">API Base URL</Label>
            <Input
              id="api-url"
              placeholder="http://192.168.1.100"
              value={inputValue}
              onChange={(e) => setInputValue(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleSave()}
            />
          </div>
          <DialogFooter>
            <Button onClick={handleSave}>Save</Button>
          </DialogFooter>
        </DialogContent>
      </Dialog>
    </ApiUrlContext.Provider>
  )
}
