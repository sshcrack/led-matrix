import { View } from 'react-native';
import { Button } from '~/components/ui/button';
import { Input } from '~/components/ui/input';
import { Text } from '~/components/ui/text';
import { Plus } from '~/lib/icons/Plus';
import { Trash2 } from '~/lib/icons/Trash2';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { titleCase } from '~/lib/utils';
import { PluginPropertyProps } from '../property_list';
import { usePropertyUpdate } from '../SceneContext';

export function StringListProperty({ value, defaultVal, propertyName }: PluginPropertyProps<string[]>) {
    const setValue = usePropertyUpdate(propertyName);
    const title = titleCase(propertyName)

    const addItem = () => {
        setValue([...(value || []), ""]);
    };

    const removeItem = (index: number) => {
        const newValue = [...(value || [])];
        newValue.splice(index, 1);
        setValue(newValue);
    };

    const updateItem = (index: number, text: string) => {
        const newValue = [...(value || [])];
        newValue[index] = text;
        setValue(newValue);
    };

    return (
        <View className='w-full'>
            <View className='flex-row justify-between items-center mb-2'>
                <Text className='font-semibold'>{title}</Text>
                <View className='flex-row gap-2'>
                    <Button variant="secondary" size="icon" onPress={() => setValue(defaultVal)} >
                        <RotateCcw className='text-foreground' />
                    </Button>
                    <Button variant="outline" size="icon" onPress={addItem}>
                        <Plus className='text-foreground' />
                    </Button>
                </View>
            </View>

            <View className='gap-2 pl-2 border-l-2 border-border'>
                {(value || []).map((item, index) => (
                    <View key={index} className='flex-row gap-2 items-center'>
                        <Input
                            value={item}
                            onChangeText={(text) => updateItem(index, text)}
                            className='flex-1'
                        />
                        <Button variant="ghost" size="icon" onPress={() => removeItem(index)}>
                            <Trash2 className='text-destructive' />
                        </Button>
                    </View>
                ))}
                {(value || []).length === 0 && (
                    <Text className='text-muted-foreground italic text-sm py-1'>No items</Text>
                )}
            </View>
        </View>
    );
}
