import { ReactSetState } from '~/lib/utils';
import { Text } from '../ui/text';
import GeneralProperty from './properties/GeneralProperty';
import ProvidersProperty from './properties/ProvidersProperty';
import { Preset, Scene } from '../apiTypes/list_presets';
import DurationProperty from './properties/DurationProperty';


export type DynamicPluginPropertyProps<T> = {
    propertyName: string,
    value: T,
    defaultVal: T,
    setScene: ReactSetState<Scene>
}


export type PluginPropertyProps<T> = {
    propertyName: string,
    value: T,
    defaultVal: T,
    setValue: (value: T) => void
}

export const propertyComponents = {
    "general": GeneralProperty,
    "duration": DurationProperty,
    //"weight": WeightProperty,
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