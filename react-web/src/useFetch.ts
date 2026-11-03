import { useEffect, useState } from 'react'
import { useApiUrl } from './components/apiUrl/apiUrlContext'

export default function useFetch<T>(path_name: string, timeout: number = 15000) {
  const [data, setData] = useState<T | null>(null)
  const [error, setError] = useState<Error | null>(null)
  // Identifies the last request that finished. Loading is derived by comparing
  // it with the current request, so no state is set synchronously in the effect.
  const [settledKey, setSettledKey] = useState<string | null>(null)
  const [retry, setRetry] = useState(0)
  const apiUrl = useApiUrl()

  const requestKey = `${retry}\n${apiUrl}\n${path_name}\n${timeout}`
  const isLoading = !apiUrl || settledKey !== requestKey

  useEffect(() => {
    if (!apiUrl) return
    const controller = new AbortController()
    const timer = setTimeout(() => controller.abort(), timeout)
    fetch(apiUrl + path_name, { signal: controller.signal })
      .then((res) => res.json())
      .then((data) => { setData(data); setError(null) })
      .catch((error) => { setError(error); setData(null) })
      .finally(() => { setSettledKey(requestKey); clearTimeout(timer) })
    return () => { controller.abort(); clearTimeout(timer) }
  }, [retry, apiUrl, path_name, timeout, requestKey])

  return { data, error, isLoading, setRetry, setData }
}
