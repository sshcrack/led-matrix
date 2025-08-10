import { Link } from 'expo-router';
import { useState } from 'react';
import { Pressable, Share, View } from 'react-native';
import Toast from 'react-native-toast-message';
import { RawPreset as ApiPreset } from '../apiTypes/list_presets';
import { useApiUrl } from '../apiUrl/ApiUrlProvider';
import Loader from '../Loader';
import { Button } from '../ui/button';
import { Card, CardContent, CardDescription, CardHeader } from '../ui/card';
import { StatusIndicator } from '../ui/status-indicator';
import { Text } from '../ui/text';
import { Download } from '~/lib/icons/Download';
import { Pen } from '~/lib/icons/Pen';
import { Trash2 } from '~/lib/icons/Trash2';

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
    const apiUrl = useApiUrl();

    const handleSetActive = async () => {
        if (isSettingActive) return;

        setIsSettingActive(true);
        try {
            await fetch(apiUrl + `/set_active?id=${name}`);
            setStatusRefresh();
        } catch (e: any) {
            Toast.show({
                type: "error",
                text1: "Error activating preset",
                text2: e.message
            });
        } finally {
            setIsSettingActive(false);
        }
    };

    const handleDelete = async () => {
        setDeleting(true);
        try {
            await fetch(apiUrl + `/preset?id=${encodeURIComponent(name)}`, {
                method: "DELETE"
            });
            setPresetRefresh();
        } catch (e: any) {
            Toast.show({
                type: "error",
                text1: "Error deleting preset",
                text2: e.message
            });
        } finally {
            setDeleting(false);
        }
    };

    const handleShare = () => {
        const presetJson = JSON.stringify(preset, null, 2);
        Share.share({
            message: presetJson,
            title: `LED Matrix Preset: ${name}`
        });
    };

    return (
        <Card className={`w-80 min-h-48 animate-scale-in shadow-lg transition-all duration-200 ${isActive
            ? 'border-2 border-primary bg-primary/5 shadow-primary/20'
            : 'border border-border hover:shadow-xl hover:scale-[1.02]'
            }`}>
            <CardHeader className="pb-3">
                <View className="flex-row items-start justify-between">
                    <View className="flex-1">
                        <View className="flex-row items-center gap-2 mb-1">
                            <StatusIndicator
                                status={isActive ? 'active' : 'inactive'}
                                size="sm"
                            />
                            <Text
                                numberOfLines={1}
                                role='heading'
                                aria-level={3}
                                className='text-lg font-bold text-card-foreground truncate'
                            >
                                {name}
                            </Text>
                        </View>
                        <CardDescription className="text-sm">
                            {preset.scenes.length} scene{preset.scenes.length !== 1 ? 's' : ''}
                        </CardDescription>
                    </View>
                    <Pressable
                        onPress={handleShare}
                        className="p-2 hover:bg-secondary/50 rounded-full transition-colors"
                    >
                        <Download className="text-muted-foreground" width={16} height={16} />
                    </Pressable>
                </View>
            </CardHeader>

            <CardContent className="pt-0 gap-3">
                <Button
                    disabled={isActive || isSettingActive}
                    onPress={handleSetActive}
                    className={`w-full ${isActive ? 'bg-success' : ''}`}
                    variant={isActive ? "default" : "outline"}
                >
                    {isSettingActive ? (
                        <View className="flex-row items-center gap-2">
                            <Loader className="w-4 h-4" />
                            <Text>Activating...</Text>
                        </View>
                    ) : (
                        <Text>{isActive ? "Active" : "Activate"}</Text>
                    )}
                </Button>

                <View className="flex-row gap-2">
                    <Link href={{
                        pathname: '/modify-preset/[preset_id]',
                        params: { preset_id: name },
                    }} asChild>
                        <Button variant="outline" className="flex-1">
                            <View className="flex-row items-center gap-2">
                                <Pen className="text-foreground" width={16} height={16} />
                                <Text>Edit</Text>
                            </View>
                        </Button>
                    </Link>

                    <Button
                        variant="destructive"
                        onPress={handleDelete}
                        disabled={deleting || isActive}
                        className="flex-1"
                    >
                        {deleting ? (
                            <View className="flex-row items-center gap-2">
                                <Loader className="w-4 h-4" />
                                <Text>Deleting...</Text>
                            </View>
                        ) : (
                            <View className="flex-row items-center gap-2">
                                <Trash2 className="text-destructive-foreground" width={16} height={16} />
                                <Text>Delete</Text>
                            </View>
                        )}
                    </Button>
                </View>
            </CardContent>
        </Card>
    );
}