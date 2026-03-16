import { useEffect, useState } from 'react'

type ColorScheme = 'light' | 'dark'

export function useColorScheme(): ColorScheme {
  const [scheme, setScheme] = useState<ColorScheme>(() => {
    const stored = localStorage.getItem('colorScheme')
    if (stored === 'dark' || stored === 'light') return stored
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light'
  })

  useEffect(() => {
    const root = document.documentElement
    if (scheme === 'dark') {
      root.classList.add('dark')
    } else {
      root.classList.remove('dark')
    }
    localStorage.setItem('colorScheme', scheme)
  }, [scheme])

  return scheme
}

export function useSetColorScheme() {
  return (scheme: ColorScheme) => {
    const root = document.documentElement
    if (scheme === 'dark') {
      root.classList.add('dark')
    } else {
      root.classList.remove('dark')
    }
    localStorage.setItem('colorScheme', scheme)
  }
}

export function toggleColorScheme() {
  const root = document.documentElement
  const isDark = root.classList.contains('dark')
  if (isDark) {
    root.classList.remove('dark')
    localStorage.setItem('colorScheme', 'light')
  } else {
    root.classList.add('dark')
    localStorage.setItem('colorScheme', 'dark')
  }
}

export function initColorScheme() {
  const stored = localStorage.getItem('colorScheme')
  const prefersDark = window.matchMedia('(prefers-color-scheme: dark)').matches
  if (stored === 'dark' || (!stored && prefersDark)) {
    document.documentElement.classList.add('dark')
  }
}
