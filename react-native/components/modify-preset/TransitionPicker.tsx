import { useEffect, useMemo } from 'react';
import { View } from 'react-native';
import { useSubConfig } from '../configShare/ConfigProvider';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import { Text } from '../ui/text';
import useFetch from '../useFetch';
import usePresetId from './PresetIdProvider';

const DEFAULT_TRANSITION_NAME = 'blend';

export default function TransitionPicker() {
    const presetId = usePresetId();
    const { config: transitionName, setSubConfig } = useSubConfig<string>(presetId, 'transition_name');
    const { data: availableTransitions } = useFetch<string[]>('/list_transitions');

    const currentValue = transitionName ?? DEFAULT_TRANSITION_NAME;

    // Build Select option objects
    const options = useMemo(() => {
        const names = availableTransitions && availableTransitions.length > 0
            ? availableTransitions
            : [DEFAULT_TRANSITION_NAME];
        return names.map(name => ({ value: name, label: name.charAt(0).toUpperCase() + name.slice(1) }));
    }, [availableTransitions]);

    const selectedOption = useMemo(
        () => options.find(o => o.value === currentValue) ?? options[0],
        [options, currentValue]
    );

    return (
        <View className='w-full gap-2'>
            <Text className='text-lg font-semibold'>Preset Transition Effect</Text>
            <Text className='text-muted-foreground'>
                Applied when switching between scenes. Can be overridden per scene.
            </Text>
            <Select
                value={selectedOption}
                onValueChange={(option) => {
                    if (option) {
                        setSubConfig(option.value);
                    }
                }}
            >
                <SelectTrigger className='w-full'>
                    <SelectValue
                        className='text-foreground text-sm native:text-lg'
                        placeholder='Select transition…'
                    />
                </SelectTrigger>
                <SelectContent>
                    {options.map((option) => (
                        <SelectItem key={option.value} value={option.value} label={option.label}>
                            {option.label}
                        </SelectItem>
                    ))}
                </SelectContent>
            </Select>
        </View>
    );
}
