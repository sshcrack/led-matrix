import type { ReactNode } from 'react'
import { MatrixContext } from './matrixContext'
import { useMatrixState } from './useMatrix'

export function MatrixProvider({ children }: { children: ReactNode }) {
  return <MatrixContext.Provider value={useMatrixState()}>{children}</MatrixContext.Provider>
}
