import { View } from 'react-native';
import { Button } from '~/components/ui/button';
import { Text } from '~/components/ui/text';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { titleCase } from '~/lib/utils';
import { PluginPropertyProps } from '../property_list';
import { usePropertyUpdate } from '../SceneContext';
import ColorPicker, { Panel1, Swatches, Preview, OpacitySlider, HueSlider } from 'reanimated-color-picker';
import { useState } from 'react';

export function ColorProperty({ value, defaultVal, propertyName }: PluginPropertyProps<string>) {
    const setValue = usePropertyUpdate(propertyName);
    const title = titleCase(propertyName);
    const [showPicker, setShowPicker] = useState(false);

    // Ensure the color value is a valid hex format
    const normalizeColor = (color: string): string => {
        if (!color) return '#000000';
        if (color.startsWith('#')) return color;
        return `#${color}`;
    };

    const displayColor = normalizeColor(value);

    const onSelectColor = ({ hex }: { hex: string }) => {
        // Remove the # prefix when saving to match the expected format
        setValue(hex.replace('#', ''));
        setShowPicker(false);
    };

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
                <View className='flex-1 flex-row gap-2'>
                    <Button
                        variant="outline"
                        className='flex-1'
                        onPress={() => setShowPicker(true)}
                    >
                        <View className='flex-row items-center gap-2'>
                            <View 
                                className='w-6 h-6 rounded border border-border'
                                style={{ backgroundColor: displayColor }}
                            />
                            <Text className='text-sm'>{displayColor}</Text>
                        </View>
                    </Button>
                </View>
            </View>
            
            {showPicker && (
                <View className='absolute top-0 left-0 right-0 bottom-0 bg-black/50 justify-center items-center z-50'>
                    <View className='bg-card p-4 rounded-lg m-4 max-w-sm w-full'>
                        <ColorPicker
                            value={displayColor}
                            onComplete={onSelectColor}
                            onChange={() => {}}
                        >
                            <Preview />
                            <Panel1 />
                            <HueSlider />
                            <OpacitySlider />
                            <Swatches />
                        </ColorPicker>
                        <View className='flex-row gap-2 mt-4 justify-end'>
                            <Button
                                variant="outline"
                                onPress={() => setShowPicker(false)}
                            >
                                <Text>Cancel</Text>
                            </Button>
                        </View>
                    </View>
                </View>
            )}
        </View>
    );
}
