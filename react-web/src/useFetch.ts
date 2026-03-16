import { useEffect, useState } from 'react'
import { useApiUrl } from './components/apiUrl/ApiUrlProvider'

export default function useFetch<T>(path_name: string, timeout: number = 15000) {
  const [data, setData] = useState<T | null>(null)
  const [error, setError] = useState<Error | null>(null)
  const [isLoading, setLoading] = useState(true)
  const [retry, setRetry] = useState(0)
  const apiUrl = useApiUrl()

  useEffect(() => {
    if (!apiUrl) return
    setLoading(true)
    const controller = new AbortController()
    const timer = setTimeout(() => controller.abort(), timeout)
    fetch(apiUrl + path_name, { signal: controller.signal })
      .then((res) => res.json())
      .then((data) => { setData(data); setError(null) })
      .catch((error) => { setError(error); setData(null) })
      .finally(() => { setLoading(false); clearTimeout(timer) })
    return () => { controller.abort(); clearTimeout(timer) }
  }, [retry, apiUrl, path_name, timeout])

  return { data, error, isLoading, setRetry, setData }
}
