import '~/global.css';

import { DarkTheme, DefaultTheme, Theme, ThemeProvider } from '@react-navigation/native';
import { PortalHost } from '@rn-primitives/portal';
import { Stack } from 'expo-router';
import { StatusBar } from 'expo-status-bar';
import * as React from 'react';
import { Platform } from 'react-native';
import Toast from 'react-native-toast-message';
import SaveButton from '~/components/configShare/SaveButton';
import { ThemeToggle } from '~/components/ThemeToggle';
import { setAndroidNavigationBar } from '~/lib/android-navigation-bar';
import { NAV_THEME } from '~/lib/constants';
import { useColorScheme } from '~/lib/useColorScheme';
import { ConfigProvider } from '~/components/configShare/ConfigProvider';

const LIGHT_THEME: Theme = {
  ...DefaultTheme,
  colors: NAV_THEME.light,
};
const DARK_THEME: Theme = {
  ...DarkTheme,
  colors: NAV_THEME.dark,
};

export {
  // Catch any errors thrown by the Layout component.
  ErrorBoundary
} from 'expo-router';

export default function RootLayout() {
  const hasMounted = React.useRef(false);
  const { colorScheme, isDarkColorScheme } = useColorScheme();
  const [isColorSchemeLoaded, setIsColorSchemeLoaded] = React.useState(false);

  useIsomorphicLayoutEffect(() => {
    if (hasMounted.current) {
      return;
    }

    if (Platform.OS === 'web') {
      // Adds the background color to the html element to prevent white background on overscroll.
      document.documentElement.classList.add('bg-background');
    }
    setAndroidNavigationBar(colorScheme);
    setIsColorSchemeLoaded(true);
    hasMounted.current = true;
  }, []);

  if (!isColorSchemeLoaded) {
    return null;
  }

  return (
    <ThemeProvider value={isDarkColorScheme ? DARK_THEME : LIGHT_THEME}>
      <StatusBar style={isDarkColorScheme ? 'light' : 'dark'} />
      <ConfigProvider>
        <Stack>
          <Stack.Screen
            name='index'
            options={{
              title: 'LED Matrix Controller',
              headerRight: () => <ThemeToggle />,
            }}
          />
          <Stack.Screen
            name='modify-preset/[id]'
            getId={({ params }) => `modify-preset-${params?.id}`}
            options={({ route }) => ({
              //@ts-ignore
              title: `Modify ${route.params?.id}`,
              headerRight: () => <SaveButton />,
            })}
          />
          <Stack.Screen
            name='modify-providers/[preset_id]/[scene_id]'
            getId={({ params }) => `modify-providers-${params?.preset_id}-${params?.scene_id}`}
            options={{
              title: `Configure Providers of Scene`,
              headerRight: () => <SaveButton />,
            }}
          />
        </Stack>
      </ConfigProvider>
      <Toast />
      <PortalHost />
    </ThemeProvider>
  );
}

const useIsomorphicLayoutEffect =
  Platform.OS === 'web' && typeof window === 'undefined' ? React.useEffect : React.useLayoutEffect;
