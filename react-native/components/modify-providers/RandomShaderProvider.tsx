import { useContext, useEffect, useState } from "react";
import { Text } from "../ui/text";
import { ProviderDataContext } from "./ProviderDataContext";
import { RandomShaderProvider as ApiRandomShaderProvider } from "../apiTypes/list_scenes";
import { View } from "react-native";
import { Input } from '../ui/input';

export default function RandomShaderProvider() {
    const { data: untypedData, setData } = useContext(ProviderDataContext);
    const data = untypedData as ApiRandomShaderProvider | null;
    const [minPage, setMinPage] = useState(data?.arguments?.min_page?.toString() ?? "0");
    const [maxPage, setMaxPage] = useState(data?.arguments?.max_page?.toString() ?? "100");

    const onSave = () => {
        setData(e => {
            if (!e)
                return e

            const clone = JSON.parse(JSON.stringify(e)) as ApiRandomShaderProvider;
            const minPageInt = parseInt(minPage);
            const maxPageInt = parseInt(maxPage);

            if (!isNaN(minPageInt))
                clone.arguments.min_page = minPageInt;
            if (!isNaN(maxPageInt))
                clone.arguments.max_page = maxPageInt;

            return clone
        })
    }

    useEffect(() => {
        return () => onSave()
    }, [])

    return (
        <View className='gap-5 pb-5'>
            <View className='flex-row flex-1'>
                <Text className="flex-1">Min Page</Text>
                <Input
                    className='flex-1'
                    placeholder="Enter min page here"
                    value={minPage}
                    onBlur={() => onSave()}
                    onChangeText={e => {
                        setMinPage(e);
                    }}
                    keyboardType='number-pad'
                />
            </View>
            <View className="flex-row flex-1">
                <Text className="flex-1">Max Page</Text>
                <Input
                    className='flex-1'
                    placeholder="Enter max page here"
                    value={maxPage}
                    onBlur={() => onSave()}
                    onChangeText={e => {
                        setMaxPage(e);
                    }}
                    keyboardType='number-pad'
                />
            </View>
        </View>
    );
}
