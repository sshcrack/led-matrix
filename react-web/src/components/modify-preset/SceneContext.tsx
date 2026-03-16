import { createContext, useContext } from 'react'
import type { Scene } from '~/apiTypes/list_presets'

interface SceneContextType {
  updateScene: (uuid: string, scene: Scene) => void
  deleteScene: (uuid: string) => void
}

export const SceneContext = createContext<SceneContextType>({
  updateScene: () => {},
  deleteScene: () => {},
})

export function useSceneContext() {
  return useContext(SceneContext)
}
