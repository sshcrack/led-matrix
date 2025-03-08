import { useContext, useEffect, useState } from "react";
import { Text } from "../ui/text";
import { ProviderDataContext } from "./ProviderDataContext";
import { PagesProvider as ApiPagesProvider } from "../apiTypes/list_scenes";
import { View } from "react-native";
import { Input } from '../ui/input';

export default function PagesProvider() {
    const { data: untypedData, setData } = useContext(ProviderDataContext);
    const data = untypedData as ApiPagesProvider | null;
    const [pageBegin, setPageBegin] = useState(data?.arguments?.begin?.toString() ?? "0");
    const [pageEnd, setPageEnd] = useState(data?.arguments?.end?.toString() ?? "-1");

    const onSave = () => {
        setData(e => {
            if (!e)
                return e

            const clone = JSON.parse(JSON.stringify(e)) as ApiPagesProvider;
            const pageBeginInt = parseInt(pageBegin);
            const pageEndInt = parseInt(pageEnd);

            if (!isNaN(pageBeginInt))
                clone.arguments.begin = pageBeginInt;
            if (!isNaN(pageEndInt))
                clone.arguments.end = pageEndInt;

            return clone
        })
    }

    useEffect(() => {
        return () => onSave()
    }, [])

    return (
        <View className='gap-5 pb-5'>
            <View className='flex-row flex-1'>
                <Text className="flex-1">Begin</Text>
                <Input
                    className='flex-1'
                    placeholder="Enter page begin here"
                    value={pageBegin}
                    onBlur={() => onSave()}
                    onChangeText={e => {
                        setPageBegin(e);
                    }}
                    keyboardType='number-pad'
                />
            </View>
            <View className="flex-row flex-1">
                <Text className="flex-1">End (-1 if to end)</Text>
                <Input
                    className='flex-1'
                    placeholder="Enter page end here"
                    value={pageEnd}
                    onBlur={() => onSave()}
                    onChangeText={e => {
                        setPageEnd(e);
                    }}
                    keyboardType='number-pad'
                />
            </View>
        </View>
    );
}
