import { useCallback, useState } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { toast } from 'sonner'
import type { UpdateStatus } from '~/apiTypes/update'

export type InstallState = 'idle' | 'installing' | 'success' | 'error'

export function useUpdateInstallation(onStatusChange?: () => void) {
  const apiUrl = useApiUrl()
  const [installState, setInstallState] = useState<InstallState>('idle')
  const [installError, setInstallError] = useState<string | null>(null)

  const checkForUpdates = useCallback(async () => {
    if (!apiUrl) return
    try {
      const res = await fetch(`${apiUrl}/api/update/check`, { method: 'POST' })
      if (!res.ok) throw new Error(`HTTP ${res.status}`)
      const data = await res.json()
      if (data.update_available) {
        toast.success(`Update available: ${data.version || 'new version'}`)
      } else {
        toast.info('Already on latest version')
      }
      onStatusChange?.()
    } catch {
      toast.error('Failed to check for updates')
    }
  }, [apiUrl, onStatusChange])

  const installUpdate = useCallback(async (version: string) => {
    if (!apiUrl) return
    setInstallState('installing')
    setInstallError(null)
    try {
      const res = await fetch(`${apiUrl}/api/update/install?version=${encodeURIComponent(version)}`, { method: 'POST' })
      if (!res.ok) throw new Error((await res.text().catch(() => '')) || `HTTP ${res.status}`)
      setInstallState('success')
      toast.success('Update installation started')
      onStatusChange?.()
    } catch (e) {
      setInstallState('error')
      setInstallError((e as Error).message)
      toast.error(`Failed to install update: ${(e as Error).message}`)
    }
  }, [apiUrl, onStatusChange])

  const isInProgress = (status: UpdateStatus | null) => {
    return status?.status === 1 || status?.status === 2 || status?.status === 3
  }

  return { installState, installError, checkForUpdates, installUpdate, isInProgress }
}
