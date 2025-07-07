import { useEffect, useState } from 'react';

// Debug logging helper with timestamp
const debug = (message: string, data?: any) => {
  const timestamp = new Date().toISOString().split('T')[1].slice(0, -1);
  console.log(`[${timestamp}] [Debounce] ${message}`, data ? data : '');
};

/**
 * Hook that creates a debounced version of a value
 * @param value The value to debounce
 * @param delay The delay in milliseconds
 * @returns The debounced value
 */
export function useDebounce<T>(value: T, delay: number = 500): T {
  const [debouncedValue, setDebouncedValue] = useState<T>(value);

  useEffect(() => {
    debug('Value changed, debouncing', { delay });
    const timer = setTimeout(() => {
      debug('Debounce timer complete, updating value');
      setDebouncedValue(value);
    }, delay);

    return () => {
      debug('Cleaning up debounce timer');
      clearTimeout(timer);
    };
  }, [value, delay]);

  return debouncedValue;
}

export default useDebounce;
