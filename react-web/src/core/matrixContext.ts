import { createContext, useContext } from 'react'
import type { Matrix } from './useMatrix'

export const MatrixContext = createContext<Matrix | null>(null)

export function useMatrix(): Matrix {
  const ctx = useContext(MatrixContext)
  if (!ctx) throw new Error('useMatrix must be used inside <MatrixProvider>')
  return ctx
}
