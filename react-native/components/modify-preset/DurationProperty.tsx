import { View } from 'react-native';
import { Input } from '../ui/input';
import { Text } from '../ui/text';

export default function DurationProperty({ value }: { value: number }) {
    return <View className='flex-row gap-2 align-center'>
        <Text className='font-semibold self-center'>Duration</Text>
        <Input value={value.toString()} autoCorrect={false} autoCapitalize='none' keyboardType='decimal-pad'></Input>
    </View>
}