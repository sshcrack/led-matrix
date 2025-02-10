import { Link } from 'expo-router';
import { Pen } from "~/lib/icons/Pen";
import { Preset as ApiPreset } from '../apiTypes/list_presets';
import { Card, CardContent, CardDescription, CardFooter, CardHeader, CardTitle } from '../ui/card';
import { Text } from '../ui/text';
import { View } from '@rn-primitives/slot';
import { Button } from '../ui/button';

export default function Preset({ preset, name, isActive }: { preset: ApiPreset, name: string, isActive?: boolean }) {

    return <Card className={`max-w-sm ${isActive ? "border-[3px] border-green-400" : ""}`}>
        <CardHeader>
            <Text
                role='heading'
                aria-level={3}
                className='text-2xl text-card-foreground font-semibold leading-none tracking-tight'
            >
                {name}
            </Text>
            <CardDescription>{preset.scenes.length} Scenes</CardDescription>
        </CardHeader>
        <CardContent className='gap-5'>
            <Link push asChild href={{
                pathname: '/modify-preset/[id]',
                params: { id: name },
            }}>
                <Button>
                    <Text>Modify</Text>
                </Button>
            </Link>

            <Button variant="destructive">
                <Text>Delete</Text>
            </Button>
        </CardContent>
    </Card>
}