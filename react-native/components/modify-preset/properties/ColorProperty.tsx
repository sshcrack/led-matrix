import { View } from 'react-native';
import { Button } from '~/components/ui/button';
import { Text } from '~/components/ui/text';
import { Dialog, DialogContent, DialogHeader, DialogTitle, DialogFooter } from '~/components/ui/dialog';
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
    const [tempColor, setTempColor] = useState<string>('');

    // Ensure the color value is a valid hex format
    const normalizeColor = (color: string | number): string => {
        // Handle null or undefined
        if (color === null || color === undefined) return '#000000';

        // Convert number to string if needed
        const colorStr = typeof color === 'number' ? color.toString(16).padStart(6, '0') : String(color);

        // Handle hex format
        if (colorStr.startsWith('#')) return colorStr;
        return `#${colorStr}`;
    };

    const displayColor = normalizeColor(value);

    const onColorChange = ({ hex }: { hex: string }) => {
        setTempColor(hex);
    };

    const onConfirmColor = () => {
        if (tempColor) {
            // Remove the # prefix when saving to match the expected format
            const hexWithoutPrefix = tempColor.replace('#', '');

            // If the defaultVal is a number, convert hex to number before saving
            if (typeof defaultVal === 'number') {
                setValue(parseInt(hexWithoutPrefix, 16));
            } else {
                setValue(hexWithoutPrefix);
            }
        }
        setShowPicker(false);
        setTempColor('');
    };

    const onCancelColor = () => {
        setShowPicker(false);
        setTempColor('');
    };

    const onOpenPicker = () => {
        console.log('Opening color picker with value:', displayColor);
        setTempColor(displayColor);
        setShowPicker(true);
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
                        onPress={() => onOpenPicker()}
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

            <Dialog open={showPicker} onOpenChange={setShowPicker}>
                <DialogContent className='max-w-sm'>
                    <DialogHeader>
                        <DialogTitle>
                            <Text className='text-lg font-semibold'>Choose Color</Text>
                        </DialogTitle>
                    </DialogHeader>

                    <View className='py-4'>
                        <ColorPicker
                            value={tempColor || displayColor}
                            onCompleteJS={() => { }} // Don't close on complete
                            onChangeJS={onColorChange} // Update temp color
                        >
                            <View className='mb-4'>
                                <Preview />
                            </View>
                            <View className='mb-4'>
                                <Panel1 />
                            </View>
                            <View className='mb-3'>
                                <HueSlider />
                            </View>
                            <View className='mb-4'>
                                <OpacitySlider />
                            </View>
                        </ColorPicker>
                    </View>

                    <DialogFooter>
                        <View className='flex-row gap-2 justify-end'>
                            <Button
                                variant="outline"
                                onPress={onCancelColor}
                            >
                                <Text>Cancel</Text>
                            </Button>
                            <Button
                                variant="default"
                                onPress={onConfirmColor}
                            >
                                <Text>Confirm</Text>
                            </Button>
                        </View>
                    </DialogFooter>
                </DialogContent>
            </Dialog>
        </View>
    );
}
