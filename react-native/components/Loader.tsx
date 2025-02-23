import { useTheme } from '@react-navigation/native';
import { ActivityIndicator, ActivityIndicatorProps } from 'react-native';

export default function Loader({inverted, ...props}: ActivityIndicatorProps & { inverted?: boolean }) {
    const theme = useTheme()

    const val = inverted ? !theme.dark : theme.dark
    return <ActivityIndicator color={val ? "#fff" : "#000"} {...props} />
}