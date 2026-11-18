import { useEffect, useRef, useState } from 'react'

// The daemon lease expires after 20 seconds even if a browser loses its connection.
export function useHaloAlignment(apiUrl: string | null, ledCount: number) {
  const count = useRef(ledCount)
  const requests = useRef(Promise.resolve())
  const [status, setStatus] = useState<'starting' | 'active' | 'unavailable'>('starting')
  useEffect(() => { count.current = ledCount }, [ledCount])

  useEffect(() => {
    if (!apiUrl) return

    let closed = false
    let timer: ReturnType<typeof setTimeout> | undefined
    const request = async (led_count: number) => {
      const response = await fetch(`${apiUrl}/halo/test`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ led_count }),
        signal: AbortSignal.timeout(4000),
        keepalive: led_count === 0,
      })
      if (!response.ok) throw new Error('Alignment request failed')
      return (await response.json() as { test_active: boolean }).test_active
    }
    const renew = () => {
      requests.current = requests.current.then(async () => {
        if (closed) return
        try {
          const active = await request(count.current)
          if (!closed) setStatus(active ? 'active' : 'unavailable')
        } catch {
          if (!closed) setStatus('unavailable')
        }
        if (!closed) timer = setTimeout(renew, 4000)
      })
    }
    renew()

    return () => {
      closed = true
      clearTimeout(timer)
      requests.current = requests.current.then(() => request(0).then(() => {}, () => {}))
    }
  }, [apiUrl])

  return status
}
