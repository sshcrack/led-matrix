import { useEffect } from 'react';
import Animated, { Easing, useAnimatedStyle, useSharedValue, withRepeat, withTiming } from 'react-native-reanimated';
import { LoaderCircle } from '~/lib/icons/LoaderCircle';

export default function Loader() {
    const sv = useSharedValue<number>(0);

    useEffect(() => {
        sv.value = withRepeat(withTiming(360, {
            easing: Easing.linear,
            duration: 1000
        }), -1);
    }, []);


    const animatedDefault = useAnimatedStyle(() => ({
        transform: [{ rotate: `${sv.value}deg` }]
    }));

    return <Animated.View style={[animatedDefault]}>
        <LoaderCircle
            className='text-foreground'
        />
    </Animated.View>;
}