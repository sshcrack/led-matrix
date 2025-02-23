import { useContext, useState } from 'react';
import { ActivityIndicator, Pressable, StyleSheet, View } from 'react-native';
import Animated, { ReduceMotion, useAnimatedStyle, useSharedValue, withDelay, withSequence, withTiming } from 'react-native-reanimated';
import Toast from 'react-native-toast-message';
import { Check } from '~/lib/icons/Check';
import { Save } from '~/lib/icons/Save';
import { objectToArrayPresets } from '../apiTypes/list_presets';
import { ConfigContext } from './ConfigProvider';
import { useApiUrl } from '../apiUrl/ApiUrlProvider';
import Loader from '../Loader';

export default function SaveButton({ presetId }: { presetId: string }) {
    const { config, setUpdate } = useContext(ConfigContext)
    const preset = config.get(presetId)
    const [isSaving, setIsSaving] = useState(false)
    const showSaved = useSharedValue(0)
    const apiUrl = useApiUrl()


    const saveButtonStyle = useAnimatedStyle(() => {
        return {
            opacity: 1 - showSaved.value
        }
    })

    const checkmarkStyle = useAnimatedStyle(() => {
        return {
            opacity: showSaved.value
        }
    })

    return <Pressable
        className='p-7 relative items-center justify-center'
        disabled={isSaving}
        onPress={() => {
            setIsSaving(true)
            const raw = objectToArrayPresets(preset!)
            fetch(apiUrl + `/preset?id=${encodeURIComponent(presetId)}`, {
                method: "POST",
                body: JSON.stringify(raw),
                headers: {
                    'Content-Type': 'application/json'
                }
            })
                .then(() => {
                    console.log("Successfully saved preset")
                    showSaved.value = withSequence(ReduceMotion.Never, withTiming(1, {
                        duration: 0
                    }), withDelay(1000, withTiming(0, {
                        duration: 100
                    })))
                })
                .catch(e => Toast.show({
                    type: "success",
                    text1: "Error saving preset",
                    text2: e.message ?? JSON.stringify(e)
                }))
                .finally(() => {
                    setIsSaving(false)
                    setUpdate(Math.random())
                })
        }}
    >
        {isSaving ? <View style={[styles.stackOnTop]}>
            <Loader size="small" />
        </View> : (
            <>
                <Animated.View style={[styles.stackOnTop, saveButtonStyle]}>
                    <Save className='text-foreground' size={30} strokeWidth={1.25} />
                </Animated.View>
                <Animated.View style={[styles.stackOnTop, checkmarkStyle]}>
                    <Check className='text-foreground' size={30} strokeWidth={1.25} />
                </Animated.View>
            </>
        )}
    </Pressable>
}


const styles = StyleSheet.create({
    stackOnTop: {
        position: "absolute",
        top: 0, left: 0, right: 0, bottom: 0, justifyContent: 'center', alignItems: 'center'
    }
});