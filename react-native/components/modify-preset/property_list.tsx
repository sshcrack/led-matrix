import { Property } from '../apiTypes/list_scenes';
import { Text } from '../ui/text';
import DurationProperty from './properties/DurationProperty';
import GeneralProperty from './properties/GeneralProperty';
import ProvidersProperty from './properties/ProvidersProperty';
import WeightProperty from './properties/WeightProperty';


export type PluginPropertyProps<T> = {
    propertyName: string,
    value: T,
    defaultVal: T
}

export const propertyComponents = {
    "general": GeneralProperty,
    //"duration": DurationProperty,
    //"weight": WeightProperty,
    "providers": ProvidersProperty
}

type ComponentKeys = keyof typeof propertyComponents;

export function DynamicPluginProperty(props: PluginPropertyProps<any>) {
    const Component = propertyComponents[props.propertyName as ComponentKeys] ?? propertyComponents["general"]

    if(!Component)
        return <Text>Unknown Property type {props.propertyName}</Text>

    return <Component {...props} />
}