import { Pressable, View } from 'react-native';
import { setAndroidNavigationBar } from '~/lib/android-navigation-bar';
import { MoonStar } from '~/lib/icons/MoonStar';
import { Sun } from '~/lib/icons/Sun';
import { useColorScheme } from '~/lib/useColorScheme';
import { cn } from '~/lib/utils';

export function ThemeToggle() {
  const { isDarkColorScheme, setColorScheme } = useColorScheme();

  function toggleColorScheme() {
    const newTheme = isDarkColorScheme ? 'light' : 'dark';
    setColorScheme(newTheme);
    setAndroidNavigationBar(newTheme);
  }

  return (
    <Pressable
      onPress={toggleColorScheme}
      className='web:ring-offset-background web:transition-colors web:focus-visible:outline-none web:focus-visible:ring-2 web:focus-visible:ring-ring web:focus-visible:ring-offset-2'
    >
      {({ pressed }) => (
        <View
          className={cn(
            'flex-1 aspect-square pt-0.5 justify-center items-center web:px-5 p-2 rounded-full transition-colors',
            pressed && 'opacity-70 bg-secondary/50',
            'hover:bg-secondary/30'
          )}
        >
          {isDarkColorScheme ? (
            <MoonStar className='text-foreground' size={20} strokeWidth={1.5} />
          ) : (
            <Sun className='text-foreground' size={20} strokeWidth={1.5} />
          )}
        </View>
      )}
    </Pressable>
  );
}
