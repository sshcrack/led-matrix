import { useMemo } from 'react'
import { View } from 'react-native'
import { titleCase } from '~/lib/utils'
import { Text } from '../../ui/text'
import { PluginPropertyProps } from '../property_list'
import numberPropertyBuilder from './NumberProperty'

export default function GeneralProperty(props: PluginPropertyProps<any>) {
    const { typeId, propertyName } = props
    const MaybeNumberComp = useMemo(() => {
        if (typeId === "int16_t")
            return numberPropertyBuilder(-32768, 32767, "int")
        if (typeId === "int")
            return numberPropertyBuilder(-2147483648, 2147483647, "int")
        if (typeId === "millis")
            return numberPropertyBuilder(0, 4294967295, "int", true)
        if (typeId === "double" || typeId === "float")
            return numberPropertyBuilder(-3.4028235e38, 3.4028235e38, "float")

        return null
    }, [typeId])

    if (MaybeNumberComp !== null) {
        return <MaybeNumberComp
            {...props}
        />
    }

    return <View className='flex-row gap-2 w-full justify-between'>
        <Text className='font-semibold'>'{titleCase(propertyName)}': Unknown type: {typeId}</Text>
    </View>
}