import { format, parse } from '@lukeed/ms';
import { useEffect, useMemo, useState } from 'react';
import { View } from 'react-native';
import { useSubConfig } from '../configShare/ConfigProvider';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Text } from '../ui/text';
import usePresetId from './PresetIdProvider';
import { RotateCcw } from '~/lib/icons/RotateCcw';

const DEFAULT_TRANSITION_DURATION = 750;

export default function PresetTransitionDuration() {
    const presetId = usePresetId();
    const { config: transitionDuration, setSubConfig } = useSubConfig<number>(presetId, 'transition_duration');

    const normalizedValue = useMemo(() => {
        const value = transitionDuration ?? DEFAULT_TRANSITION_DURATION;
        return Math.max(0, value);
    }, [transitionDuration]);

    const [value, setValue] = useState<string>(format(normalizedValue) as string);

    useEffect(() => {
        setValue(format(normalizedValue) as string);
    }, [normalizedValue]);

    return (
        <View className='w-full gap-2'>
            <Text className='text-lg font-semibold'>Preset Transition Duration</Text>
            <Text className='text-muted-foreground'>Used as fallback when a scene transition duration is set to 0.</Text>
            <View className='flex-row gap-2'>
                <Button
                    variant='secondary'
                    size='icon'
                    onPress={() => setSubConfig(DEFAULT_TRANSITION_DURATION)}
                >
                    <RotateCcw className='text-foreground' />
                </Button>
                <Input
                    className='flex-1'
                    value={value}
                    placeholder='750ms'
                    autoCapitalize='none'
                    autoCorrect={false}
                    onChangeText={setValue}
                    onBlur={() => {
                        const parsed = parse(value);
                        if (parsed === null || Number.isNaN(parsed)) {
                            setSubConfig(DEFAULT_TRANSITION_DURATION);
                            return;
                        }

                        setSubConfig(Math.max(0, Number(parsed)));
                    }}
                />
            </View>
        </View>
    );
}
