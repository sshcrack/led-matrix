import { View } from 'react-native'
import { Input } from '../../ui/input'
import { Text } from '../../ui/text'
import { PluginPropertyProps } from '../property_list'
import { titleCase } from '~/lib/utils'


export default function GeneralProperty({ defaultVal, propertyName, value }: PluginPropertyProps<any>) {
    if (typeof value === "object") {
        value = JSON.stringify(value)
    }

    if (typeof defaultVal === "object") {
        defaultVal = JSON.stringify(defaultVal)
    }

    let val = value ?? defaultVal
    if(typeof val === "number") {
        val = Math.round(val * 1000) / 1000
    }

    val = JSON.stringify(val)

    return <View className='flex-row gap-2 w-full justify-between'>
        <Text className='font-semibold'>{titleCase(propertyName)}</Text>
        <Input
            className='w-1/2'
            value={val}
            aria-labelledby='inputLabel'
            aria-errormessage='inputError'
        />
    </View>
}