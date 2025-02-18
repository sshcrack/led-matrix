import { ReactSetState } from '~/lib/utils';
import { Scene } from '../apiTypes/list_presets';
import { TypeId } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import GeneralProperty from './properties/GeneralProperty';
import ProvidersProperty from './properties/ProvidersProperty';


export type DynamicPluginPropertyProps<T> = {
    propertyName: string,
    typeId: TypeId,
    value: T,
    defaultVal: T,
    setScene: ReactSetState<Scene>
}


export type PluginPropertyProps<T> = {
    propertyName: string,
    typeId: TypeId,
    value: T,
    defaultVal: T,
    setValue: (value: T) => void
}

export const propertyComponents = {
    "general": GeneralProperty,
    "providers": ProvidersProperty
}

type ComponentKeys = keyof typeof propertyComponents;

export function DynamicPluginProperty({ setScene: setData, ...props }: DynamicPluginPropertyProps<any>) {
    const Component = propertyComponents[props.propertyName as ComponentKeys] ?? propertyComponents["general"]

    if (!Component)
        return <Text>Unknown Property type {props.propertyName}</Text>

    return <Component
        setValue={(value) => setData((prev) => {
            if (!prev) {
                console.log("Prev is null")
                return prev
            }

            const prev_value = prev.arguments[props.propertyName]
            if (typeof prev_value !== typeof value) {
                console.error(`Invalid type for ${props.propertyName} expected ${typeof prev_value} got ${typeof value}`)
                return { ...prev }
            }

            const clone = JSON.parse(JSON.stringify(prev))
            clone.arguments[props.propertyName] = value

            return clone
        })}

        {...props}
    />
}