import { View } from 'react-native';
import { PluginPropertyProps } from '../property_list';
import { Text } from '~/components/ui/text';
import { Input } from '~/components/ui/input';
import { titleCase } from '~/lib/utils';
import { Button } from '~/components/ui/button';
import { RotateCcw } from '~/lib/icons/RotateCcw';
import { parse, format } from '@lukeed/ms';
import { useEffect, useMemo, useState } from 'react';
import { KeyboardTypeOptions } from 'react-native';
import { usePropertyUpdate } from '../SceneContext';

export type NumberType = "float" | "int"

export default function numberPropertyBuilder(min: number, max: number, number_type: NumberType = "int", millis: boolean = false) {
    return function NumberProperty({ value, defaultVal, propertyName, additional }: PluginPropertyProps<number>) {
        const setValue = usePropertyUpdate(propertyName);
        const title = titleCase(propertyName)

        const minPropVal = additional?.min ?? min;
        const maxPropVal = additional?.max ?? max;

        const valueStr = useMemo(() => {
            let numVal = value
            if (number_type === "float") {
                numVal = Math.round(value * 1000) / 1000
            }

            return millis ? format(numVal) as string : numVal.toString()
        }, [value])
        const [modifiedVal, setModifiedVal] = useState<string>(valueStr)

        useEffect(() => {
            setModifiedVal(valueStr)
        }, [valueStr])

        const keyboardType: KeyboardTypeOptions = millis ?
            "default"
            : number_type === "float" ? "decimal-pad" : "number-pad"
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
                    placeholder={title}
                    onBlur={() => {
                        let int = NaN
                        if (millis) {
                            int = parse(modifiedVal) ?? NaN
                        }

                        if (isNaN(int))
                            int = number_type === "float" ? parseFloat(modifiedVal) : parseInt(modifiedVal)
                        const toSet = isNaN(int) ? defaultVal : Math.max(minPropVal, int)

                        setValue(Math.min(toSet, maxPropVal))
                    }}
                    value={modifiedVal}
                    onChangeText={(text) => setModifiedVal(text)}
                    autoCorrect={false}
                    autoCapitalize='none'
                    keyboardType={keyboardType}
                    className='flex-1'
                />
            </View>
        </View>
    }
}