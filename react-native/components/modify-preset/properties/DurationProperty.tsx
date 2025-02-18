import { View } from 'react-native';
import { Input } from '../../ui/input';
import { Text } from '../../ui/text';
import { PluginPropertyProps } from '../property_list';

export default function DurationProperty({ value, setValue }: PluginPropertyProps<number>) {
    return <View className='flex-row gap-2 w-full justify-between'>
        <Text className='font-semibold self-center'>Duration</Text>
        <Input
            placeholder='Duration'
            value={value === 0 ? "" : value.toString()}
            onChangeText={(text) => {
                const int = parseInt(text)
                setValue(isNaN(int) ? 0 : int)
            }}
            autoCorrect={false}
            autoCapitalize='none'
            keyboardType='decimal-pad'
            className='w-1/2'
        />
    </View>
}