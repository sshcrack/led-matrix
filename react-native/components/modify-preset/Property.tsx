import { View } from 'react-native'
import { Property } from '../apiTypes/list_scenes'
import { Text } from '../ui/text'
import { Input } from '../ui/input'

export type PropertyProps = {
    property: Property,
    propertyName: string,
    value: any,
    defaultVal: any
}

export default function PropertyManage({ defaultVal, property, propertyName, value }: PropertyProps) {
    return <View className='flex-row gap-2'>
        <Text className='font-semibold'>{propertyName}</Text>
        <Text>{value}</Text>
        <Input
            placeholder='Write some stuff...'
            value={value}
            aria-labelledby='inputLabel'
            aria-errormessage='inputError'
        />
        <Text>Default '{defaultVal}'</Text>
    </View>
}