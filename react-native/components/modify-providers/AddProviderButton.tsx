import { useState } from 'react';
import { ScrollView, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import Toast from 'react-native-toast-message';
import uuid from 'react-native-uuid';
import { titleCase } from '~/lib/utils';
import { ListProviders, ProviderValue } from '../apiTypes/list_scenes';
import { useSubConfig } from '../configShare/ConfigProvider';
import { Button } from '../ui/button';
import { Option, Select, SelectContent, SelectGroup, SelectItem, SelectLabel, SelectTrigger, SelectValue } from '../ui/select';
import { Text } from '../ui/text';

export default function AddProviderButton({ providers, presetId, sceneId }: { providers: ListProviders[], presetId: string, sceneId: string }) {
    const [opt, setValue] = useState<Option>({ value: providers[0].name, label: titleCase(providers[0].name) })

    const insets = useSafeAreaInsets();
    const contentInsets = {
        top: insets.top,
        bottom: insets.bottom,
        left: 12,
        right: 12,
    };


    const { setSubConfig } = useSubConfig<ProviderValue[]>(presetId, ["scenes", sceneId, "arguments", "providers"])
    return <View className='w-full flex-row'>
        <Select
            className='flex-1 h-full rounded-r-none'
            value={opt}
            onValueChange={e => setValue(e)}
        >
            <SelectTrigger>
                <SelectValue
                    className='text-foreground text-sm native:text-lg'
                    placeholder='Select a provider'
                />
            </SelectTrigger>
            <SelectContent insets={contentInsets}>
                <ScrollView>
                    <SelectGroup>
                        <SelectLabel>Scenes</SelectLabel>
                        {providers.map(e => {
                            return <SelectItem key={e.name} label={titleCase(e.name)} value={e.name}>
                                {titleCase(e.name)}
                            </SelectItem>
                        })}
                    </SelectGroup>
                </ScrollView>
            </SelectContent>
        </Select>
        <Button
            className='rounded-l-none'
            onPress={() => {
                if (!opt) {
                    console.log("Opt is null")
                    return
                }

                const provider = providers.find(e => e.name === opt.value)
                if (!provider) {
                    Toast.show({
                        type: "error",
                        text1: "Error adding providers",
                        text2: "Scene not found"
                    })
                    return
                }

                const args = provider.properties.reduce((acc, e) => {
                    acc[e.name] = e.default_value
                    return acc
                }, {} as { [key: string]: any })

                setSubConfig(e => {
                    if (!e)
                        return e

                    const id = uuid.v4()
                    const curr = [...e ]
                    curr.push({
                        type: provider.name,
                        arguments: args,
                        uuid: id
                    })

                    console.log("Adding", provider.name)
                    console.log("Total is now", curr.length)

                    return curr
                })
            }}
        >
            <Text>Add new Provider</Text>
        </Button>
    </View>
}