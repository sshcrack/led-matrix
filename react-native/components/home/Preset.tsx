import { Link } from 'expo-router';
import { useState } from 'react';
import { View } from 'react-native';
import Toast from 'react-native-toast-message';
import { RawPreset as ApiPreset } from '../apiTypes/list_presets';
import { useApiUrl } from '../apiUrl/ApiUrlProvider';
import Loader from '../Loader';
import { Button } from '../ui/button';
import { Card, CardContent, CardDescription, CardHeader } from '../ui/card';
import { Text } from '../ui/text';

export type PresetProps = {
    preset: ApiPreset,
    name: string,
    isActive?: boolean,
    setStatusRefresh: () => void,
    setPresetRefresh: () => void
}

export default function Preset({ preset, name, isActive, setStatusRefresh, setPresetRefresh }: PresetProps) {
    const [isSettingActive, setIsSettingActive] = useState(false);
    const [deleting, setDeleting] = useState(false);
    const apiUrl = useApiUrl()

    return <Card className={`w-[20rem] border-[3px] ${isActive ? "border-green-400" : "border-gray-100"}`}>
        <CardHeader>
            <Text
                numberOfLines={1}
                role='heading'
                aria-level={3}
                className='text-2xl text-card-foreground font-semibold leading-none tracking-tight truncate'
            >
                {name}
            </Text>
            <CardDescription>{preset.scenes.length} Scenes</CardDescription>
        </CardHeader>
        <CardContent className='gap-5'>
            <Button
                disabled={isActive || isSettingActive}
                onPress={() => {
                    if (isSettingActive)
                        return

                    setIsSettingActive(true)
                    fetch(apiUrl + `/set_preset?id=${encodeURIComponent(name)}`)
                        .then(() => setStatusRefresh())
                        .catch(e => {
                            Toast.show({
                                type: "error",
                                text1: "Error setting status",
                                text2: e.message
                            })
                        })
                        .finally(() => setIsSettingActive(false))
                }}
                variant="outline"
                className='flex gap-2 items-center justify-center flex-row'
            >
                {isSettingActive && <Loader />}
                <Text>{isSettingActive ? "Setting active..." : "Set Active"}</Text>
            </Button>

            <View className='flex gap-2 flex-row w-full'>
                <Link push asChild href={{
                    pathname: '/modify-preset/[preset_id]',
                    params: { preset_id: name },
                }}>
                    <Button className="flex-1">
                        <Text>Modify</Text>
                    </Button>
                </Link>

                <Button disabled={deleting || isActive} variant="destructive" onPress={() => {
                    setDeleting(true)
                    fetch(apiUrl + `/preset?id=${encodeURIComponent(name)}`, {
                        method: "DELETE"
                    })
                        .then(() => setPresetRefresh())
                        .catch(e => {
                            Toast.show({
                                type: "error",
                                text1: "Error deleting preset",
                                text2: e.message
                            })
                        })
                        .finally(() => setDeleting(false))
                }}>
                    {deleting ? <Loader /> : <Text>Delete</Text>}
                </Button>
            </View>
        </CardContent>
    </Card>
}