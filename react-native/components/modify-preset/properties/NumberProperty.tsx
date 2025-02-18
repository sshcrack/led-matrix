import { View } from 'react-native';
import { PluginPropertyProps } from '../property_list';
import { Text } from '~/components/ui/text';
import { Input } from '~/components/ui/input';

export default function numberPropertyBuilder(min: number, max: number) {
    return function NumberProperty({ value, setValue }: PluginPropertyProps<number>) {
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
}