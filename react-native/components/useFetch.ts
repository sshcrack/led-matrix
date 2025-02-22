import { setVisibilityAsync } from 'expo-navigation-bar';
import { useEffect, useState } from 'react';

export function getApiUrl(path: string) {
    return `${process.env.EXPO_PUBLIC_API_URL}${path}`
}

export default function useFetch<T>(path_name: string, timeout: number = 15000) {
    const [data, setData] = useState<T | null>(null);
    const [error, setError] = useState<Error | null>(null);
    const [isLoading, setLoading] = useState(true);
    const [retry, setRetry] = useState(0);

    AbortSignal.timeout ??= function timeout(ms) {
        const ctrl = new AbortController()
        setTimeout(() => ctrl.abort(), ms)
        return ctrl.signal
    }

    useEffect(() => {
        setLoading(true)
        fetch(getApiUrl(path_name), { signal: AbortSignal.timeout(timeout) })
            .then((res) => res.json())
            .then((data) => {
                setData(data)
                setError(null)
            })
            .catch((error) => {
                setError(error)
                setData(null)
            })
            .finally(() => setLoading(false));
    }, [retry])

    return { data, error, isLoading, setRetry }
}