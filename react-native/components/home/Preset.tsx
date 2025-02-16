import { Link } from 'expo-router';
import { Preset as ApiPreset } from '../apiTypes/list_presets';
import { Button } from '../ui/button';
import { Card, CardContent, CardDescription, CardHeader } from '../ui/card';
import { Text } from '../ui/text';
import { View } from 'react-native';

export default function Preset({ preset, name, isActive }: { preset: ApiPreset, name: string, isActive?: boolean }) {

    return <Card className={`w-[20rem] ${isActive ? "border-[3px] border-green-400" : ""}`}>
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
            <Button disabled={isActive} variant="outline">
                <Text>Set Active</Text>
            </Button>

            <View className='flex gap-2 flex-row w-full'>
                <Link push asChild href={{
                    pathname: '/modify-preset/[id]',
                    params: { id: name },
                }}>
                    <Button className="flex-1">
                        <Text>Modify</Text>
                    </Button>
                </Link>

                <Button variant="destructive">
                    <Text>Delete</Text>
                </Button>
            </View>
        </CardContent>
    </Card>
}