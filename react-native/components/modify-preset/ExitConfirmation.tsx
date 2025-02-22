import { useNavigation } from 'expo-router';
import { useContext, useEffect, useRef, useState } from 'react';
import { arrayToObjectPresets, Preset, RawPreset } from '../apiTypes/list_presets';
import { ConfigContext } from '../configShare/ConfigProvider';
import { AlertDialog, AlertDialogAction, AlertDialogCancel, AlertDialogContent, AlertDialogDescription, AlertDialogFooter, AlertDialogHeader, AlertDialogTitle } from '../ui/alert-dialog';
import { Text } from '../ui/text';
import usePresetId from './PresetIdProvider';
import { NavigationAction } from '@react-navigation/native';

export default function ExitConfirmation({ data }: { data: RawPreset | null }) {
    // Navigation
    const navigation = useNavigation();
    const presetId = usePresetId()
    const { config } = useContext(ConfigContext)

    const [open, setOpen] = useState(false);

    const ref = useRef<Preset | undefined>(undefined)
    const dataRef = useRef<Preset | undefined>(undefined)
    const actionRef = useRef<NavigationAction | null>(null)

    useEffect(() => {
        ref.current = config.get(presetId)
    }, [config])
    useEffect(() => {
        if (!data) {
            dataRef.current = undefined
            return
        }

        dataRef.current = arrayToObjectPresets(data)
    }, [data])


    // Effect
    useEffect(() => {
        navigation.addListener('beforeRemove', e => {
            const hasDiff = JSON.stringify(ref.current) !== JSON.stringify(dataRef.current)

            if (!hasDiff || ref.current === undefined || dataRef.current === undefined) {
                return;
            }

            e.preventDefault();

            actionRef.current = e.data.action
            setOpen(true)
        });
    }, []);
    return <AlertDialog onOpenChange={setOpen} open={open}>
        <AlertDialogContent>
            <AlertDialogHeader>
                <AlertDialogTitle>Do you really want to exit?</AlertDialogTitle>
                <AlertDialogDescription>
                    There are unsaved changes. Do you want to exit without saving?
                </AlertDialogDescription>
            </AlertDialogHeader>
            <AlertDialogFooter>
                <AlertDialogCancel onPress={() => setOpen(false)}>
                    <Text>Cancel</Text>
                </AlertDialogCancel>
                <AlertDialogAction onPress={() => {
                    if (actionRef.current)
                        navigation.dispatch(actionRef.current)
                }}>
                    <Text>Continue</Text>
                </AlertDialogAction>
            </AlertDialogFooter>
        </AlertDialogContent>
    </AlertDialog>
}