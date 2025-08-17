import { useCallback } from 'react';
import { Scene } from '../apiTypes/list_presets';
import { TypeId } from '../apiTypes/list_scenes';
import { useSubConfig } from '../configShare/ConfigProvider';
import { Text } from '../ui/text';
import usePresetId from './PresetIdProvider';
import GeneralProperty from './properties/GeneralProperty';
import ProvidersProperty from './properties/ProvidersProperty';
import { SceneContext } from './SceneContext';


export type DynamicPluginPropertyProps<T> = {
    propertyName: string,
    typeId: TypeId,
    value: T,
    defaultVal: T,
    additional: any,
    sceneId: string
}


export type PluginPropertyProps<T> = {
    propertyName: string,
    additional?: any,
    typeId: TypeId,
    value: T,
    defaultVal: T
}

export const propertyComponents = {
    "general": GeneralProperty,
    "providers": ProvidersProperty
}

type ComponentKeys = keyof typeof propertyComponents;

export function DynamicPluginProperty({ sceneId, ...props }: DynamicPluginPropertyProps<any>) {
    const Component = propertyComponents[props.propertyName as ComponentKeys] ?? propertyComponents["general"]
    const presetId = usePresetId()

    const { setSubConfig } = useSubConfig<Scene>(presetId, ["scenes", sceneId])

    if (!Component)
        return <Text>Unknown Property type {props.propertyName}</Text>

    return (
        <SceneContext.Provider
            value={{
                sceneId: sceneId,
                updateProperty: useCallback((propertyName: string, value: any) => {
                    setSubConfig((prev) => {
                        if (!prev) return prev;

                        const clone = JSON.parse(JSON.stringify(prev));
                        clone.arguments[propertyName] = value;
                        return clone;
                    });
                }, [setSubConfig])
            }}
        >
            <Component {...props} />
        </SceneContext.Provider>
    );
}