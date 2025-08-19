import { useState } from 'react';
import { Plus } from '~/lib/icons/Plus';
import { FilePlus2 } from '~/lib/icons/FilePlus2';
import { useApiUrl } from '../apiUrl/ApiUrlProvider';
import Loader from '../Loader';
import { AlertDialog, AlertDialogCancel, AlertDialogContent, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle, AlertDialogTrigger } from '../ui/alert-dialog';
import { Button } from '../ui/button';
import { Card, CardContent } from '../ui/card';
import { Input } from '../ui/input';
import { Text } from '../ui/text';
import { View } from 'react-native';
import * as DocumentPicker from 'expo-document-picker';
import * as FileSystem from 'expo-file-system';

export default function AddPresetButton({ presetNames, setRetry }: { presetNames: string[], setRetry: () => void }) {
    const apiUrl = useApiUrl();
    const [presetName, setPreset] = useState<string>("");
    const [adding, setAdding] = useState(false);
    const [open, setOpen] = useState(false);
    const [errorText, setErrorText] = useState<string | null>(null);

    const handleAddPreset = async () => {
        if (presetNames.includes(presetName)) {
            setErrorText("Preset already exists");
            return;
        }

        setAdding(true);
        try {
            const response = await fetch(apiUrl + `/add_preset?id=${encodeURIComponent(presetName)}`, {
                method: "POST",
                body: JSON.stringify({ scenes: [] }),
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            if (!response.ok) {
                const errorData = await response.json().catch(() => ({ error: "Unknown error" }));
                throw new Error(`Failed to add preset: ${errorData.error}`);
            }

            presetNames.push(presetName);
            setPreset("");
            setOpen(false);
            setRetry();
        } catch (e: any) {
            setErrorText(`Error adding preset: ${e.message}`);
        } finally {
            setAdding(false);
        }
    };

    const handleLoadFromFile = async () => {
        if (presetNames.includes(presetName)) {
            setErrorText("Preset already exists");
            return;
        }

        setAdding(true);
        try {
            const result = await DocumentPicker.getDocumentAsync({
                multiple: false,
                type: "application/json"
            });

            if (result.canceled) {
                throw new Error("Cancelled");
            }

            const asset = result.assets[0];
            let content: string;

            if (asset.file) {
                content = await asset.file.text();
            } else {
                content = await FileSystem.readAsStringAsync(asset.uri);
            }

            // Validate JSON
            JSON.parse(content);

            const response = await fetch(apiUrl + `/add_preset?id=${encodeURIComponent(presetName)}`, {
                method: "POST",
                body: content,
                headers: {
                    'Content-Type': 'application/json'
                }
            });

            if (!response.ok) {
                const errorData = await response.json().catch(() => ({ error: "Unknown error" }));
                throw new Error(`Failed to add preset: ${errorData.error}`);
            }

            presetNames.push(presetName);
            setPreset("");
            setOpen(false);
            setRetry();
        } catch (e: any) {
            setErrorText(`Error adding preset: ${e.message}`);
        } finally {
            setAdding(false);
        }
    };

    return (
        <AlertDialog open={open} onOpenChange={setOpen}>
            <AlertDialogTrigger asChild>
                <Card className="w-80 min-h-48 web:h-full border-2 border-dashed border-primary/40 bg-card hover:bg-primary/5 hover:border-primary/60 transition-all duration-200 animate-scale-in shadow-lg">
                    <CardContent className="flex-1 items-center justify-center p-6">
                        <View className="items-center gap-3">
                            <View className="p-4 bg-primary/10 rounded-full border border-primary/20">
                                {adding ? (
                                    <Loader className="w-6 h-6 text-primary" />
                                ) : (
                                    <Plus className="text-primary" width={24} height={24} />
                                )}
                            </View>
                            <Text className="text-lg font-bold text-primary">
                                Add Preset
                            </Text>
                            <Text className="text-sm text-foreground text-center">
                                Create a new LED matrix preset
                            </Text>
                        </View>
                    </CardContent>
                </Card>
            </AlertDialogTrigger>
            <AlertDialogContent className='w-11/12 max-w-lg'>
                <AlertDialogHeader>
                    <AlertDialogTitle className="text-2xl">Add New Preset</AlertDialogTitle>
                </AlertDialogHeader>
                <View className="gap-4 py-4">
                    <Input
                        value={presetName}
                        onChangeText={(text) => {
                            setPreset(text);
                            setErrorText(null);
                        }}
                        className='w-full'
                        placeholder='Enter preset name'
                    />
                    {errorText ? (
                        <Text className='text-destructive text-sm'>{errorText}</Text>
                    ) : null}
                </View>
                <AlertDialogFooter>
                    <View className="flex-row gap-2 w-full">
                        <AlertDialogCancel className="flex-1">
                            <Text>Cancel</Text>
                        </AlertDialogCancel>
                        <Button
                            className="flex-1"
                            onPress={handleAddPreset}
                            disabled={adding || !presetName.trim()}
                        >
                            <View className="flex-row items-center gap-2">
                                {adding ? (
                                    <Loader className="w-4 h-4" />
                                ) : (
                                    <Plus className="text-primary-foreground" width={16} height={16} />
                                )}
                                <Text>Create</Text>
                            </View>
                        </Button>
                        <Button
                            variant="outline"
                            className="flex-1"
                            onPress={handleLoadFromFile}
                            disabled={adding || !presetName.trim()}
                        >
                            <View className="flex-row items-center gap-2">
                                {adding ? (
                                    <Loader className="w-4 h-4" />
                                ) : (
                                    <FilePlus2 className="text-foreground" width={16} height={16} />
                                )}
                                <Text>Import</Text>
                            </View>
                        </Button>
                    </View>
                </AlertDialogFooter>
            </AlertDialogContent>
        </AlertDialog>
    );
}