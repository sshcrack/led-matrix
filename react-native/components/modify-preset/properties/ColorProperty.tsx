import { View } from 'react-native';
import { Button } from '~/components/ui/button';
import { Text } from '~/components/ui/text';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { titleCase } from '~/lib/utils';
import { PluginPropertyProps } from '../property_list';
import { usePropertyUpdate } from '../SceneContext';
import ColorPicker, { Panel1, Swatches, Preview, OpacitySlider, HueSlider } from 'reanimated-color-picker';
import { useState } from 'react';

export function ColorProperty({ value, defaultVal, propertyName }: PluginPropertyProps<string | number>) {
    const setValue = usePropertyUpdate(propertyName);
    const title = titleCase(propertyName);
    const [showPicker, setShowPicker] = useState(false);

    // Convert number to hex string or ensure string is in hex format
    const normalizeColor = (color: string | number): string => {
        if (color === null || color === undefined) return '#000000';
        console.log(`Normalizing color: ${color} ${typeof color}`);
        
        if (typeof color === 'number') {
            // Convert number to hex (e.g., 16777215 -> #ffffff)
            return `#${color.toString(16).padStart(6, '0')}`;
        }
        
        if (typeof color === 'string') {
            if (color.startsWith('#')) return color;
            // Try to parse as number first
            const numColor = parseInt(color, 10);
            if (!isNaN(numColor)) {
                return `#${numColor.toString(16).padStart(6, '0')}`;
            }
            // Treat as hex string
            return `#${color}`;
        }
        
        return '#000000';
    };

    // Convert hex string to number
    const hexToNumber = (hex: string): number => {
        return parseInt(hex.replace('#', ''), 16);
    };

    const displayColor = normalizeColor(value);

    const onSelectColor = ({ hex }: { hex: string }) => {
        // Convert hex to number and save the number value
        const numberValue = hexToNumber(hex);
        setValue(numberValue);
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
