import { useEffect } from 'react'
import type { ReactNode } from 'react'
import { useApiUrl } from '~/components/apiUrl/apiUrlContext'
import { MatrixContext } from './matrixContext'
import { loadPluginUis } from './pluginRegistry'
import { useMatrixState } from './useMatrix'

export function MatrixProvider({ children }: { children: ReactNode }) {
  const apiUrl = useApiUrl()
  useEffect(() => { if (apiUrl !== null) void loadPluginUis(apiUrl) }, [apiUrl])
  return <MatrixContext.Provider value={useMatrixState()}>{children}</MatrixContext.Provider>
}
