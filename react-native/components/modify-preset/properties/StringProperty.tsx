import { View } from 'react-native';
import { Button } from '~/components/ui/button';
import { Input } from '~/components/ui/input';
import { Text } from '~/components/ui/text';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { titleCase } from '~/lib/utils';
import { PluginPropertyProps } from '../property_list';
import { usePropertyUpdate } from '../SceneContext';

export function StringProperty({ value, defaultVal, propertyName }: PluginPropertyProps<string>) {
    const setValue = usePropertyUpdate(propertyName);
    const title = titleCase(propertyName)

    return <View className='flex-row gap-2 w-full justify-between'>
        <Text className='font-semibold self-center'>{title}</Text>
        <View className='w-1/2 gap-2 flex-row'>
            <Button
                variant="secondary"
                size="icon"
                onPress={() => setValue(defaultVal)}
            >
                <RotateCcw className='text-foreground' />
            </Button>
            <Input
                value={value}
                onChangeText={val => setValue(val)}
                className='flex-1'
            />
        </View>
    </View>
}