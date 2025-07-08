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
import { ApiUrlProvider } from '~/components/apiUrl/ApiUrlProvider';
import ResetApiUrl from '~/components/modify-preset/ResetApiUrl';
import { GestureHandlerRootView } from 'react-native-gesture-handler';

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
      <GestureHandlerRootView>
        <StatusBar style={isDarkColorScheme ? 'light' : 'dark'} />
        <ApiUrlProvider>
          <ConfigProvider>
            <Stack>
              <Stack.Screen
                name='index'
                options={{
                  title: 'LED Matrix',
                  headerLargeTitle: true,
                  headerStyle: {
                    backgroundColor: isDarkColorScheme ? 'hsl(2, 6%, 23%)' : 'hsl(248, 250%, 252%)',
                  },
                  headerTitleStyle: {
                    fontWeight: 'bold',
                    fontSize: 28,
                    color: isDarkColorScheme ? 'hsl(226, 232%, 240%)' : 'hsl(15, 23%, 42%)',
                  },
                  headerLeft: () => (Platform.OS !== "web" || __DEV__) && <ResetApiUrl />,
                  headerRight: () => <ThemeToggle />,
                }}
              />
              <Stack.Screen
                name='schedules'
                options={{
                  title: 'Schedules',
                  headerLargeTitle: false,
                  headerStyle: {
                    backgroundColor: isDarkColorScheme ? 'hsl(2, 6%, 23%)' : 'hsl(248, 250%, 252%)',
                  },
                  headerTitleStyle: {
                    fontWeight: 'bold',
                    fontSize: 20,
                    color: isDarkColorScheme ? 'hsl(226, 232%, 240%)' : 'hsl(15, 23%, 42%)',
                  },
                  headerRight: () => <ThemeToggle />,
                }}
              />
              <Stack.Screen
                name='modify-preset/[preset_id]'
                getId={({ params }) => `modify-preset-${params?.preset_id}`}
                options={({ route }) => ({
                  //@ts-ignore
                  title: `Edit ${route.params?.preset_id}`,
                  headerStyle: {
                    backgroundColor: isDarkColorScheme ? 'hsl(2, 6%, 23%)' : 'hsl(248, 250%, 252%)',
                  },
                  headerTitleStyle: {
                    fontWeight: 'bold',
                    fontSize: 18,
                    color: isDarkColorScheme ? 'hsl(226, 232%, 240%)' : 'hsl(15, 23%, 42%)',
                  },
                  //@ts-ignore
                  headerRight: () => <SaveButton presetId={route.params?.preset_id} />,
                })}
              />
              <Stack.Screen
                name='modify-providers/[preset_id]/[scene_id]'
                getId={({ params }) => `modify-providers-${params?.preset_id}-${params?.scene_id}`}
                options={{
                  title: `Configure Scene`,
                  headerStyle: {
                    backgroundColor: isDarkColorScheme ? 'hsl(2, 6%, 23%)' : 'hsl(248, 250%, 252%)',
                  },
                  headerTitleStyle: {
                    fontWeight: 'bold',
                    fontSize: 18,
                    color: isDarkColorScheme ? 'hsl(226, 232%, 240%)' : 'hsl(15, 23%, 42%)',
                  },
                }}
              />
            </Stack>
          </ConfigProvider>
        </ApiUrlProvider>
        <PortalHost />
        <Toast />
      </GestureHandlerRootView>
    </ThemeProvider>
  );
}

const useIsomorphicLayoutEffect =
  Platform.OS === 'web' && typeof window === 'undefined' ? React.useEffect : React.useLayoutEffect;
