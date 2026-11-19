import { useEffect, useRef, useState } from 'react'
import type { HaloSettings } from './HaloCard'

export interface HaloAlignmentLayout {
  led_count: number
  first_led?: number
  reverse?: boolean
}

export const wrap = (position: number, count: number) => (position % count + count) % count
export const maxLeds = 120 // Stock Pro Micro firmware limit.

export function initialHaloAlignment(settings: HaloSettings) {
  const legacyAnchor = 2 * (settings.rotation - (settings.reverse ? 1 : 0))
  const legacyMidpoint = settings.start_between_zones ? (settings.reverse ? -1 : 1) : 0
  const initialCount = Math.min(settings.led_count, maxLeds)
  const initialSlot = settings.first_led !== undefined && settings.first_led >= 0
    ? Math.round(settings.first_led * initialCount / settings.led_count)
    : Math.round((legacyAnchor + legacyMidpoint + 1) * initialCount / 16)
  return { led_count: initialCount, first_led: wrap(initialSlot, initialCount), reverse: settings.reverse }
}

// The daemon lease expires after 20 seconds even if a browser loses its connection.
export function useHaloAlignment(apiUrl: string | null, layout: HaloAlignmentLayout) {
  const currentLayout = useRef(layout)
  const refresh = useRef<(() => void) | undefined>(undefined)
  const requests = useRef(Promise.resolve())
  const [status, setStatus] = useState<'starting' | 'active' | 'unavailable'>('starting')
  useEffect(() => {
    currentLayout.current = layout
    refresh.current?.()
  }, [layout])

  useEffect(() => {
    if (!apiUrl) return

    let closed = false
    let timer: ReturnType<typeof setTimeout> | undefined
    const request = async (draft: HaloAlignmentLayout) => {
      const response = await fetch(`${apiUrl}/halo/test`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(draft),
        signal: AbortSignal.timeout(4000),
        keepalive: draft.led_count === 0,
      })
      if (!response.ok) throw new Error('Alignment request failed')
      return (await response.json() as { test_active: boolean }).test_active
    }
    let pending = false
    const renew = () => {
      clearTimeout(timer)
      if (pending) return
      pending = true
      requests.current = requests.current.then(async () => {
        if (closed) return
        const draft = currentLayout.current
        try {
          const active = await request(draft)
          if (!closed) setStatus(active ? 'active' : 'unavailable')
        } catch {
          if (!closed) setStatus('unavailable')
        }
        pending = false
        if (!closed) timer = setTimeout(renew, draft === currentLayout.current ? 4000 : 0)
      })
    }
    refresh.current = renew
    renew()

    return () => {
      closed = true
      refresh.current = undefined
      clearTimeout(timer)
      requests.current = requests.current.then(() => request({ led_count: 0 }).then(() => {}, () => {}))
    }
  }, [apiUrl])

  return status
}
