import { View } from 'react-native';
import { PluginPropertyProps } from '../property_list';
import { Text } from '~/components/ui/text';
import { titleCase } from '~/lib/utils';
import { Button } from '~/components/ui/button';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { usePropertyUpdate } from '../SceneContext';
import { 
    Select, 
    SelectContent, 
    SelectItem, 
    SelectTrigger, 
    SelectValue 
} from '~/components/ui/select';

interface EnumOption {
    value: string;
    display_name: string;
}

interface EnumPropertyData {
    enum_name: string;
    enum_values: EnumOption[];
}

// The value for enum properties will be a string (the enum value name)
export function EnumProperty({ value, defaultVal, propertyName, additional, ...props }: PluginPropertyProps<string>) {
    const setValue = usePropertyUpdate(propertyName);
    const title = titleCase(propertyName);
    
    // The enum metadata should be available in the property data
    // For now, we'll assume it's passed via a custom prop or context
    // In practice, this data would come from the backend API
    const enumData = additional as EnumPropertyData | undefined;
    
    if (!enumData) {
        return (
            <View className='flex-row gap-2 w-full justify-between'>
                <Text className='font-semibold self-center'>{title}</Text>
                <Text className='text-muted-foreground self-center'>Loading enum options...</Text>
            </View>
        );
    }

    return (
        <View className='flex-row gap-2 w-full justify-between'>
            <Text className='font-semibold self-center'>{title}</Text>
            <View className='w-1/2 gap-2 flex-row'>
                <Button
                    variant="secondary"
                    size="icon"
                    onPress={() => setValue(defaultVal)}
                >
                    <RotateCcw className='text-foreground' />
                </Button>
                <View className='flex-1'>
                    <Select
                        value={{ value, label: enumData.enum_values.find(opt => opt.value === value)?.display_name || value }}
                        onValueChange={(option) => {
                            if (option?.value) {
                                setValue(option.value);
                            }
                        }}
                    >
                        <SelectTrigger>
                            <SelectValue 
                                placeholder={`Select ${title.toLowerCase()}`}
                                className='text-sm text-foreground'
                            />
                        </SelectTrigger>
                        <SelectContent>
                            {enumData.enum_values.map((option) => (
                                <SelectItem 
                                    key={option.value} 
                                    value={option.value}
                                    label={option.display_name}
                                >
                                    <Text>{option.display_name}</Text>
                                </SelectItem>
                            ))}
                        </SelectContent>
                    </Select>
                </View>
            </View>
        </View>
    );
}