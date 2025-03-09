import { useMemo } from 'react'
import { View } from 'react-native'
import { titleCase } from '~/lib/utils'
import { Text } from '../../ui/text'
import { PluginPropertyProps } from '../property_list'
import numberPropertyBuilder from './NumberProperty'
import { BooleanProperty } from './BooleanProperty'
import { StringProperty } from './StringProperty'

type TypeMapping = {
    "bool": boolean,
    "int": number,
    "int16_t": number,
    "uint8_t": number,
    "double": number,
    "float": number,
    "millis": number,
    "string": string
}

type PluginPropertyComponent<T> = (({ value, defaultVal, propertyName }: PluginPropertyProps<T>) => React.JSX.Element)

type PropertiesType = {
    [key in keyof TypeMapping]: PluginPropertyComponent<TypeMapping[key]>
}

const Properties: PropertiesType = {
    "int16_t": numberPropertyBuilder(-32768, 32767, "int"),
    "uint8_t": numberPropertyBuilder(0, 255, "int"),
    "int": numberPropertyBuilder(-2147483648, 2147483647, "int"),
    "millis": numberPropertyBuilder(0, 4294967295, "int", true),
    // Float / Double
    "double": numberPropertyBuilder(-3.4028235e38, 3.4028235e38, "float"),
    "float": numberPropertyBuilder(-3.4028235e38, 3.4028235e38, "float"),
    "bool": BooleanProperty,
    "string": StringProperty
}

export default function GeneralProperty(props: PluginPropertyProps<any>) {
    const { typeId, propertyName } = props
    const MaybePropertyComp = useMemo(() => {
        if (typeId in Properties)
            return Properties[typeId as keyof TypeMapping] as PluginPropertyComponent<any>

        return null
    }, [typeId])

    if (MaybePropertyComp !== null) {
        return <MaybePropertyComp
            {...props}
        />
    }

    return <View className='flex-row gap-2 w-full justify-between'>
        <Text className='font-semibold'>'{titleCase(propertyName)}': Unknown type: {typeId}</Text>
    </View>
}