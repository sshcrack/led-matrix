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

export type NumberType = "float" | "int"

export default function numberPropertyBuilder(min: number, max: number, number_type: NumberType = "int", millis: boolean = false) {
    return function NumberProperty({ value, defaultVal, setValue, propertyName }: PluginPropertyProps<number>) {
        const title = titleCase(propertyName)

        const valueStr = useMemo(() => {
            let numVal = value
            if (number_type === "float") {
                numVal = Math.round(value * 1000) / 1000
            }

            const strVal = millis ? format(numVal) as string : numVal.toString()

            return numVal === 0 ? "" : strVal
        }, [value])
        const [modifiedVal, setModifiedVal] = useState<string>(valueStr)
        const [_, setUpdate] = useState(0)

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
                        const toSet = isNaN(int) ? min : Math.max(min, int)

                        setValue(Math.min(toSet, max))
                        setUpdate(Math.random())
                    }}
                    value={modifiedVal}
                    onChangeText={(text) => setModifiedVal(text)}
                    autoCorrect={false}
                    autoCapitalize='none'
                    keyboardType={keyboardType}
                    className='w-full'
                />
            </View>
        </View>
    }
}