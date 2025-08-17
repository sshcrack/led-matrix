import { useContext, useState } from 'react';
import { Pressable, StyleSheet, View } from 'react-native';
import Animated, { ReduceMotion, useAnimatedStyle, useSharedValue, withDelay, withSequence, withTiming } from 'react-native-reanimated';
import Toast from 'react-native-toast-message';
import { Check } from '~/lib/icons/Check';
import { Save } from '~/lib/icons/Save';
import Loader from '../Loader';
import { ConfigContext } from './ConfigProvider';

export default function SaveButton({ presetId }: { presetId: string }) {
    const { setUpdate, savePreset } = useContext(ConfigContext)
    const [isSaving, setIsSaving] = useState(false)
    const showSaved = useSharedValue(0)


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
            savePreset(presetId)
                .then(() => {
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