import { View } from 'react-native';
import { Input } from '../../ui/input';
import { Text } from '../../ui/text';
import { PluginPropertyProps } from '../property_list';

export default function DurationProperty({ value }: PluginPropertyProps<number>) {
    return <View className='flex-row gap-2 align-center'>
        <Text className='font-semibold self-center'>Duration</Text>
        <Input
            value={value.toString()}
            autoCorrect={false}
            autoCapitalize='none'
            keyboardType='decimal-pad'
        />
    </View>
}