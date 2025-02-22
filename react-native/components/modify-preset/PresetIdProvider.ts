import { createContext, useContext } from 'react'

export type PresetIdState = string
export const PresetIdContext = createContext<PresetIdState>("")

export default function usePresetId() {
    const presetId = useContext(PresetIdContext)
    return presetId
}