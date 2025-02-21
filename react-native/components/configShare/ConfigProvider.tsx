import { createContext, PropsWithChildren, useContext, useState } from 'react'
import { ReactSetState } from '~/lib/utils'
import { ListScenes } from '../apiTypes/list_scenes'
import _ from "lodash"
import { Preset } from '../apiTypes/list_presets'

export type ConfigState = {
    config: Map<string, Preset>,
    setConfig: ReactSetState<Map<string, Preset>>,
    savePreset: (preset_name: string) => void
}

export const ConfigContext = createContext<ConfigState>({} as ConfigState)

export function useSubConfig(presetName: string, path: string) {
    const { config, setConfig } = useContext(ConfigContext)

    const preset = config.get(presetName)
    const subConfig = preset ? _.get(preset, path) : null

    return {
        config: subConfig,
        setSubConfig: (value: any) => {
            setConfig(e => {
                const curr = new Map(e)
                const copy: Preset = JSON.parse(JSON.stringify(curr.get(presetName)))

                const toSet = _.set(copy, path, value)
                curr.set(presetName, toSet)

                return curr
            })
        }
    }
}

export function ConfigProvider({ children }: PropsWithChildren<{}>) {
    const [config, setConfig] = useState<Map<string, Preset>>(() => new Map())

    return <ConfigContext.Provider value={{
        config,
        setConfig,
        savePreset: (preset_name) => {
            console.log("Saving preset with name", preset_name, "and config", config?.get(preset_name))
        }
    }}>
        {children}
    </ConfigContext.Provider>
}