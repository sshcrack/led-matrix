import { ReactSetState } from '~/lib/utils';
import { Scene } from '../apiTypes/list_presets';
import { TypeId } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import GeneralProperty from './properties/GeneralProperty';
import ProvidersProperty from './properties/ProvidersProperty';
import { SceneContext } from './SceneContext';
import { useCallback } from 'react';
import { useLocalSearchParams } from 'expo-router';


export type DynamicPluginPropertyProps<T> = {
    propertyName: string,
    typeId: TypeId,
    value: T,
    defaultVal: T,
    setScene: ReactSetState<Scene>,
    sceneId: string
}


export type PluginPropertyProps<T> = {
    propertyName: string,
    typeId: TypeId,
    value: T,
    defaultVal: T
}

export const propertyComponents = {
    "general": GeneralProperty,
    "providers": ProvidersProperty
}

type ComponentKeys = keyof typeof propertyComponents;

export function DynamicPluginProperty({ setScene: setData, sceneId, ...props }: DynamicPluginPropertyProps<any>) {
    const Component = propertyComponents[props.propertyName as ComponentKeys] ?? propertyComponents["general"]

    const local = useLocalSearchParams()
    const presetId = local.id
    if (typeof presetId !== "string")
        return <Text>Invalid preset id {JSON.stringify(presetId)}</Text>

    if (!Component)
        return <Text>Unknown Property type {props.propertyName}</Text>

    return (
        <SceneContext.Provider
            value={{
                sceneId: sceneId,
                presetId: presetId,
                setScene: setData,
                updateProperty: useCallback((propertyName: string, value: any) => {
                    setData((prev) => {
                        if (!prev) return prev;

                        const clone = JSON.parse(JSON.stringify(prev));
                        clone.arguments[propertyName] = value;
                        return clone;
                    });
                }, [setData])
            }}
        >
            <Component {...props} />
        </SceneContext.Provider>
    );
}