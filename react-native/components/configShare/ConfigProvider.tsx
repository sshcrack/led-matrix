import _ from "lodash"
import { createContext, PropsWithChildren, useContext, useState } from 'react'
import { ReactSetState } from '~/lib/utils'
import { objectToArrayPresets, Preset } from '../apiTypes/list_presets'
import Toast from 'react-native-toast-message'
import { useApiUrl } from '../apiUrl/ApiUrlProvider'

export type ConfigState = {
    config: Map<string, Preset>,
    setConfig: ReactSetState<Map<string, Preset>>,
    savePreset: (preset_name: string) => Promise<void>,
    update: number,
    setUpdate: ReactSetState<number>
}

export const ConfigContext = createContext<ConfigState>({} as ConfigState)

export type SubConfigReturnType<T> = {
    config: T,
    setSubConfig: (value: React.SetStateAction<T | null>) => void
}

export function useSubConfig<T>(presetName: string, path: Parameters<typeof _.get>[1]) {
    const { config, setConfig } = useContext(ConfigContext)

    const preset = config.get(presetName)
    const subConfig = preset ? _.get(preset, path) : null

    const toReturn: SubConfigReturnType<T> = {
        config: subConfig as T,
        setSubConfig: (valueOrFn: (T | null) | ((prev: T | null) => (T | null))) => {
            setConfig(e => {
                const curr = new Map(e)
                const currOriginal = curr.get(presetName);
                if (!currOriginal) {
                    console.error("No preset found with name", presetName)
                    return curr
                }

                const copy: Preset = JSON.parse(JSON.stringify(currOriginal))
                const newValue = typeof valueOrFn === 'function'
                    ? (valueOrFn as (prev: T) => T)(_.get(copy, path))
                    : valueOrFn

                const toSet = _.set(copy, path, newValue)
                curr.set(presetName, toSet)

                return curr
            })
        }
    }

    return toReturn
}

export function ConfigProvider({ children }: PropsWithChildren<{}>) {
    const [config, setConfig] = useState<Map<string, Preset>>(() => new Map())
    const [update, setUpdate] = useState(0)
    const apiUrl = useApiUrl()

    return <ConfigContext.Provider value={{
        config,
        setConfig,
        update,
        setUpdate,
        savePreset: async (presetId) => {
            const raw = objectToArrayPresets(config.get(presetId)!)
            fetch(apiUrl + `/preset?id=${encodeURIComponent(presetId)}`, {
                method: "POST",
                body: JSON.stringify(raw),
                headers: {
                    'Content-Type': 'application/json'
                }
            })
                .then(() => console.log("Successfully saved preset"))
                .finally(() => {
                    setUpdate(Math.random())
                })
        }
    }}>
        {children}
    </ConfigContext.Provider>
}