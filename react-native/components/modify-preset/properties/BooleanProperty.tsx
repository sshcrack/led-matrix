import { titleCase } from '~/lib/utils';
import { PluginPropertyProps } from '../property_list';
import { usePropertyUpdate } from '../SceneContext';
import { View } from 'react-native';
import { Text } from '~/components/ui/text';
import { Button } from '~/components/ui/button';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { Switch } from '~/components/ui/switch';

export function BooleanProperty({ value, defaultVal, propertyName }: PluginPropertyProps<boolean>) {
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
            <View className='flex-1 justify-center'>
                <Switch
                    checked={value}
                    onCheckedChange={(val) => setValue(val)}
                />
            </View>
        </View>
    </View>
}