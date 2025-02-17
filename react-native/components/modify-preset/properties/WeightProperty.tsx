import { View } from 'react-native';
import { Text } from '../../ui/text';
import { PluginPropertyProps } from '../property_list';

export default function WeightProperty({ value }: PluginPropertyProps<number>) {
    return <View className='flex-row gap-2'>
        <Text className='font-semibold'>Weight</Text>
        <Text>{value}</Text>
    </View>
}