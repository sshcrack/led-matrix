import { View } from 'react-native';
import { ProviderValue } from '~/components/apiTypes/list_scenes';
import { Input } from '~/components/ui/input';
import { Text } from '~/components/ui/text';
import { PluginPropertyProps } from '../property_list';

export default function ProvidersProperty({ defaultVal, propertyName, value }: PluginPropertyProps<ProviderValue>) {
    return <View className='flex-row gap-2'>
        <Text className='font-semibold'>{propertyName}</Text>
        <Text>{JSON.stringify(value)}</Text>
        <Input
            placeholder='Write some stuff...'
            value={JSON.stringify(value)}
            aria-labelledby='inputLabel'
            aria-errormessage='inputError'
        />
        <Text>Default '{JSON.stringify(defaultVal)}'</Text>
    </View>
}