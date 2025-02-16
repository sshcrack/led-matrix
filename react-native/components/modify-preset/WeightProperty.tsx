import { View } from 'react-native';
import { Text } from '../ui/text';

export default function WeightProperty({ value }: { value: number }) {
    return <View className='flex-row gap-2'>
        <Text className='font-semibold'>Weight</Text>
        <Text>{value}</Text>
    </View>
}